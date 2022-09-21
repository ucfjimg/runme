#!/bin/sh

echo Compiling
make

echo
echo Testing: Simple hello world
./runme hello

echo
echo Testing: Redirecting stdin to test input
./runme add add.in

echo
echo "Testng: Infinite loop (time limit exceeded)"
./runme tle

echo
echo "Testing: Segfault (run time error)"
./runme segfault

echo
echo Testing: Non zero exit code
./runme nonzeroexit


