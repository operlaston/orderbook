#pragma once

#include "Utils.h"
#include <cstdint>
#include <vector>
#include <list>
#include <Order.h>
#include <PriceBitset.h>

using Level = std::list<Order>;

class Orderbook {
public:
  Orderbook();
  void addOrder(Side side, Price price, Quantity quantity);
  const std::vector<Level>& getBids() const { return m_bids; }
  const std::vector<Level>& getAsks() const { return m_asks; }
  const PriceBitset& getBidsBitset() const { return m_bidsBitset; }
  const PriceBitset& getAsksBitset() const { return m_asksBitset; }

private:
  std::vector<Level> m_bids;
  std::vector<Level> m_asks;
  PriceBitset m_bidsBitset;
  PriceBitset m_asksBitset;
  uint32_t m_bestBidPrice;
  uint32_t m_bestAskPrice;
  uint64_t m_numBids;
  uint64_t m_numAsks;

  OrderId m_currId{0};
  static constexpr uint64_t MAX_PRICE = 100000;
};
