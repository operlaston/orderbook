#include <Server.h>
#include <array>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

Server::Server(uint16_t port) {

  // initialize epoll fd
  m_epollFd = ::epoll_create1(0);
  if (m_epollFd == -1) {
    throw std::runtime_error("Failed to create epoll instance");
  }

  // initialize server socket
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

  addFdEpoll(m_socket);

  std::cout << "Server started on port " << port << std::endl;
}

Server::~Server() {
  if (m_socket != -1) {
    ::close(m_socket);
  }
  if (m_epollFd != -1) {
    ::close(m_epollFd);
  }
}

bool Server::addFdEpoll(int fd) {
  struct epoll_event sessionFdEvent{};
  sessionFdEvent.events = EPOLLIN;
  sessionFdEvent.data.fd = fd;
  return ::epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &sessionFdEvent) == 0;
}

bool Server::removeFdEpoll(int fd) {
  return ::epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, NULL) == 0;
}

void Server::acceptClient() {
  int clientSocket = -1;
  // i dont care about the client's ip for now
  if ((clientSocket = ::accept(m_socket, NULL, NULL)) < 0) {
    throw std::runtime_error("Failed to accept tcp connection");
  }

  if (!addFdEpoll(clientSocket)) {
    throw std::runtime_error("Failed to add client fd to epoll interest list");
  }
  m_sessions.emplace_back(clientSocket);
}

void Server::run() {
  std::array<struct epoll_event, SOMAXCONN> revents;

  while (true) {
    int numEvents = epoll_wait(m_epollFd, revents.data(), revents.size(), -1);

    if (numEvents == -1) {
      throw std::runtime_error("Failed to receive events from epoll_wait");
    }

    for (int i = 0; i < numEvents; i++) {
      struct epoll_event currEvent = revents[i];
      if (currEvent.data.fd == m_socket) {
        acceptClient();
      } else {
        // TODO: handle client message
      }
    }
  }
}
