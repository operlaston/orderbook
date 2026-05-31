CC=g++
CCFLAGS=-std=c++20 -Wall -Wextra -g
INCLUDES=-I./include

orderbook: src/main.cpp src/Orderbook.cpp $(wildcard include/*.h)
	$(CC) $(CCFLAGS) $(INCLUDES) -o orderbook src/main.cpp src/Orderbook.cpp src/Server.cpp

clean:
	rm -rf orderbook
