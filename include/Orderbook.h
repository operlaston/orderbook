#pragma once

#include "ResponseTypes.h"
#include <Order.h>
#include <ServerEngineContext.h>
#include <Trade.h>
#include <Utils.h>
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
  OrderId m_currId = 0; // this is 1 on the first trade
  ServerEngineContext &m_ctx;
  Quantity matchOrder(const Order &incomingOrder);
  bool canFill(const Order &incomingOrder);
  bool removeOrder(OrderId orderId);

public:
  // Orderbook();
  explicit Orderbook(ServerEngineContext &ctx);
  void addOrder(Response::NewOrder &res, Side side, Price price,
                Quantity quantity, OrderType orderType,
                TimeInForce timeInForce);
  void cancelOrder(Response::CancelOrder &res, OrderId orderId);
  void modifyOrder(Response::ModifyOrder &res, OrderId orderId,
                   Quantity newQuantity);
  void printOrderbook();
  void printTrades();
  const std::vector<Trade> &getTrades() const { return m_trades; }
  const std::map<Price, std::list<Order>, std::greater<Price>> &
  getBids() const {
    return m_bids;
  }
  const std::map<Price, std::list<Order>> &getAsks() const { return m_asks; }
  void run();
};
