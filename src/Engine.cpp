#include <Engine.h>
#include <Utils.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <unistd.h>
#include <variant>

namespace {

// Responses pushed without waking the server are capped, so replies are never
// held back for long under constant load.
constexpr int MAX_UNSIGNALLED_RESPONSES = 32;

void signalServer(int eventFd) {
  uint64_t inc = 1;
  ssize_t n;
  do {
    n = ::write(eventFd, &inc, sizeof(inc));
  } while (n < 0 && errno == EINTR);

  // Only fails if the counter would overflow, which can't happen here.
  if (n != sizeof(inc)) {
    ::perror("eventFd write");
    std::abort();
  }
}

ServerResponse errorResponse(const ClientRequest &req) {
  return std::visit(
      overload{[](const Request::NewOrder &r) -> ServerResponse {
                 return Response::NewOrder{r.sessionId, Status::ServerError,
                                           0};
               },
               [](const Request::CancelOrder &r) -> ServerResponse {
                 return Response::CancelOrder{r.sessionId,
                                              Status::ServerError};
               },
               [](const Request::ModifyOrder &r) -> ServerResponse {
                 return Response::ModifyOrder{r.sessionId,
                                              Status::ServerError};
               }},
      req);
}

} // namespace

ServerResponse handleRequest(Orderbook &orderbook, const ClientRequest &req) {
  return std::visit(
      overload{[&](const Request::NewOrder &r) -> ServerResponse {
                 NewOrderResult res =
                     orderbook.addOrder(r.side, r.price, r.quantity);
                 return Response::NewOrder{r.sessionId, res.status,
                                           res.orderId};
               },
               [&](const Request::CancelOrder &r) -> ServerResponse {
                 return Response::CancelOrder{r.sessionId,
                                              orderbook.cancelOrder(r.orderId)};
               },
               [&](const Request::ModifyOrder &r) -> ServerResponse {
                 return Response::ModifyOrder{
                     r.sessionId,
                     orderbook.modifyOrder(r.orderId, r.newQuantity)};
               }},
      req);
}

void runEngine(Orderbook &orderbook, ServerEngineContext &ctx,
               std::stop_token stop) {
  int unsignalled = 0;
  auto flush = [&] {
    if (unsignalled > 0) {
      signalServer(ctx.eventFd);
      unsignalled = 0;
    }
  };

  while (!stop.stop_requested()) {
    std::optional<ClientRequest> req = ctx.incomingRequests.pop();
    if (!req) {
      // Nothing more to do right now, so wake the server for whatever we've
      // pushed.
      flush();
      continue;
    }

    ServerResponse res;
    try {
      res = handleRequest(orderbook, *req);
    } catch (const std::exception &e) {
      // The engine itself doesn't throw, so this can only be an allocation
      // failure (e.g. a level or the id map growing). Report it and keep
      // serving.
      std::fprintf(stderr, "engine error: %s\n", e.what());
      res = errorResponse(*req);
    }

    // Never drop a response. If the queue is full, make sure the server has
    // been woken to drain it, then wait.
    while (!ctx.outgoingResponses.push(res)) {
      flush();
      if (stop.stop_requested()) {
        return;
      }
    }
    if (++unsignalled >= MAX_UNSIGNALLED_RESPONSES) {
      flush();
    }
  }
  flush();
}
