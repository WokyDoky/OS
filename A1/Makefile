# C Compiler
CC = gcc

CFLAGS = -Wall -O3

.PHONY: all clean

# The 'all' target is the default target that will be run when you just type 'make'.
# It depends on 'head' and 'tail', so make will build them.
all: head tail findlocation

# Rule to build the 'head' executable from 'head.c'
head: head.c
	$(CC) $(CFLAGS) -o head head.c

# Rule to build the 'tail' executable from 'tail.c'
tail: tail.c
	$(CC) $(CFLAGS) -o tail tail.c

findlocation: findlocation.c
	$(CC) $(CFLAGS) -o findlocation findlocation.c	

# The 'clean' target is used to remove the compiled files.
# You can run it with 'make clean'.
clean:
	rm -f head tail findlocation