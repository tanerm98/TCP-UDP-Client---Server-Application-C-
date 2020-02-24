# Protocoale de comunicatii:
# Laborator 8: Multiplexare
# Makefile

all: server subscriber

# Compileaza server.c
server: server.cpp
	g++ -Wall -std=c++11 server.cpp -o server

# Compileaza subscriber.c
subscriber: subscriber.cpp
	g++ -Wall -std=c++11 subscriber.cpp -o subscriber

clean:
	rm -f server subscriber
