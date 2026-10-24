#pragma once

// #include <chrono>
#include <cstdint>
#include <Utils.h>
#include <variant>

using OrderId = uint64_t;
using Price = double;
using Quantity = uint64_t;
using Timestamp = uint64_t;

template <class... Ts> struct overload : Ts... {
  using Ts::operator()...;
};

template <class... Ts> overload(Ts...) -> overload<Ts...>;

enum class Side { BUY = 0, SELL = 1 };

enum class TimeInForce {
  NONE = 0,
  GOOD_TILL_CANCEL = 1,
  IMMEDIATE_OR_CANCEL = 2,
  FILL_OR_KILL = 3,
};

enum class OrderType { LIMIT = 0, MARKET = 1 };

namespace Request {
struct NewOrder {
  int sessionId;
  Side side;
  OrderType orderType;
  TimeInForce timeInForce;
  Price price;
  Quantity quantity;
};

struct CancelOrder {
  int sessionId;
  OrderId orderId;
};

struct ModifyOrder {
  int sessionId;
  OrderId orderId;
  Quantity newQuantity;
};

} // namespace Request

using ClientRequest =
    std::variant<Request::NewOrder, Request::CancelOrder, Request::ModifyOrder>;


namespace Response {
struct NewOrder {
  int sessionId; // file descriptor number
  uint8_t status;
  OrderId newOrderId;
};
struct CancelOrder {
  int sessionId; // file descriptor number
  uint8_t status;
};
struct ModifyOrder {
  int sessionId; // file descriptor number
  uint8_t status;
};
} // namespace Response

using ServerResponse = std::variant<Response::NewOrder, Response::CancelOrder,
                                    Response::ModifyOrder>;


enum class MessageType { NEW_ORDER = 0, CANCEL_ORDER = 1, MODIFY_ORDER = 2 };

namespace ResponseStatus {
const uint8_t SUCCESS = 0;
const uint8_t BAD_REQUEST = 1;
const uint8_t INVALID_MESSAGE_TYPE = 2;
const uint8_t SERVER_ERROR = 3;
const uint8_t PARTIAL_FILL = 4;
const uint8_t CANT_FILL = 5;
} // namespace ResponseStatus

namespace GlobalLengths {
const uint8_t NEW_ORDER_MESSAGE = 19;
const uint8_t CANCEL_ORDER_MESSAGE = 8;
const uint8_t MODIFY_ORDER_MESSAGE = 16;
const uint32_t SPSC_QUEUE = 65536;
} // namespace GlobalLengths
