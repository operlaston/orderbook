#pragma once

#include "Protocol.h"
#include <Utils.h>

struct Request {
  int sessionId;
  Protocol::MessageType type;
  Side side;
  OrderId orderId;
  Price price;
  Quantity quantity;

  Request(int sid, Side s, Price p, Quantity q) :
    sessionId(sid),
    type(Protocol::MessageType::NewOrder),
    side(s),
    orderId(0), // unused by new order request
    price(p),
    quantity(q)
  {}

  Request(int sid, OrderId oid) :
    sessionId(sid),
    type(Protocol::MessageType::CancelOrder),
    side(static_cast<Side>(0)), // unused by order cancellation request
    orderId(oid),
    price(0), // unused by order cancellation request
    quantity(0) // unused by order cancellation request
  {}

  Request(int sid, OrderId oid, Quantity q) :
    sessionId(sid),
    type(Protocol::MessageType::ModifyOrder),
    side(static_cast<Side>(0)), // unused by order modification request
    orderId(oid),
    price(0), // unused by order modification request
    quantity(q)
  {}
};
