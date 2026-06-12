#include <MessageType.h>
#include <OrderType.h>
#include <Side.h>
#include <TimeInForce.h>
#include <Utils.h>
#include <arpa/inet.h>
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int sock = -1;

void requestNewOrder(Side side, OrderType orderType, TimeInForce timeInForce,
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
  ::write(sock, buf.data(), 20);
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
  requestNewOrder(Side::SELL, OrderType::LIMIT, TimeInForce::GOOD_TILL_CANCEL,
                  103.57, 200);
  requestNewOrder(Side::SELL, OrderType::LIMIT, TimeInForce::GOOD_TILL_CANCEL,
                  101.04, 70);
  requestNewOrder(Side::SELL, OrderType::LIMIT, TimeInForce::GOOD_TILL_CANCEL,
                  101.04, 60);
  requestNewOrder(Side::BUY, OrderType::LIMIT, TimeInForce::GOOD_TILL_CANCEL,
                  100.56, 50);
  requestNewOrder(Side::BUY, OrderType::LIMIT, TimeInForce::GOOD_TILL_CANCEL,
                  99.28, 50);
  requestNewOrder(Side::BUY, OrderType::LIMIT, TimeInForce::FILL_OR_KILL, 102,
                  300);

  // sleep(1000);
  close(sock);

  return 0;
}
