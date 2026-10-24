#pragma once

#include "Utils.h"
#include <cstdint>
#include <vector>
#include <list>
#include <Order.h>

const uint64_t MIN_PRICE = 1;
const uint64_t MAX_PRICE = 10000;

using Level = std::list<Order>;

class Orderbook {
public:
  Orderbook();
  void addOrder(Side side, Price price, Quantity quantity);
  const std::vector<Level>& getBids() const { return m_bids; }
  const std::vector<Level>& getAsks() const { return m_asks; }

private:
  std::vector<Level> m_bids;
  std::vector<Level> m_asks;
  OrderId m_currId{0};
};
