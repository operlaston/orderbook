#pragma once
#include <Session.h>
#include <cstdint>
#include <unordered_map>

class Server {
private:
  // mapping: [fd, Session]
  std::unordered_map<int, Session> m_sessions;
  int m_socket = -1;
  int m_epollFd = -1;
  void addFdEpoll(int fd);
  void removeFdEpoll(int fd);
  void removeSession(int fd);
  void acceptClient();
  void markSessionClosed(int fd);
  // void readMessageComponent(int fd, void *msgComponentPtr,
  //                           size_t componentSize);

public:
  Server(uint16_t port = 8080);
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server &operator=(Server &&) = delete;
  Server(Server &&) = delete;
  ~Server();
  void run();
};
