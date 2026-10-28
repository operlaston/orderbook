#pragma once

#include <Protocol.h>
#include <Utils.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

// Blocking client for the orderbook server. Each request call sends one
// message and waits for its reply.
class OrderbookClient {
public:
  explicit OrderbookClient(uint16_t port, const char *host = "127.0.0.1") {
    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == -1) {
      throw std::system_error(errno, std::generic_category(), "socket");
    }

    sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host, &servAddr.sin_addr) <= 0) {
      ::close(m_socket);
      throw std::runtime_error("invalid server address");
    }
    if (::connect(m_socket, reinterpret_cast<sockaddr *>(&servAddr),
                  sizeof(servAddr)) == -1) {
      int err = errno;
      ::close(m_socket);
      throw std::system_error(err, std::generic_category(), "connect");
    }

    int one = 1;
    ::setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  }

  OrderbookClient(const OrderbookClient &) = delete;
  OrderbookClient &operator=(const OrderbookClient &) = delete;
  OrderbookClient(OrderbookClient &&) = delete;
  OrderbookClient &operator=(OrderbookClient &&) = delete;

  ~OrderbookClient() {
    if (m_socket != -1) {
      ::close(m_socket);
    }
  }

  NewOrderResult requestNewOrder(Side side, Price price, Quantity quantity) {
    auto msg = Protocol::encodeNewOrder(side, price, quantity);
    writeAll(msg.data(), msg.size());

    std::array<uint8_t, Protocol::NEW_ORDER_RESPONSE_SIZE> res;
    readAll(res.data(), res.size());
    return Protocol::decodeNewOrderResponse(res.data());
  }

  Status requestCancelOrder(OrderId orderId) {
    auto msg = Protocol::encodeCancelOrder(orderId);
    writeAll(msg.data(), msg.size());
    return readStatus();
  }

  Status requestModifyOrder(OrderId orderId, Quantity newQuantity) {
    auto msg = Protocol::encodeModifyOrder(orderId, newQuantity);
    writeAll(msg.data(), msg.size());
    return readStatus();
  }

  // Sends raw bytes. Useful for testing how the server handles bad input.
  void sendRaw(const void *buf, size_t n) { writeAll(buf, n); }

  Status readStatus() {
    uint8_t status;
    readAll(&status, 1);
    return static_cast<Status>(status);
  }

  static const char *statusName(Status status) {
    switch (status) {
    case Status::Success:
      return "SUCCESS";
    case Status::BadRequest:
      return "BAD_REQUEST";
    case Status::ServerError:
      return "SERVER_ERROR";
    }
    return "UNKNOWN_STATUS";
  }

private:
  int m_socket = -1;

  void writeAll(const void *buf, size_t n) {
    const uint8_t *pos = static_cast<const uint8_t *>(buf);
    while (n > 0) {
      ssize_t written = ::send(m_socket, pos, n, MSG_NOSIGNAL);
      if (written == -1) {
        if (errno == EINTR) {
          continue;
        }
        throw std::system_error(errno, std::generic_category(), "send");
      }
      pos += written;
      n -= static_cast<size_t>(written);
    }
  }

  void readAll(void *buf, size_t n) {
    uint8_t *pos = static_cast<uint8_t *>(buf);
    while (n > 0) {
      ssize_t bytesRead = ::read(m_socket, pos, n);
      if (bytesRead == -1) {
        if (errno == EINTR) {
          continue;
        }
        throw std::system_error(errno, std::generic_category(), "read");
      }
      if (bytesRead == 0) {
        throw std::runtime_error("server closed the connection");
      }
      pos += bytesRead;
      n -= static_cast<size_t>(bytesRead);
    }
  }
};
