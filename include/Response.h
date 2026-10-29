#pragma once

#include <Utils.h>
#include <Protocol.h>

struct Response {
  int sessionId;
  Protocol::MessageType type;
  Protocol::Status status;
  OrderId orderId;
};
