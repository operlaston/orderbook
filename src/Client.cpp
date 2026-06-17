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
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int sock = -1;

void printStatus(uint8_t status) {
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
  std::cout << "New Order" << std::endl;
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
  ::write(sock, buf.data(), buf.size());

  // get response
  std::array<uint8_t, 9> responseBuf;
  ::read(sock, responseBuf.data(), responseBuf.size());

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
  ::write(sock, buf.data(), buf.size());

  uint8_t res;
  ::read(sock, &res, 1);
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
  ::write(sock, buf.data(), buf.size());

  uint8_t res;
  ::read(sock, &res, 1);
  printStatus(res);
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

  std::vector<uint8_t> buf = {
      0,
  };
  requestLimitOrder(Side::SELL, TimeInForce::GOOD_TILL_CANCEL, 103.57, 200);
  requestLimitOrder(Side::SELL, TimeInForce::GOOD_TILL_CANCEL, 101.04, 70);
  requestLimitOrder(Side::SELL, TimeInForce::GOOD_TILL_CANCEL, 101.04, 60);
  OrderId someCancelledId =
      requestLimitOrder(Side::BUY, TimeInForce::GOOD_TILL_CANCEL, 100.56, 50);
  OrderId someModifiedId =
      requestLimitOrder(Side::BUY, TimeInForce::GOOD_TILL_CANCEL, 99.28, 50);
  requestMarketOrder(Side::BUY, 500);

  requestCancelOrder(someCancelledId);

  requestModifyOrder(someModifiedId, 30);

  // sleep(1000);
  close(sock);

  return 0;
}
