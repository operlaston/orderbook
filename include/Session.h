#pragma once
#include <cstdint>
#include <unistd.h>

class Session {
private:
  // client socket file descriptor
  int m_socket = -1;
  uint32_t m_activeRequests = 0;
  bool m_closed = false;

public:
  explicit Session(int socketFd) : m_socket(socketFd) {}
  Session(const Session &other) = delete;
  Session &operator=(const Session &other) = delete;
  Session(Session &&other) = delete;
  Session &operator=(Session &&other) = delete;
  ~Session() {
    if (m_socket != -1) {
      ::close(m_socket);
    }
  }
  int getSocket() const { return m_socket; }
  int getActiveRequests() const { return m_activeRequests; }
  bool isMarkedClosed() const { return m_closed; }
  void markClosed() { m_closed = true; }
  void addActiveRequest() { m_activeRequests++; }
  void removeActiveRequest() { m_activeRequests--; }
};
