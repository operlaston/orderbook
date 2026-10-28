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

// Values match the wire protocol (0 = BUY, 1 = SELL).
enum class Side : uint8_t {
  Bid = 0,
  Ask = 1
};

// Values match the wire protocol status byte.
enum class Status : uint8_t {
  Success = 0,
  BadRequest = 1,
  ServerError = 2
};

// Result of a new order. orderId is 0 unless (part of) the order is resting in
// the book: fully filled and rejected orders don't get an id.
struct NewOrderResult {
  Status status;
  OrderId orderId;
};

// #ifdef __cpp_lib_hardware_interference_size
// static constexpr size_t CACHE_LINE_SIZE =
//     std::hardware_destructive_interference_size;
// #else
// static constexpr size_t CACHE_LINE_SIZE = 64;
// #endif

static constexpr size_t CACHE_LINE_SIZE = 64;
