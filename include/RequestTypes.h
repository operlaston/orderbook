#pragma once

#include <OrderType.h>
#include <Side.h>
#include <TimeInForce.h>
#include <Using.h>

namespace Request {
struct NewOrder {
  Side side;
  OrderType orderType;
  TimeInForce timeInForce;
  Price price;
  Quantity quantity;
};

struct CancelOrder {
  OrderId orderId;
};

struct ModifyOrder {
  OrderId orderId;
  Quantity newQuantity;
};

} // namespace Request

using ClientRequest =
    std::variant<Request::NewOrder, Request::CancelOrder, Request::ModifyOrder>;
