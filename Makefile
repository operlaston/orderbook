CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wno-interference-size -O2 -g -MMD -MP -pthread
LDFLAGS := -pthread
INCLUDES := -I include
BUILD := build

SERVER_SRCS := src/Orderbook.cpp src/Server.cpp src/main.cpp
SERVER_OBJS := $(SERVER_SRCS:src/%.cpp=$(BUILD)/%.o)

.PHONY: all clean

all: $(BUILD)/orderbook $(BUILD)/client

$(BUILD)/orderbook: $(SERVER_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD)/client: src/client/Client.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -r $(BUILD)

-include $(SERVER_OBJS:.o=.d) $(BUILD)/Client.d
