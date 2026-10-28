#include <Protocol.h>
#include <Server.h>
#include <Utils.h>
#include <arpa/inet.h>
#include <array>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <optional>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <variant>

namespace {
// Reads per readable event before going back to epoll, so one busy client
// can't starve the others. epoll is level-triggered, so leftover data
// triggers another event.
constexpr int MAX_READS_PER_EVENT = 16;

static_assert(Protocol::MAX_MESSAGE_SIZE < Session::RECV_BUFFER_SIZE);
} // namespace

Server::Server(ServerEngineContext &ctx, uint16_t port) : m_ctx(ctx) {
  // The destructor doesn't run if the constructor throws, so clean up here.
  auto fail = [this](const char *what) {
    int err = errno;
    if (m_socket != -1) {
      ::close(m_socket);
    }
    if (m_epollFd != -1) {
      ::close(m_epollFd);
    }
    throw std::system_error(err, std::generic_category(), what);
  };

  m_epollFd = ::epoll_create1(0);
  if (m_epollFd == -1) {
    fail("failed to create epoll instance");
  }

  m_socket = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (m_socket == -1) {
    fail("failed to create socket");
  }

  int opt = 1;
  if (::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) ==
      -1) {
    fail("failed to set SO_REUSEADDR");
  }

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port);

  if (::bind(m_socket, reinterpret_cast<sockaddr *>(&serverAddr),
             sizeof(serverAddr)) == -1) {
    fail("failed to bind socket");
  }
  if (::listen(m_socket, SOMAXCONN) == -1) {
    fail("failed to listen on socket");
  }
  if (!addFdEpoll(m_socket) || !addFdEpoll(m_ctx.eventFd)) {
    fail("failed to add fd to epoll");
  }
  std::cout << "Server started on port " << port << std::endl;
}

Server::~Server() {
  m_sessions.clear(); // closes client sockets
  if (m_socket != -1) {
    ::close(m_socket);
  }
  if (m_epollFd != -1) {
    ::close(m_epollFd);
  }
}

bool Server::addFdEpoll(int fd) {
  epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = fd;
  return ::epoll_ctl(m_epollFd, EPOLL_CTL_ADD, fd, &event) == 0;
}

void Server::removeFdEpoll(int fd) {
  if (::epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
    ::perror("epoll_ctl DEL");
  }
}

void Server::acceptClients() {
  // The listening socket is non-blocking, so accept until it runs dry.
  while (true) {
    int fd = ::accept4(m_socket, nullptr, nullptr, SOCK_NONBLOCK);
    if (fd == -1) {
      if (errno == EINTR || errno == ECONNABORTED) {
        continue;
      }
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        ::perror("accept");
      }
      return;
    }

    // Responses are tiny. Without this, Nagle's algorithm can hold them back
    // for tens of milliseconds.
    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    m_sessions.try_emplace(fd, fd);
    if (!addFdEpoll(fd)) {
      ::perror("epoll_ctl ADD");
      m_sessions.erase(fd); // closes fd
    }
  }
}

// Reads whatever the client sent and parses every complete message in it.
void Server::handleReadable(int fd) {
  auto it = m_sessions.find(fd);
  // The session can already be closed by an earlier event in the same
  // epoll_wait batch.
  if (it == m_sessions.end() || it->second.isClosed()) {
    return;
  }
  Session &session = it->second;

  for (int i = 0; i < MAX_READS_PER_EVENT; i++) {
    ssize_t n = ::read(fd, session.recvSpace(), session.recvSpaceLeft());
    if (n > 0) {
      session.commitReceived(static_cast<size_t>(n));
      if (!parseMessages(fd, session)) {
        return; // session was closed, don't touch it again
      }
      continue;
    }
    if (n == 0) { // client hung up
      closeSession(fd);
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      closeSession(fd);
    }
    return;
  }
}

// Framing: each message starts with a type byte that fixes its length. Pull
// out every complete message in the buffer and keep a trailing partial one
// for the next read. Returns false if the session was closed, in which case
// it may already be destroyed.
bool Server::parseMessages(int fd, Session &session) {
  const uint8_t *data = session.recvData();
  size_t len = session.recvLen();
  size_t offset = 0;

  while (offset < len) {
    uint8_t type = data[offset];
    std::optional<size_t> size = Protocol::messageSize(type);
    if (!size) {
      // There's no way to tell where the next message starts, so give up on
      // this connection.
      closeSession(fd, /*sendReject=*/true);
      return false;
    }
    if (len - offset < *size) {
      break; // partial message, wait for more bytes
    }

    const uint8_t *payload = data + offset + 1;
    ClientRequest req;
    switch (static_cast<Protocol::MessageType>(type)) {
    case Protocol::MessageType::NewOrder:
      req = Protocol::decodeNewOrder(payload, fd);
      break;
    case Protocol::MessageType::CancelOrder:
      req = Protocol::decodeCancelOrder(payload, fd);
      break;
    case Protocol::MessageType::ModifyOrder:
      req = Protocol::decodeModifyOrder(payload, fd);
      break;
    }
    offset += *size;

    // Count the request before pushing. pushRequest can send responses, and
    // a non-zero count keeps this session alive while it does.
    session.addActiveRequest();
    pushRequest(req);
    if (session.isClosed()) {
      return false; // a write to this client failed while we waited
    }
  }

  session.consume(offset);
  return true;
}

