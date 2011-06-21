CC = gcc
CFLAGS = -Wall -g -O3 -lpthread # -Wextra -Werror
LIBS = -L/usr/local/lib -lsctp
CFILES = arguments.c network.c discovery.c agent.c controller.c

all: 
#	$(CC) $(CFLAGS) -o client client.c $(CFILES) $(LIBS) 
#	$(CC) $(CFLAGS) -o server server.c $(CFILES) $(LIBS)
	$(CC) $(CFLAGS) -o agent  $(CFILES) $(LIBS)

clean:
	rm -fr agent client server *.o
