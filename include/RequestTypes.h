#pragma once

#include <OrderType.h>
#include <Side.h>
#include <TimeInForce.h>
#include <Using.h>

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
