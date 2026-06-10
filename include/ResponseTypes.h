#pragma once

#include "Using.h"
#include <cstdint>
#include <variant>

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
