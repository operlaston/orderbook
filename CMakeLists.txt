cmake_minimum_required(VERSION 3.16)

project(orderbook LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to a Debug build if none was chosen. Use -DCMAKE_BUILD_TYPE=Release
# for benchmarks and for running the server for real.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

option(ORDERBOOK_SANITIZE "Build with AddressSanitizer and UBSan" OFF)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Flags and include paths shared by every target.
add_library(project_options INTERFACE)
target_include_directories(project_options INTERFACE include)
target_compile_options(project_options INTERFACE -Wall -Wextra)
target_link_libraries(project_options INTERFACE Threads::Threads)
if(ORDERBOOK_SANITIZE)
  target_compile_options(project_options INTERFACE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(project_options INTERFACE -fsanitize=address,undefined)
endif()

# Matching engine, shared by the server, tests and benchmark.
add_library(engine STATIC
  src/Orderbook.cpp
  src/Engine.cpp
)
target_link_libraries(engine PUBLIC project_options)

# TCP server + engine
add_executable(orderbook
  src/main.cpp
  src/Server.cpp
)
target_link_libraries(orderbook PRIVATE engine)

# Interactive command-line client
add_executable(client src/client/Client.cpp)
target_link_libraries(client PRIVATE project_options)

add_executable(tests
  tests/EngineTest.cpp
  tests/OrderbookFuzzTest.cpp
  tests/OrderbookTest.cpp
  tests/ProtocolTest.cpp
  tests/SpscQueueTest.cpp
)
target_link_libraries(tests PRIVATE engine GTest::gtest GTest::gtest_main)

add_executable(bench bench/benchmark.cpp)
target_link_libraries(bench PRIVATE engine)
