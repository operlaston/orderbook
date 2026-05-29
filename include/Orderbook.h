#pragma once

#include <Order.h>
#include <Trade.h>
#include <Using.h>
#include <assert.h>
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

public:
  Orderbook();
  bool addOrder(Side side, Price price, Quantity quantity);
  bool cancelOrder(OrderId orderId);
  bool modifyOrder(OrderId orderId, Quantity newQuantity);
  void printOrderbook();
  void printTrades();
};
