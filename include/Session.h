#pragma once
#include <unistd.h>

class Session {
private:
  // client socket file descriptor
  int m_socket;

public:
  Session(int socketFd) : m_socket(socketFd) {}
  ~Session() {
    if (m_socket != -1) {
      close(m_socket);
    }
  }
};
