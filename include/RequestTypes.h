#pragma once

#include <Utils.h>
#include <variant>

// Requests passed from the server thread to the engine thread.
// sessionId is the client's socket fd, so the response can be routed back.
namespace Request {
struct NewOrder {
  int sessionId;
  Side side;
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
