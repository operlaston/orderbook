#pragma once
#include "ResponseTypes.h"
#include "ServerEngineContext.h"
#include <Session.h>
#include <cstdint>
#include <unordered_map>

class Server {
private:
  // mapping: [fd, Session]
  std::unordered_map<int, Session> m_sessions;
  ServerEngineContext &m_ctx;
  int m_socket = -1;
  int m_epollFd = -1;

  void addFdEpoll(int fd);
  void removeFdEpoll(int fd);
  void removeSession(int fd);
  void acceptClient();
  void markSessionClosed(int fd);
  bool handleNewOrder(int currFd);
  bool handleCancelOrder(int currFd);
  bool handleModifyOrder(int currFd);
  void sendResponse(const ServerResponse &res);
  void writeSafely(int fd, const void *buf, size_t n);

public:
  // Server(uint16_t port = 8080);
  Server(ServerEngineContext &ctx, uint16_t port = 8080);
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server &operator=(Server &&) = delete;
  Server(Server &&) = delete;
  ~Server();
  void run();
};
