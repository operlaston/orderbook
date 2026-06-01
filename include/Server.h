#pragma once
#include <Session.h>
#include <cstdint>
#include <vector>

class Server {
private:
  std::vector<Session> m_sessions;
  int m_socket = -1;
  int m_epollFd = -1;
  bool addFdEpoll(int fd);
  bool removeFdEpoll(int fd);
  void acceptClient();

public:
  Server(uint16_t port = 8080);
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server &operator=(Server &&) = delete;
  Server(Server &&) = delete;
  ~Server();
  void run();
};
