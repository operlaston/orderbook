#pragma once

#include <OrderRequest.h>
#include <SpscQueue.h>
#include <cerrno>
#include <sys/eventfd.h>
#include <system_error>

struct ServerEngineContext {
  alignas(CACHE_LINE_SIZE) SpscQueue<OrderRequest, 65536> incomingRequests;
  alignas(CACHE_LINE_SIZE) SpscQueue<uint8_t, 65536> outgoingResponses;
  alignas(CACHE_LINE_SIZE) int eventFd;

  ServerEngineContext() {
    eventFd = eventfd(0, EFD_NONBLOCK);
    if (eventFd == -1) {
      std::system_error(errno, std::generic_category(),
                        "Failed to create eventFd");
    }
  }
};
