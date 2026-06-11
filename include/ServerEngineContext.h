#pragma once

#include <RequestTypes.h>
#include <ResponseTypes.h>
#include <SpscQueue.h>
#include <cerrno>
#include <sys/eventfd.h>
#include <system_error>
#include <unistd.h>

struct ServerEngineContext {
  alignas(CACHE_LINE_SIZE) SpscQueue<ClientRequest, 65536> incomingRequests;
  alignas(CACHE_LINE_SIZE) SpscQueue<ServerResponse, 65536> outgoingResponses;
  alignas(CACHE_LINE_SIZE) int eventFd = -1;

  ServerEngineContext() {
    eventFd = eventfd(0, EFD_NONBLOCK);
    if (eventFd == -1) {
      std::system_error(errno, std::generic_category(),
                        "Failed to create eventFd");
    }
  }
  // no copying (nor moving)
  ServerEngineContext(const ServerEngineContext &) = delete;
  ServerEngineContext &operator=(const ServerEngineContext &) = delete;
  ~ServerEngineContext() {
    if (eventFd != -1) {
      ::close(eventFd);
    }
  }
};
