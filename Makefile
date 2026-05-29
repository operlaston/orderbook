CC=g++
CCFLAGS=-std=c++20 -Wall -Wextra
INCLUDES=-I./include

orderbook: src/main.cpp src/Orderbook.cpp $(wildcard include/*.h)
	$(CC) $(CCFLAGS) $(INCLUDES) -o orderbook src/main.cpp src/Orderbook.cpp

clean:
	rm -rf orderbook
