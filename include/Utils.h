#pragma once

// #include <chrono>
#include <cstddef>
#include <cstdint>

using OrderId = uint64_t;
using Quantity = uint64_t;
using Price = uint32_t;

enum class Status: uint8_t {
  Success = 0,
  BadRequest = 1,
  ServerError = 2
};

enum class MessageType: uint8_t {
  NewOrder = 0,
  CancelOrder = 1,
  ModifyOrder = 2
};

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
static constexpr uint32_t SPSC_QUEUE_SIZE = 1 << 17; // 131072
