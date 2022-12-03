CC=gcc
CFLAGS=-g -O2
OBJECTS=client.o server.o raw.o hashmap.o linkedlist.o
EXECS=client server
FILES=client.c server.c duckchat.h hashmap.c hashmap.h linkedlist.c linkedlist.h Makefile raw.c raw.h

all: $(EXECS)

client: client.o raw.o
	$(CC) $(CFLAGS) client.o raw.o -o client

server: server.o hashmap.o linkedlist.o
	$(CC) $(CFLAGS) server.o hashmap.o linkedlist.o -o server

clean:
	rm -f $(OBJECTS) $(EXECS)

client.o: client.c duckchat.h raw.h
hashmap.o: hashmap.c hashmap.h
linkedlist.o: linkedlist.c linkedlist.h
raw.o: raw.c raw.h
server.o: server.c duckchat.h hashmap.h linkedlist.h