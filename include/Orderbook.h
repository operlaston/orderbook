#pragma once

#include <Order.h>
#include <Trade.h>
#include <Using.h>
#include <functional>
#include <list>
#include <map>

using PriceLevel = std::list<Order>;

class Orderbook {
private:
  std::map<Price, std::list<Order>, std::greater<Price>> m_bids;
  std::map<Price, std::list<Order>> m_asks;
  std::unordered_map<OrderId, std::list<Order>::iterator> m_activeOrders;
  std::vector<Trade> m_trades;
  OrderId currId;
  Quantity matchOrder(const Order &incomingOrder);
  bool canFill(const Order &incomingOrder);

public:
  Orderbook();
  void addOrder(Side side, Price price, Quantity quantity,
                OrderType orderType = OrderType::LIMIT,
                TimeInForce timeInForce = TimeInForce::GOOD_TILL_CANCEL);
  void removeOrder(OrderId orderId);
  void modifyOrder(OrderId orderId, Quantity newQuantity);
  void printOrderbook();
  void printTrades();
};
