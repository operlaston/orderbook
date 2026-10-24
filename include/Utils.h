#pragma once

// #include <chrono>
#include <cstdint>

using OrderId = uint64_t;
using Price = uint32_t;
using Quantity = uint64_t;
using Timestamp = uint64_t;

template <class... Ts> struct overload : Ts... {
  using Ts::operator()...;
};

template <class... Ts> overload(Ts...) -> overload<Ts...>;

#ifdef DEBUG_BUILD
inline constexpr bool kDebug = true;
#else
inline constexpr bool kDebug = false;
#endif

enum class Side : uint8_t {
  Bid = 0,
  Ask = 1
};
