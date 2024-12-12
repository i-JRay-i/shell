CC = gcc
CFLAGS = -g3 -Wall

shell: main.c
	$(CC) $(CFLAGS) -o shell main.c
