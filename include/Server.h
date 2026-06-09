#pragma once
#include "ServerEngineContext.h"
#include <Session.h>
#include <cstdint>
#include <memory>
#include <unordered_map>

class Server {
private:
  // mapping: [fd, Session]
  std::unordered_map<int, Session> m_sessions;
  ServerEngineContext &m_ctx;
  int m_socket = -1;
  int m_epollFd = -1;

  void initFds(uint16_t port);
  void addFdEpoll(int fd);
  void removeFdEpoll(int fd);
  void removeSession(int fd);
  void acceptClient();
  void markSessionClosed(int fd);
  // void readMessageComponent(int fd, void *msgComponentPtr,
  //                           size_t componentSize);

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
