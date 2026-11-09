CC = gcc
CFLAGS = -Wall -Wextra -g

.PHONY: all clean test

all: minishell

minishell: main.o parser.o executor.o
	$(CC) $(CFLAGS) -o minishell main.o parser.o executor.o

main.o: main.c parser.h executor.h
	$(CC) $(CFLAGS) -c main.c

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c

executor.o: executor.c executor.h
	$(CC) $(CFLAGS) -c executor.c

clean:
	rm -f *.o minishell

test:
	python3 tests/test_shell.py
