CC = gcc
CFLAGS = -Wall -O3 -lpthread -g -ggdb -I/usr/include/mysql
LIBS = -L/usr/local/lib -lsctp -L/usr/lib/mysql -lmysqlclient  -I/usr/include/libxml2  -lxml2

CFILES = arguments.c network.c agent.c

all: 
	$(CC) $(CFLAGS) -o agent  $(CFILES) $(LIBS)

clean:
	rm -fr agent client server *.o

