#pragma once

// #include <chrono>
#include <cstddef>
#include <cstdint>

using OrderId = uint64_t;
using Quantity = uint64_t;
using Price = uint32_t;
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

// #ifdef __cpp_lib_hardware_interference_size
// static constexpr size_t CACHE_LINE_SIZE =
//     std::hardware_destructive_interference_size;
// #else
// static constexpr size_t CACHE_LINE_SIZE = 64;
// #endif

static constexpr size_t CACHE_LINE_SIZE = 64;
