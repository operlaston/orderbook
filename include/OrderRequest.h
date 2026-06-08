#pragma once

#include <OrderType.h>
#include <Side.h>
#include <TimeInForce.h>
#include <Using.h>

struct OrderRequest {
  Side side;
  OrderType orderType;
  TimeInForce timeInForce;
  Price price;
  Quantity quantity;
};
