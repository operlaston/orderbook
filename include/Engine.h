#pragma once

#include <Orderbook.h>
#include <RequestTypes.h>
#include <ResponseTypes.h>
#include <ServerEngineContext.h>
#include <stop_token>

// Runs one request against the book and builds its response.
ServerResponse handleRequest(Orderbook &orderbook, const ClientRequest &req);

// The engine thread's main loop. It busy-polls ctx.incomingRequests, applies
// each request to the book, pushes the response to ctx.outgoingResponses and
// wakes the server through ctx.eventFd. Returns once stop is requested. With
// the default token it runs forever.
void runEngine(Orderbook &orderbook, ServerEngineContext &ctx,
               std::stop_token stop = {});
