#include "GlobalConsts.h"
#include <MessageType.h>
#include <OrderType.h>
#include <Side.h>
#include <TimeInForce.h>
#include <Utils.h>
#include <arpa/inet.h>
#include <array>
#include <bit>
#include <cstring>
#include <endian.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

class OrderbookClient {
private:
  int m_socket;

public:
  explicit OrderbookClient(uint16_t port) {
    struct sockaddr_in serv_addr;

    if ((m_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("socket");
      throw std::runtime_error("socket creation failed");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
      perror("inet_pton");
      throw std::runtime_error("inet_pton failed");
    }

    if (connect(m_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <
        0) {
      perror("connect");
      throw std::runtime_error("inet_pton failed");
    }

    std::cout << "Connected to server on port " << port << std::endl;
  }

  // delete copy and move semantics
  OrderbookClient(const OrderbookClient &other) = delete;
  OrderbookClient &operator=(const OrderbookClient &other) = delete;
  OrderbookClient(OrderbookClient &&other) = delete;
  OrderbookClient &operator=(OrderbookClient &&other) = delete;

  ~OrderbookClient() {
    if (m_socket != -1) {
      close(m_socket);
      m_socket = -1;
    }
  }

  void safeWrite(const void *buf, size_t n) {
    ssize_t bytesWritten;
    char *pos = (char *)buf;
    do {
      bytesWritten = write(m_socket, pos, n);
      if (bytesWritten == -1) {
        if (errno == EINTR)
          continue;
        else {
          ::perror("write");
          std::abort();
        }
      }
      n -= bytesWritten;
      pos += bytesWritten;
    } while (n > 0);
  }

  void safeRead(void *buf, size_t n) {
    ssize_t bytesRead;
    do {
      bytesRead = read(m_socket, buf, n);
      if (bytesRead == -1) {
        if (errno == EINTR)
          continue;
        else {
          ::perror("read");
          std::abort();
        }
      }
      n -= bytesRead;
      buf = (char *)buf + bytesRead;
    } while (n > 0);
  }

  OrderId requestNewOrder(Side side, OrderType orderType,
                          TimeInForce timeInForce, Price price,
                          Quantity quantity) {
    std::array<uint8_t, 20> buf;
    buf[0] = static_cast<uint8_t>(MessageType::NEW_ORDER);
    buf[1] = static_cast<uint8_t>(side);
    buf[2] = static_cast<uint8_t>(orderType);
    buf[3] = static_cast<uint8_t>(timeInForce);

    uint64_t priceRaw = std::bit_cast<uint64_t>(price);
    priceRaw = htobe64(priceRaw);
    uint64_t quantityRaw = htobe64(quantity);

    std::memcpy(&buf[4], &priceRaw, 8);
    std::memcpy(&buf[12], &quantityRaw, 8);
    safeWrite(buf.data(), buf.size());

    // get response
    std::array<uint8_t, 9> responseBuf;
    safeRead(responseBuf.data(), responseBuf.size());

    printStatus(responseBuf[0]);

    uint64_t newOrderIdRaw;
    memcpy(&newOrderIdRaw, &responseBuf[1], 8);
    OrderId newOrderId = be64toh(newOrderIdRaw);
    if (newOrderId > 0)
      std::cout << "\nNew Order ID is " << newOrderId << std::endl;
    return newOrderId;
  }

  OrderId requestMarketOrder(Side side, Quantity quantity) {
    return requestNewOrder(side, OrderType::MARKET, TimeInForce::NONE, 0,
                           quantity);
  }

  OrderId requestLimitOrder(Side side, TimeInForce timeInForce, Price price,
                            Quantity quantity) {
    return requestNewOrder(side, OrderType::LIMIT, timeInForce, price,
                           quantity);
  }

  void requestCancelOrder(OrderId orderId) {
    std::cout << "Cancel Order " << orderId << std::endl;
    std::array<uint8_t, 9> buf;
    buf[0] = static_cast<uint8_t>(MessageType::CANCEL_ORDER);
    uint64_t orderIdRaw = htobe64(orderId);
    memcpy(&buf[1], &orderIdRaw, 8);
    safeWrite(buf.data(), buf.size());

    uint8_t res;
    safeRead(&res, 1);
    printStatus(res);
  }

  void requestModifyOrder(OrderId orderId, Quantity newQuantity) {
    std::cout << "Modify Order " << orderId
              << ". Attempt to change quantity to " << newQuantity << std::endl;
    std::array<uint8_t, 17> buf;
    buf[0] = static_cast<uint8_t>(MessageType::MODIFY_ORDER);
    uint64_t orderIdRaw = htobe64(orderId);
    uint64_t newQuantityRaw = htobe64(newQuantity);
    memcpy(&buf[1], &orderIdRaw, 8);
    memcpy(&buf[9], &newQuantityRaw, 8);
    safeWrite(buf.data(), buf.size());

    uint8_t res;
    safeRead(&res, 1);
    printStatus(res);
  }

  static int parseOption(int low, int high) {
    int actionOption = 0;
    bool success = false;
    while (!success) {
      if (!(std::cin >> actionOption)) {
        std::cout << "Please enter a valid number from " << low << "-" << high
                  << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max());
      } else if (actionOption < low || actionOption > high) {
        std::cout << "Please enter a valid number from " << low << "-" << high
                  << std::endl;
      } else {
        success = true;
      }
    }
    return actionOption;
  }

  static void printStatus(uint8_t status) {
    std::cout << std::endl;
    switch (status) {
    case ResponseStatus::SUCCESS:
      std::cout << "SUCCESS" << std::endl;
      break;
    case ResponseStatus::BAD_REQUEST:
      std::cout << "BAD REQUEST" << std::endl;
      break;
    case ResponseStatus::INVALID_MESSAGE_TYPE:
      std::cout << "INVALID MESSAGE TYPE" << std::endl;
      break;
    case ResponseStatus::SERVER_ERROR:
      std::cout << "SERVER ERROR" << std::endl;
      break;
    case ResponseStatus::PARTIAL_FILL:
      std::cout << "PARTIALLY FILLED ORDER" << std::endl;
      break;
    case ResponseStatus::CANT_FILL:
      std::cout << "CANT FILL ORDER" << std::endl;
      break;
    }
  }
};
