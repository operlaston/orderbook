#pragma once

#include <RequestTypes.h>
#include <ResponseTypes.h>
#include <SpscQueue.h>
#include <cerrno>
#include <sys/eventfd.h>
#include <system_error>
#include <unistd.h>

// Shared state between the server (network) thread and the engine thread.
// - incomingRequests: server -> engine. The server is the only producer and
//   the engine the only consumer.
// - outgoingResponses: engine -> server, likewise single producer/consumer.
// - eventFd: the server sleeps in epoll_wait, so it can't poll the response
//   queue. The engine writes to this eventfd after pushing responses. The fd
//   is in the server's epoll set, so the write wakes it to drain the queue.
//
// The queues are a few MB, so allocate this on the heap.
struct ServerEngineContext {
  alignas(CACHE_LINE_SIZE) SpscQueue<ClientRequest, 65536> incomingRequests;
  alignas(CACHE_LINE_SIZE) SpscQueue<ServerResponse, 65536> outgoingResponses;
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
