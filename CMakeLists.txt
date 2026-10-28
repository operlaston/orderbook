# Minimum CMake version required to understand this file
cmake_minimum_required(VERSION 3.16)

# Project name and the languages it uses
project(orderbook LANGUAGES CXX)

# Use C++17
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to a Debug build (includes debug symbols) if none was chosen
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Find system libraries
find_package(GTest REQUIRED)

# The program to build and the source files that go into it
add_executable(tests
  src/Orderbook.cpp
  tests/OrderbookTest.cpp
  tests/OrderbookFuzzTest.cpp
  tests/ProtocolTest.cpp
  tests/SpscQueueTest.cpp
)

add_executable(bench
  src/Orderbook.cpp
  bench/benchmark.cpp
)

# Where to look for #include <...> headers (glad/, KHR/)
target_include_directories(tests PRIVATE include)
target_include_directories(bench PRIVATE include)

# Libraries to link against
target_link_libraries(tests PRIVATE GTest::gtest GTest::gtest_main pthread ${CMAKE_DL_LIBS})
