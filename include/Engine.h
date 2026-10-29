#pragma once

#include "Orderbook.h"
#include "Protocol.h"
#include "Request.h"
#include "ServerEngineContext.h"
#include <cstdint>
#include <optional>
class Engine {
public:
  Engine(uint64_t blockCount, uint32_t initialOrderMax, ServerEngineContext& ctx) :
    m_orderbook(blockCount, initialOrderMax),
    m_ctx(ctx) {}

  void run() {
    while (true) {
      std::optional<Request> requestOpt = m_ctx.incomingRequests.pop();
      if (requestOpt.has_value()) {
        Request request = *requestOpt;
        switch (request.type) {
          case Protocol::MessageType::NewOrder:
            handleNew();
            break;
          case Protocol::MessageType::CancelOrder:
            handleCancel();
            break;
          case Protocol::MessageType::ModifyOrder:
            handleModify();
            break;
        }
      }
    }
  }


private:
  Orderbook m_orderbook;
  ServerEngineContext &m_ctx;

  void handleNew() {}
  void handleCancel() {}
  void handleModify() {}
};