void Server::pushRequest(const ClientRequest &req) {
  while (!m_ctx.incomingRequests.push(req)) {
    // The engine may itself be waiting for room in the response queue.
    // Drain it here, or the two threads would wait on each other forever.
    drainResponses();
  }
}

void Server::drainResponses() {
  while (std::optional<ServerResponse> res = m_ctx.outgoingResponses.pop()) {
    sendResponse(*res);
  }
}

void Server::sendResponse(const ServerResponse &res) {
  int fd = std::visit([](const auto &r) { return r.sessionId; }, res);
  auto it = m_sessions.find(fd);
  if (it == m_sessions.end()) {
    // Can't happen: a session lives until all its responses are in.
    assert(false && "response for unknown session");
    return;
  }
  Session &session = it->second;
  session.removeActiveRequest();

  if (!session.isBroken()) {
    bool ok = std::visit(
        overload{[&](const Response::NewOrder &r) {
                   auto buf =
                       Protocol::encodeNewOrderResponse(r.status, r.orderId);
                   return writeAll(fd, buf.data(), buf.size());
                 },
                 [&](const Response::CancelOrder &r) {
                   uint8_t status = static_cast<uint8_t>(r.status);
                   return writeAll(fd, &status, 1);
                 },
                 [&](const Response::ModifyOrder &r) {
                   uint8_t status = static_cast<uint8_t>(r.status);
                   return writeAll(fd, &status, 1);
                 }},
        res);
    if (!ok) {
      session.markBroken();
      closeSession(fd);
      return;
    }
  }
  removeIfDone(fd);
}

// Sockets are non-blocking. Responses are tiny, so a short or failed write
// means the client stopped reading and its socket buffer is full, or the
// connection is gone. Either way the caller drops the client (slow consumer
// disconnect) rather than buffering for it without limit.
bool Server::writeAll(int fd, const void *buf, size_t n) {
  const uint8_t *pos = static_cast<const uint8_t *>(buf);
  while (n > 0) {
    ssize_t written = ::send(fd, pos, n, MSG_NOSIGNAL);
    if (written > 0) {
      pos += written;
      n -= static_cast<size_t>(written);
    } else if (written == -1 && errno == EINTR) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}

// Stops reading from a client. The session is destroyed (closing the fd) once
// the engine has answered all of its outstanding requests.
void Server::closeSession(int fd, bool sendReject) {
  auto it = m_sessions.find(fd);
  if (it == m_sessions.end()) {
    return;
  }
  Session &session = it->second;
  if (!session.isClosed()) {
    removeFdEpoll(fd);
    session.markClosed();
  }
  if (sendReject) {
    session.setRejectOnRemove();
  }
  removeIfDone(fd);
}

void Server::removeIfDone(int fd) {
  auto it = m_sessions.find(fd);
  if (it == m_sessions.end()) {
    return;
  }
  Session &session = it->second;
  if (!session.isClosed() || session.getActiveRequests() > 0) {
    return;
  }
  if (session.shouldRejectOnRemove() && !session.isBroken()) {
    uint8_t status = static_cast<uint8_t>(Status::BadRequest);
    writeAll(fd, &status, 1);
  }
  m_sessions.erase(it); // closes the socket
}

void Server::run() {
  std::array<epoll_event, 64> events;

  while (true) {
    int numEvents =
        ::epoll_wait(m_epollFd, events.data(), events.size(), -1);
    if (numEvents == -1) {
      if (errno == EINTR) {
        continue;
      }
      throw std::system_error(errno, std::generic_category(), "epoll_wait");
    }

    for (int i = 0; i < numEvents; i++) {
      int fd = events[i].data.fd;
      if (fd == m_socket) {
        acceptClients();
      } else if (fd == m_ctx.eventFd) {
        // Reset the eventfd counter, then send every queued response. One
        // wakeup can cover many responses.
        uint64_t count;
        if (::read(m_ctx.eventFd, &count, sizeof(count)) == -1 &&
            errno != EAGAIN) {
          throw std::system_error(errno, std::generic_category(),
                                  "eventfd read");
        }
        drainResponses();
      } else {
        handleReadable(fd);
      }
    }
  }
}
