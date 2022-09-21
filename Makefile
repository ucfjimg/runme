all: runme hello add tle segfault nonzeroexit

runme:	runme.c
hello:	hello.c
add: add.c
tle: tle.c
segfault: segfault.c
nonzeroexit: nonzeroexit.c

clean:
	rm runme hello add tle segfault nonzeroexit

