CC = gcc
CFLAGS = -Wall -g -O3 -lpthread # -Wextra -Werror
LIBS = -L/usr/local/lib -lsctp

all: 
	$(CC) $(CFLAGS) -o client client.c  $(LIBS) 
	$(CC) $(CFLAGS) -o server server.c  $(LIBS)

clean:
	rm -fr agent client server *.o
