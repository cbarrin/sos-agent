CC = gcc
CFLAGS = -Wall -g  -lpthread # -Wextra -Werror

all: 
	$(CC) $(CFLAGS) -o client client.c list.c  -L/usr/local/lib -lsctp
	$(CC) $(CFLAGS) -o server server.c list.c tcp_thread.c -L/usr/local/lib -lsctp

clean:
	rm -fr client server *.o
