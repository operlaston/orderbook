#pragma once

#include "Orderbook.h"
#include "Protocol.h"
#include "Request.h"
#include "Response.h"
#include "ServerEngineContext.h"
#include <cstdint>
#include <optional>
#include <stop_token>

class Engine {
public:
  Engine(uint64_t blockCount, uint32_t initialOrderMax, ServerEngineContext& ctx) :
    m_orderbook(blockCount, initialOrderMax),
    m_ctx(ctx) {}

  void run(std::stop_token st) {
    while (!st.stop_requested()) {
      execNextRequest();
    }
  }

  void execNextRequest() {
    std::optional<Request> requestOpt = m_ctx.incomingRequests.pop();
    if (requestOpt.has_value()) {
      Request request = *requestOpt;
      switch (request.type) {
        case Protocol::MessageType::NewOrder:
          {
            auto [newOrderId, status] =
              m_orderbook.addOrder(request.side, request.price, request.quantity);
            m_ctx.outgoingResponses.push(Response{request.sessionId,
                request.type, status, newOrderId});
            break;
          }
        case Protocol::MessageType::CancelOrder:
          {
            Protocol::Status status = m_orderbook.cancelOrder(request.orderId);
            m_ctx.outgoingResponses.push(Response{request.sessionId,
                request.type, status, request.orderId});
            break;
          }
        case Protocol::MessageType::ModifyOrder:
          {
            Protocol::Status status = m_orderbook.modifyOrder(request.orderId, request.quantity);
            m_ctx.outgoingResponses.push(Response{request.sessionId,
                request.type, status, request.orderId});
            break;
          }
          // requests without a proper message type should not arrive because the network-side thread should handle that
          // but if they do they are simply skipped
      }
      writeToServer(m_ctx.eventFd);
    }
  }

  void writeToServer(int eventFd) {
    uint64_t inc = 1;
    ssize_t n;
    do {
      n = ::write(eventFd, &inc, sizeof(inc));
    } while (n < 0 && errno == EINTR);
  }

private:
  Orderbook m_orderbook;
  ServerEngineContext &m_ctx;
};
