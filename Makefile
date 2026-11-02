CC = gcc
CFLAGS = -Wall -Wextra -g

all: minishell

minishell: main.o parser.o
	$(CC) $(CFLAGS) -o minishell main.o parser.o

main.o: main.c parser.h
	$(CC) $(CFLAGS) -c main.c

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c parser.c

clean:
	rm -f *.o minishell