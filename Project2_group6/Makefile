CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c11 -pthread
LDFLAGS=-pthread

OBJS=main.o sync.o

all: Project2

Project2: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

main.o: main.c sync.h
	$(CC) $(CFLAGS) -c main.c

sync.o: sync.c sync.h
	$(CC) $(CFLAGS) -c sync.c

test: Project2
	bash ./tests.sh

clean:
	rm -f Project2 *.o
