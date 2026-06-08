#include <GlobalConsts.h>
#include <MessageType.h>
#include <OrderRequest.h>
#include <OrderType.h>
#include <Server.h>
#include <Side.h>
#include <TimeInForce.h>
#include <Using.h>
#include <array>
#include <bit>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

Server::Server(uint16_t port) {

  // initialize epoll fd
  m_epollFd = ::epoll_create1(0);
  if (m_epollFd == -1) {
    throw std::runtime_error("Failed to create epoll instance");
  }

  // initialize server socket
  m_socket = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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

void Server::addFdEpoll(int fd) {
  struct epoll_event sessionFdEvent{};
  sessionFdEvent.events = EPOLLIN;
  sessionFdEvent.data.fd = fd;
  if (::epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &sessionFdEvent) == -1) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to add fd to epoll interest list");
  }
}

void Server::removeFdEpoll(int fd) {
  if (epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, NULL) == -1) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to remove fd from epoll interest list");
  }
}

void Server::removeSession(int fd) {
  m_sessions.erase(fd);
  removeFdEpoll(fd);
}

void Server::markSessionClosed(int fd) { m_sessions[fd].markClosed(); }

void Server::acceptClient() {
  int clientSocket = -1;
  // i dont care about the client's ip for now
  if ((clientSocket = ::accept(m_socket, NULL, NULL)) < 0) {
    throw std::runtime_error("Failed to accept tcp connection");
  }

  // make socket nonblocking while maintaing current flags
  int currFlags = fcntl(clientSocket, F_GETFL, 0);
  if (currFlags == -1) {
    throw std::runtime_error("Failed to get flags of client socket");
  }
  currFlags |= O_NONBLOCK;
  if (fcntl(clientSocket, F_SETFL, currFlags) == -1) {
    throw std::runtime_error("Failed to set flags of clietn socket");
  }

  // add client socket to epoll interest list
  addFdEpoll(clientSocket);

  auto [it, didEmplace] = m_sessions.try_emplace(clientSocket, clientSocket);
  assert(didEmplace);
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
      int currFd = currEvent.data.fd;
      if (currFd == m_socket) {
        acceptClient();
      }
      // else if (currFd == eventFd) {
      //
      // }
      else {
        // handle all messages currently in the buffer
        while (true) {
          uint8_t messageTypeRaw;
          int bytesRead = read(currFd, &messageTypeRaw, 1);
          if (bytesRead == 0) { // client closed their end
            markSessionClosed(currFd);
            // if not processing any active requests from this client
            // remove the session object from the session map
            if (m_sessions[currFd].getActiveRequests() == 0) {
              removeSession(currFd);
            }
            break;
          } else if (bytesRead == -1) {
            if (errno == EAGAIN) { // no more messages to read
              break;
            } else { // unknown error occurred, remove the session
              removeSession(currFd);
            }
          }

          MessageType messageType = static_cast<MessageType>(messageTypeRaw);

          if (messageType == MessageType::ORDER) {
            // 1 byte Side, 1 byte OrderType, 1 byte TimeInForce, 8 byte Price,
            // 8 byte Quantity
            Side side;
            OrderType orderType;
            TimeInForce timeInForce;
            uint64_t priceRaw;
            uint64_t quantityRaw;

            std::array<uint8_t, GlobalLengths::ORDER_MESSAGE> buf;
            bytesRead = read(currFd, buf.data(), buf.size());
            if (bytesRead == 0) {
              markSessionClosed(currFd);
              write(currFd, &ResponseStatus::MALFORMED_REQUEST, 1);
              break;
            } else if (bytesRead != buf.size()) {
              write(currFd, &ResponseStatus::MALFORMED_REQUEST, 1);
              break;
            }
            side = static_cast<Side>(buf[0]);
            orderType = static_cast<OrderType>(buf[1]);
            timeInForce = static_cast<TimeInForce>(buf[2]);
            std::memcpy(&priceRaw, &buf[3], sizeof(Price));
            std::memcpy(&quantityRaw, &buf[11], sizeof(Quantity));

            Price price = be64toh(priceRaw);
            price = std::bit_cast<Price>(priceRaw);
            Quantity quantity = be64toh(quantityRaw);

            OrderRequest req{side, orderType, timeInForce, price, quantity};
            m_sessions[currFd].addActiveRequest();

            // TODO: push the request onto the spsc queue

          } else {
            // invalid message type (maybe supported in the future)
            write(currFd, &ResponseStatus::INVALID_MESSAGE_TYPE, 1);
            break;
          }
        }
      }
    }
  }
}
