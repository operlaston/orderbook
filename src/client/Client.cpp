#include <OrderbookClient.h>
#include <Utils.h>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

namespace {

// Reads an unsigned number in [low, high], asking again on bad input. Exits on
// end of input.
uint64_t readNumber(const std::string &prompt, uint64_t low, uint64_t high) {
  while (true) {
    std::cout << prompt << std::flush;
    uint64_t value;
    if (std::cin >> value && value >= low && value <= high) {
      return value;
    }
    if (std::cin.eof()) {
      std::cout << std::endl;
      std::exit(0);
    }
    std::cout << "Please enter a number from " << low << " to " << high
              << std::endl;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }
}

constexpr uint64_t U64_MAX = std::numeric_limits<uint64_t>::max();

} // namespace

int main(int argc, char **argv) {
  uint16_t port = argc > 1 ? static_cast<uint16_t>(std::atoi(argv[1])) : 8080;

  try {
    OrderbookClient client{port};
    std::cout << "Connected to server on port " << port << std::endl;

    while (true) {
      std::cout << "\n=======Actions=======\n"
                   "(1) New Order\n"
                   "(2) Cancel Order\n"
                   "(3) Modify Order\n";
      uint64_t action = readNumber("> ", 1, 3);

      if (action == 1) {
        uint64_t sideOption = readNumber("Side (1) Buy (2) Sell: ", 1, 2);
        Side side = sideOption == 1 ? Side::Bid : Side::Ask;
        Price price = static_cast<Price>(
            readNumber("Price: ", 0, std::numeric_limits<Price>::max()));
        Quantity quantity = readNumber("Quantity: ", 0, U64_MAX);

        NewOrderResult res = client.requestNewOrder(side, price, quantity);
        std::cout << OrderbookClient::statusName(res.status);
        if (res.orderId != 0) {
          std::cout << ", resting with order id " << res.orderId;
        } else if (res.status == Status::Success) {
          std::cout << ", filled in full";
        }
        std::cout << std::endl;
      } else if (action == 2) {
        OrderId id = readNumber("Order id: ", 0, U64_MAX);
        std::cout << OrderbookClient::statusName(client.requestCancelOrder(id))
                  << std::endl;
      } else {
        OrderId id = readNumber("Order id: ", 0, U64_MAX);
        Quantity quantity = readNumber("New quantity: ", 0, U64_MAX);
        std::cout << OrderbookClient::statusName(
                         client.requestModifyOrder(id, quantity))
                  << std::endl;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }
}
