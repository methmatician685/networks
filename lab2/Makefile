CC = g++
CFLAGS = -Wall

all: server client

server: server.cpp protocol.h
	$(CC) $(CFLAGS) -o server server.cpp

client: client.cpp protocol.h
	$(CC) $(CFLAGS) -o client client.cpp

clean:
	rm -f server client
