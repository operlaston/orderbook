#include <GlobalConsts.h>
#include <MessageType.h>
#include <OrderType.h>
#include <RequestTypes.h>
#include <Server.h>
#include <ServerEngineContext.h>
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

// Server::Server(uint16_t port) { initFds(port); }

Server::Server(ServerEngineContext &ctx, uint16_t port) : m_ctx(ctx) {
  initFds(port);
}

void Server::initFds(uint16_t port) {
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

// return false means error occurred or bad request
bool Server::handleNewOrder(int currFd) {
  // 1 byte Side, 1 byte OrderType, 1 byte TimeInForce, 8 byte Price,
  // 8 byte Quantity
  Side side;
  OrderType orderType;
  TimeInForce timeInForce;
  uint64_t priceRaw;
  uint64_t quantityRaw;

  std::array<uint8_t, GlobalLengths::NEW_ORDER_MESSAGE> buf;
  int bytesRead = read(currFd, buf.data(), buf.size());
  if (bytesRead == 0) {
    markSessionClosed(currFd);
    write(currFd, &ResponseStatus::BAD_REQUEST, 1);
    return false;
  } else if (bytesRead != buf.size()) {
    write(currFd, &ResponseStatus::BAD_REQUEST, 1);
    return false;
  }
  side = static_cast<Side>(buf[0]);
  orderType = static_cast<OrderType>(buf[1]);
  timeInForce = static_cast<TimeInForce>(buf[2]);
  std::memcpy(&priceRaw, &buf[3], sizeof(Price));
  std::memcpy(&quantityRaw, &buf[11], sizeof(Quantity));

  priceRaw = be64toh(priceRaw);
  Price price = std::bit_cast<Price>(priceRaw);
  Quantity quantity = be64toh(quantityRaw);

  Request::NewOrder newOrderReq{currFd,      side,  orderType,
                                timeInForce, price, quantity};
  m_sessions[currFd].addActiveRequest();

  // push the request onto the spsc queue
  // TODO: maybe look into exponential backoff or something?
  while (!m_ctx.incomingRequests.push(newOrderReq))
    ;

  return true;
}

bool Server::handleCancelOrder(int currFd) {
  OrderId orderId;
  int bytesRead = read(currFd, &orderId, sizeof(OrderId));
  if (bytesRead == 0) {
    markSessionClosed(currFd);
    write(currFd, &ResponseStatus::BAD_REQUEST, 1);
    return false;
  } else if (bytesRead != sizeof(OrderId)) {
    write(currFd, &ResponseStatus::BAD_REQUEST, 1);
    return false;
  }
  orderId = be64toh(orderId);

  Request::CancelOrder cancelOrderReq{currFd, orderId};
  m_sessions[currFd].addActiveRequest();

  // push the request onto the spsc queue
  // TODO: maybe look into exponential backoff or something?
  while (!m_ctx.incomingRequests.push(cancelOrderReq))
    ;
  return true;
}

bool Server::handleModifyOrder(int currFd) {
  OrderId orderId;
  Quantity newQuantity;
  std::array<uint8_t, GlobalLengths::MODIFY_ORDER_MESSAGE> buf;
  int bytesRead = read(currFd, buf.data(), buf.size());
  if (bytesRead == 0) {
    markSessionClosed(currFd);
    write(currFd, &ResponseStatus::BAD_REQUEST, 1);
    return false;
  } else if (bytesRead != buf.size()) {
    write(currFd, &ResponseStatus::BAD_REQUEST, 1);
    return false;
  }

  std::memcpy(&orderId, &buf[0], sizeof(OrderId));
  std::memcpy(&newQuantity, &buf[8], sizeof(Quantity));

  orderId = be64toh(orderId);
  newQuantity = be64toh(newQuantity);

  Request::ModifyOrder modifyOrderReq{currFd, orderId, newQuantity};
  m_sessions[currFd].addActiveRequest();

  // push the request onto the spsc queue
  // TODO: maybe look into exponential backoff or something?
  while (!m_ctx.incomingRequests.push(modifyOrderReq))
    ;
  return true;
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
      } else if (currFd == m_ctx.eventFd) {
        // read responses and forward them back to proper client
      } else {
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

          if (messageType == MessageType::NEW_ORDER) {
            if (!handleNewOrder(currFd))
              break;

          } else if (messageType == MessageType::CANCEL_ORDER) {
            if (!handleCancelOrder(currFd))
              break;

          } else if (messageType == MessageType::MODIFY_ORDER) {
            if (!handleModifyOrder(currFd))
              break;

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
