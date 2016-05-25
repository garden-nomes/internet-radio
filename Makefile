CC=gcc
COPTS=-g

all: server client

server: server.c
	$(CC) $(COPTS) server.c serverlib.c -o server

client: client.c
	$(CC) $(COPTS) client.c -o client -lpulse -lpulse-simple

clean:
	rm -rf *.o *.dSYM server client