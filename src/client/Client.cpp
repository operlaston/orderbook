#include <MessageType.h>
#include <OrderType.h>
#include <OrderbookClient.h>
#include <Side.h>
#include <TimeInForce.h>
#include <Utils.h>
#include <arpa/inet.h>
#include <cstring>
#include <endian.h>
#include <iostream>
#include <limits>
#include <sys/socket.h>
#include <unistd.h>

int main() {
  OrderbookClient client{8080};

  while (true) {
    std::string actionsText = "=======Actions=======\n"
                              "(1) New Order\n"
                              "(2) Cancel Order\n"
                              "(3) Modify Order\n";

    std::cout << actionsText << std::endl;

    int actionOption = OrderbookClient::parseOption(1, 3);

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
      int sideRaw = OrderbookClient::parseOption(1, 2);
      sideRaw--;
      Side side = static_cast<Side>(sideRaw);

      std::cout << quantityPrompt;
      Quantity quantity = 0;
      std::cin >> quantity;

      std::cout << orderTypePrompt << std::endl;
      int orderTypeRaw = OrderbookClient::parseOption(1, 2);
      orderTypeRaw--;
      OrderType orderType = static_cast<OrderType>(orderTypeRaw);

      if (orderType == OrderType::MARKET) {
        client.requestMarketOrder(side, quantity);
        continue;
      }

      std::cout << pricePrompt;
      Price price = 0;
      std::cin >> price;

      std::cout << timeInForcePrompt << std::endl;
      int timeInForceRaw = OrderbookClient::parseOption(1, 4);
      timeInForceRaw--;
      TimeInForce timeInForce = static_cast<TimeInForce>(timeInForceRaw);

      client.requestLimitOrder(side, timeInForce, price, quantity);

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
      client.requestCancelOrder(orderId);
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
      client.requestModifyOrder(orderId, quantity);
    }
  }

  return 0;
}
