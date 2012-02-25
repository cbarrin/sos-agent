CC = gcc
CC = clang
CFLAGS = -Wall  -lpthread -g -ggdb  -O3 -lm
LIBS = -L/usr/local/lib -I/usr/include/libxml2  -lxml2  -I/usr/local/include  -pthread -L/usr/local/lib -lz -lpthread   -luuid

HASHDIR=./uthash
CFLAGS +=-I$(HASHDIR)


CFILES = arguments.c network.c agent.c poll.c controller.c discovery.c 

all: 
	$(CC) $(CFLAGS) -o agent  $(CFILES) $(LIBS)


clean:
	rm -fr agent client server *.o 

