#include <OrderType.h>
#include <Orderbook.h>
#include <TimeInForce.h>

int main() {
  Orderbook orderbook;
  orderbook.addOrder(Side::BUY, 100.56, 50);
  // orderbook.printOrderbook();
  // orderbook.printTrades();
  orderbook.addOrder(Side::SELL, 103.57, 200);
  // orderbook.printOrderbook();
  // orderbook.printTrades();
  orderbook.addOrder(Side::SELL, 101.04, 70);
  // orderbook.printOrderbook();
  // orderbook.printTrades();
  // orderbook.addOrder(Side::SELL, 101.04, 60);
  // orderbook.printOrderbook();
  // orderbook.printTrades();
  orderbook.addOrder(Side::BUY, 99.28, 50);
  // orderbook.printOrderbook();
  // orderbook.printTrades();
  orderbook.addOrder(Side::BUY, 102.37, 80, OrderType::LIMIT,
                     TimeInForce::FILL_OR_KILL);
  // orderbook.modifyOrder(4, 5);
  orderbook.printOrderbook();
  orderbook.printTrades();
  return 0;
}
