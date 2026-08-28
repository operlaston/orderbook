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
#include <sys/socket.h>
#include <unistd.h>

int sock = -1;

void safeWrite(int fd, const void *buf, size_t n) {
  ssize_t bytesWritten;
  char *pos = (char *)buf;
  do {
    bytesWritten = write(fd, pos, n);
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

void safeRead(int fd, void *buf, size_t n) {
  ssize_t bytesRead;
  do {
    bytesRead = read(fd, buf, n);
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

void printStatus(uint8_t status) {
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

OrderId requestNewOrder(Side side, OrderType orderType, TimeInForce timeInForce,
                        Price price, Quantity quantity) {
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
  safeWrite(sock, buf.data(), buf.size());

  // get response
  std::array<uint8_t, 9> responseBuf;
  safeRead(sock, responseBuf.data(), responseBuf.size());

  printStatus(responseBuf[0]);

  uint64_t newOrderIdRaw;
  memcpy(&newOrderIdRaw, &responseBuf[1], 8);
  OrderId newOrderId = be64toh(newOrderIdRaw);
  std::cout << "New Order ID is " << newOrderId << std::endl;
  return newOrderId;
}

OrderId requestMarketOrder(Side side, Quantity quantity) {
  return requestNewOrder(side, OrderType::MARKET, TimeInForce::NONE, 0,
                         quantity);
}

OrderId requestLimitOrder(Side side, TimeInForce timeInForce, Price price,
                          Quantity quantity) {
  return requestNewOrder(side, OrderType::LIMIT, timeInForce, price, quantity);
}

void requestCancelOrder(OrderId orderId) {
  std::cout << "Cancel Order " << orderId << std::endl;
  std::array<uint8_t, 9> buf;
  buf[0] = static_cast<uint8_t>(MessageType::CANCEL_ORDER);
  uint64_t orderIdRaw = htobe64(orderId);
  memcpy(&buf[1], &orderIdRaw, 8);
  safeWrite(sock, buf.data(), buf.size());

  uint8_t res;
  safeRead(sock, &res, 1);
  printStatus(res);
}

void requestModifyOrder(OrderId orderId, Quantity newQuantity) {
  std::cout << "Modify Order " << orderId << ". Attempt to change quantity to "
            << newQuantity << std::endl;
  std::array<uint8_t, 17> buf;
  buf[0] = static_cast<uint8_t>(MessageType::MODIFY_ORDER);
  uint64_t orderIdRaw = htobe64(orderId);
  uint64_t newQuantityRaw = htobe64(newQuantity);
  memcpy(&buf[1], &orderIdRaw, 8);
  memcpy(&buf[9], &newQuantityRaw, 8);
  safeWrite(sock, buf.data(), buf.size());

  uint8_t res;
  safeRead(sock, &res, 1);
  printStatus(res);
}

int parseOption(int low, int high) {
  int actionOption = 0;
  if (!(std::cin >> actionOption)) {
    std::cout << "Please enter a valid number from " << low << "-" << high
              << std::endl;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max());
    return -1;
  } else if (actionOption < low || actionOption > high) {
    std::cout << "Please enter a valid number from " << low << "-" << high
              << std::endl;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max());
    return -1;
  }
  return actionOption;
}

int main() {
  struct sockaddr_in serv_addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    std::cerr << "Error: Socket creation failed." << std::endl;
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(8080);

  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    std::cerr << "Error: Invalid address or address not supported."
              << std::endl;
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    std::cerr << "Error: Connection to server failed." << std::endl;
    return -1;
  }

  std::cout << "Connected to server on port 8080" << std::endl;

  while (true) {
    std::string actionsText = "=======Actions=======\n"
                              "(1) New Order\n"
                              "(2) Cancel Order\n"
                              "(3) Modify Order\n";

    std::cout << actionsText << std::endl;

    int actionOption = parseOption(1, 3);

    if (actionOption == 1) { // New Order
      std::string sidePrompt = "\n=======Side=======\n"
                               "(1) Buy\n"
                               "(2) Sell\n";
      std::string orderTypePrompt = "\n=======Order Type=======\n"
                                    "(1) Limit\n"
                                    "(2) Market\n";
      std::string timeInForcePrompt = "\n=======Time In Force=======\n"
                                      "(1) None\n"
                                      "(2) Good Til Cancel\n"
                                      "(3) Immediate Or Cancel\n"
                                      "(4) Fill Or Kill\n";
      std::string pricePrompt = "\nPrice: ";
      std::string quantityPrompt = "\nQuantity: ";

      std::cout << sidePrompt << std::endl;
      int sideRaw = parseOption(1, 2);
      sideRaw--;
      Side side = static_cast<Side>(sideRaw);

      std::cout << quantityPrompt;
      Quantity quantity = 0;
      std::cin >> quantity;

      std::cout << orderTypePrompt << std::endl;
      int orderTypeRaw = parseOption(1, 2);
      orderTypeRaw--;
      OrderType orderType = static_cast<OrderType>(orderTypeRaw);

      if (orderType == OrderType::MARKET) {
        requestMarketOrder(side, quantity);
        continue;
      }

      std::cout << pricePrompt;
      Price price = 0;
      std::cin >> price;

      std::cout << timeInForcePrompt << std::endl;
      int timeInForceRaw = parseOption(1, 4);
      timeInForceRaw--;
      TimeInForce timeInForce = static_cast<TimeInForce>(timeInForceRaw);

      requestLimitOrder(side, timeInForce, price, quantity);

    } else if (actionOption == 2) { // Cancel Order
      OrderId orderId = 0;
      std::cout << "\nOrder Id: ";
      if (!(std::cin >> orderId)) {
        std::cout << "Please enter a valid unsigned 64 bit integer"
                  << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max());
        return -1;
      }
      requestCancelOrder(orderId);
    } else { // Modify Order
      OrderId orderId = 0;
      std::cout << "\nOrder Id: ";
      if (!(std::cin >> orderId)) {
        std::cout << "Please enter a valid unsigned 64 bit integer"
                  << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max());
        return -1;
      }
      Quantity quantity = 0;
      std::cout << "\nNew Quantity: ";
      if (!(std::cin >> quantity)) {
        std::cout << "Please enter a valid unsigned 64 bit integer"
                  << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max());
        return -1;
      }
      requestModifyOrder(orderId, quantity);
    }
  }

  close(sock);

  return 0;
}
