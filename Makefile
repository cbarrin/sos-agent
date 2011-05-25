CC = gcc
CFLAGS = -Wall -g 
all: client server

client: client.o
	$(CC) $(CFLAGS) -o $@ client.c -L/usr/local/lib -lsctp
server: server.o
	$(CC) $(CFLAGS) -o $@ server.c -L/usr/local/lib -lsctp

clean:
	rm -fr client server *.o
