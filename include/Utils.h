#pragma once

#include <chrono>
#include <cstdint>

using OrderId = uint64_t;
using Price = double;
using Quantity = uint64_t;
using Timestamp = std::chrono::system_clock::time_point;

template <class... Ts> struct overload : Ts... {
  using Ts::operator()...;
};

template <class... Ts> overload(Ts...) -> overload<Ts...>;
