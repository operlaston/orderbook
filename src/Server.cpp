#include <Server.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

Server::Server(uint16_t port) {
  m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
  if (m_socket == -1) {
    throw std::runtime_error("Failed to create socket");
    return;
  }

  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port);

  if (::bind(m_socket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
    throw std::runtime_error("Failed to bind socket");
    return;
  }

  if (::listen(m_socket, SOMAXCONN) == -1) {
    throw std::runtime_error("Failed to listen on socket");
    return;
  }

  std::cout << "Server started on port " << port << std::endl;
}

Server::~Server() {
  if (m_socket != -1) {
    ::close(m_socket);
  }
}

void Server::run() {

  while (true) {
    // dont care about client ip addresses for now
    // keeping this here in case i want to support it later
    // struct sockaddr_in clientAddr;
    // memset(&clientAddr, 0, sizeof(clientAddr));
    // socklen_t clientAddrSize = sizeof(clientAddr);

    int clientSocket = -1;
    if ((clientSocket = ::accept(m_socket, NULL, NULL)) < 0) {
      throw std::runtime_error("Failed to accept tcp connection");
    }
  }
}
