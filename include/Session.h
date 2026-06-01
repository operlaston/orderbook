#pragma once
#include <unistd.h>

class Session {
private:
  // client socket file descriptor
  int m_socket = -1;

public:
  Session(int socketFd) : m_socket(socketFd) {}
  Session(const Session &other) = delete;
  Session &operator=(const Session &other) = delete;
  Session(Session &&other) {
    m_socket = other.m_socket;
    other.m_socket = -1;
  }
  Session &operator=(Session &&other) {
    if (this != &other) {
      if (m_socket != -1) {
        ::close(m_socket);
      }
      m_socket = other.m_socket;
      other.m_socket = -1;
    }
    return *this;
  }
  ~Session() {
    if (m_socket != -1) {
      ::close(m_socket);
    }
  }
  int getSocket() const { return m_socket; }
};
