#pragma once

#include <RequestTypes.h>
#include <ResponseTypes.h>
#include <ServerEngineContext.h>
#include <Session.h>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

// Single-threaded epoll TCP server. It parses client requests, forwards them
// to the engine thread through ctx.incomingRequests, and writes the engine's
// responses (ctx.outgoingResponses, signalled by ctx.eventFd) back to the
// right client. Responses to a client go out in the order of its requests.
class Server {
public:
  explicit Server(ServerEngineContext &ctx, uint16_t port = 8080);
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server(Server &&) = delete;
  Server &operator=(Server &&) = delete;
  ~Server();

  // Runs the event loop forever.
  void run();

private:
  // sessions keyed by socket fd
  std::unordered_map<int, Session> m_sessions;
  ServerEngineContext &m_ctx;
  int m_socket = -1;
  int m_epollFd = -1;

  bool addFdEpoll(int fd);
  void removeFdEpoll(int fd);
  void acceptClients();
  void handleReadable(int fd);
  bool parseMessages(int fd, Session &session);
  void pushRequest(const ClientRequest &req);
  void drainResponses();
  void sendResponse(const ServerResponse &res);
  bool writeAll(int fd, const void *buf, size_t n);
  void closeSession(int fd, bool sendReject = false);
  void removeIfDone(int fd);
};
