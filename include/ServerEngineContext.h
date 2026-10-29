#pragma once

#include <Utils.h>
#include <Request.h>
#include <Response.h>
#include <SpscQueue.h>
#include <cerrno>
#include <sys/eventfd.h>
#include <system_error>
#include <unistd.h>

struct ServerEngineContext {
  alignas(CACHE_LINE_SIZE) SpscQueue<Request, SPSC_QUEUE_SIZE> incomingRequests;
  alignas(CACHE_LINE_SIZE) SpscQueue<Response, SPSC_QUEUE_SIZE> outgoingResponses;
  alignas(CACHE_LINE_SIZE) int eventFd = -1;

  ServerEngineContext() {
    eventFd = eventfd(0, EFD_NONBLOCK);
    if (eventFd == -1) {
      throw std::system_error(errno, std::generic_category(),
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
