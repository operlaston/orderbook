CC=g++
CCFLAGS=-std=c++20 -Wall -Wextra -O2
INCLUDES=-I./include

orderbook:
	$(CC) $(CCFLAGS) $(INCLUDES) -o orderbook src/main.cpp src/Orderbook.cpp
