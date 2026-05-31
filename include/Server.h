#pragma once
#include <cstdint>

class Server {
private:
  int m_socket = -1;

public:
  Server(uint16_t port = 8080);
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server &operator=(Server &&) = delete;
  Server(Server &&) = delete;
  ~Server();
  void run();
};
