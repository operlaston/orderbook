#pragma once

#include <Utils.h>
#include <variant>
#include <Protocol.h>

// Responses passed from the engine thread back to the server thread.
// sessionId is the client's socket fd.
namespace Response {
struct NewOrder {
  int sessionId;
  Protocol::Status status;
  OrderId orderId; // 0 unless the order is resting in the book
};

struct CancelOrder {
  int sessionId;
  Protocol::Status status;
};

struct ModifyOrder {
  int sessionId;
  Protocol::Status status;
};
} // namespace Response

using ServerResponse = std::variant<Response::NewOrder, Response::CancelOrder,
                                    Response::ModifyOrder>;
