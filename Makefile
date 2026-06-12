CC=g++
CCFLAGS=-std=c++20 -Wall -Wextra -Wno-interference-size -g
INCLUDES=-I./include

all: build/orderbook build/client

build/orderbook: build/orderbook.o build/server.o build/main.o $(wildcard include/*.h)
	$(CC) $(CCFLAGS) $(INCLUDES) -o build/orderbook build/orderbook.o build/server.o build/main.o

build/orderbook.o: src/Orderbook.cpp $(wildcard include/*.h)
	$(CC) $(CCFLAGS) $(INCLUDES) -c src/Orderbook.cpp -o build/orderbook.o

build/server.o: src/Server.cpp $(wildcard include/*.h)
	$(CC) $(CCFLAGS) $(INCLUDES) -c src/Server.cpp -o build/server.o

build/main.o: src/main.cpp $(wildcard include/*.h)
	$(CC) $(CCFLAGS) $(INCLUDES) -c src/main.cpp -o build/main.o

build/client: src/Client.cpp $(wildcard include/*.h)
	$(CC) $(CCFLAGS) $(INCLUDES) -o build/client src/Client.cpp

clean:
	rm -rf build/*
