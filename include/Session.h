#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unistd.h>

// One connected client. Owns the socket fd (closed on destruction) and a
// buffer of received bytes that don't yet form a complete message.
//
// Lifecycle: a session is "closed" once we stop reading from it (the client
// hung up, sent garbage, or a write failed). It is removed from epoll then,
// but the object (and so the fd) stays alive until every request it sent has
// been answered by the engine. Closing the fd earlier would let the kernel
// reuse the number for a new client, which would then receive the old
// client's responses.
class Session {
public:
  static constexpr size_t RECV_BUFFER_SIZE = 4096;

  explicit Session(int socketFd) : m_socket(socketFd) {}
  // Sessions are built in place in the server's map and never moved.
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;
  Session(Session &&) = delete;
  Session &operator=(Session &&) = delete;
  ~Session() {
    if (m_socket != -1) {
      ::close(m_socket);
    }
  }

  int getSocket() const { return m_socket; }

  // ---- receive buffer ----
  uint8_t *recvSpace() { return m_recvBuf.data() + m_recvLen; }
  size_t recvSpaceLeft() const { return RECV_BUFFER_SIZE - m_recvLen; }
  void commitReceived(size_t n) {
    assert(n <= recvSpaceLeft());
    m_recvLen += n;
  }
  const uint8_t *recvData() const { return m_recvBuf.data(); }
  size_t recvLen() const { return m_recvLen; }
  // Drops the first n bytes (fully parsed messages) and moves the leftover
  // partial message to the front.
  void consume(size_t n) {
    assert(n <= m_recvLen);
    std::memmove(m_recvBuf.data(), m_recvBuf.data() + n, m_recvLen - n);
    m_recvLen -= n;
  }

  // ---- state ----
  uint32_t getActiveRequests() const { return m_activeRequests; }
  void addActiveRequest() { m_activeRequests++; }
  void removeActiveRequest() {
    assert(m_activeRequests > 0);
    m_activeRequests--;
  }

  bool isClosed() const { return m_closed; }
  void markClosed() { m_closed = true; }

  // A write failed or was cut short, so the byte stream to this client is
  // unusable. Don't write anything else.
  bool isBroken() const { return m_broken; }
  void markBroken() { m_broken = true; }

  // Send one BAD_REQUEST byte just before the session is removed. Used for an
  // unknown message type, after the replies to earlier requests have gone out.
  bool shouldRejectOnRemove() const { return m_rejectOnRemove; }
  void setRejectOnRemove() { m_rejectOnRemove = true; }

private:
  int m_socket = -1;
  uint32_t m_activeRequests = 0;
  bool m_closed = false;
  bool m_broken = false;
  bool m_rejectOnRemove = false;
  size_t m_recvLen = 0;
  std::array<uint8_t, RECV_BUFFER_SIZE> m_recvBuf;
};
