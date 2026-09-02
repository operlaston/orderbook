#pragma once

#include "ResponseTypes.h"
#include <BookSide.h>
#include <Order.h>
#include <ServerEngineContext.h>
#include <Trade.h>
#include <Utils.h>
#include <functional>
#include <list>

using PriceLevel = std::list<Order>;

class Orderbook {
private:
  BookSide<std::greater<Price>> m_bids;
  BookSide<std::less<Price>> m_asks;
  std::unordered_map<OrderId, PriceLevel::iterator> m_activeOrders;
  std::vector<Trade> m_trades;
  OrderId m_currId = 0; // this is 1 on the first trade
  ServerEngineContext &m_ctx;
  Quantity matchOrder(const Order &incomingOrder);
  bool canFill(const Order &incomingOrder);
  bool removeOrder(OrderId orderId);

  template <typename Compare>
  Quantity matchAgainst(BookSide<Compare> &restingBook,
                        const Order &incomingOrder, bool doFill);

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
  const BookSide<std::greater<Price>> &getBids() const { return m_bids; }
  const BookSide<std::less<Price>> &getAsks() const { return m_asks; }
  void run();
};
