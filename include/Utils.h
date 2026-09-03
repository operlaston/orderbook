#pragma once

// #include <chrono>
#include <cstdint>

using OrderId = uint64_t;
using Price = double;
using Quantity = uint64_t;
using Timestamp = uint64_t;

template <class... Ts> struct overload : Ts... {
  using Ts::operator()...;
};

template <class... Ts> overload(Ts...) -> overload<Ts...>;
