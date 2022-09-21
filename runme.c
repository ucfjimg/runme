#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    if (argc != 2 && argc != 3) {
        printf("runme: executable [stdin]\n");
        return 1;
    }

    const char *pgm = argv[1];
    const char *redir = argc == 3 ? argv[2] : NULL;

    // If we're given a file to redirect to the child stdin, try
    // to open it.
    //
    int fd = -1;
    if (redir) {
        fd = open(redir, O_RDONLY);
        if (fd == -1) {
            perror("opening redir file");
            return 1;
        }
    }

    // Open a pipe to collect stdout from the child program.
    //
    int pipes[2];
    pipe(pipes);
    int fdread = pipes[0];
    int fdwrite = pipes[1];
    
    // Make the read end of the pipe non-blocking so we can 
    // read and still notice the child ending
    //
    int mask = fcntl(fdread, F_GETFL);
    if (mask == -1) {
        perror("get pipe fd flags");
        return 1;
    }

    if (fcntl(fdread, F_SETFL, mask | O_NONBLOCK) == -1) {
        perror("set pipe fd flags");
        return 1;
    }
    
    // fork and create the child process
    //
    pid_t pid = fork();
    if (pid == 0) {
        // this is the child process
        //
        close(0);       // closing stdin
        if (fd != -1) {
            dup(fd);    // and reopen it from fd if there is one
        }

        close(1);       // closing stdout
        dup(fdwrite);   // send to pipe
        close(fdwrite); // and close original handle

        // let the child run for 2 seconds
        // 
        alarm(2);

        // execl() will not return unless there is an error
        // starting the program
        if (execl(pgm, pgm, NULL) == -1) {
            perror("could no run child program!");
            exit(1);
        }
    }

    int wstatus;
    int running = 1;
    size_t bufsize = 512;
    size_t inbuf = 0;
    char *buffer = malloc(bufsize);

    while (1) {
        // is the process still running? if not, save off the exit status.
        // don't exit the loop, though; there may still be data to read
        // buffered in the pipe.
        // 
        if (running && waitpid(pid, &wstatus, WNOHANG) == pid) {
            running = 0;
        }
        
        // non-blocking: try to get some data from the child
        //
        ssize_t got = read(fdread, buffer + inbuf, bufsize - inbuf);
        if (got == -1) {
            if (errno == EAGAIN) {
                // EAGAIN means there was nothing to read
                //
                if (!running) {
                    // process is gone, and we've collected all the 
                    // output. we're done.
                    //
                    break;
                }

                // sleep for 50 ms (i.e. don't hog the CPU), unless we filled
                // the buffer! then it's likely we just ended the read before we
                // got all the data.
                //
                if (inbuf < bufsize) {
                    usleep(50 * 1000);
                }
                continue;
            }

            // the read failed for some real error
            //
            perror("read");
            return 1;
        }
    
        // note the data we read, and grow the buffer if it's full
        //
        inbuf += got;
        if (inbuf == bufsize) {
            buffer = realloc(buffer, 2 * bufsize);
            if (buffer == NULL) {
                perror("reallocate read buffer");
                return 1;
            }
            bufsize *= 2;
        }
    }

    // note that we never exit the above loop with the buffer entirely
    // full, so there's always room for the nul terminator.
    //
    buffer[inbuf++] = '\0';

    // print all the lines of output
    //
    printf("--- begin child output\n");
    size_t i = 0;
    while (i < inbuf) {
        char *p = strchr(buffer + i, '\n');
        if (p == NULL) {
            printf("** %s\n", buffer+i);
            break;
        }
    
        *p++ = '\0';
        printf("** %s\n", buffer+i);
        i = p - buffer + 1;
    }
    printf("--- end child output\n");

    // check on the exit status and see what happened
    //
    if (WIFSIGNALED(wstatus)) {
        // child exited via some signal
        // 
        switch (WTERMSIG(wstatus)) {
            case SIGALRM:
                printf("TLE\n");
                break;

            case SIGABRT:       // assert and such
            case SIGSEGV:       // segfault
            case SIGBUS:        // bus error
                printf("RTE\n");
                break;

            default:
                printf("exited with some other signal\n");

        }
    } else {
        printf("exit status %d\n", WEXITSTATUS(wstatus));
    }
}
