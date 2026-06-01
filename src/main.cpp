#include <OrderType.h>
#include <Orderbook.h>
#include <Server.h>
#include <TimeInForce.h>

int main() {
  Orderbook orderbook;
  Server server;
  orderbook.addOrder(Side::SELL, 103.57, 200);
  orderbook.addOrder(Side::SELL, 101.04, 70);
  orderbook.addOrder(Side::SELL, 101.04, 60);
  orderbook.addOrder(Side::BUY, 100.56, 50);
  orderbook.addOrder(Side::BUY, 99.28, 50);
  orderbook.addOrder(Side::BUY, 102, 300, OrderType::LIMIT,
                     TimeInForce::FILL_OR_KILL);
  orderbook.printOrderbook();
  orderbook.printTrades();
  return 0;
}
