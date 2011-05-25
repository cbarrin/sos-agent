CC = gcc
CFLAGS = -Wall -g 

all: 
	$(CC) $(CFLAGS) -o client client.c -L/usr/local/lib -lsctp
	$(CC) $(CFLAGS) -o server server.c -L/usr/local/lib -lsctp

clean:
	rm -fr client server *.o
