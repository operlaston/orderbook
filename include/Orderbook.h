#pragma once

#include "BlockAllocator.h"
#include "Utils.h"
#include <cstdint>
#include <list>
#include <vector>
#include <Order.h>
#include <PriceBitset.h>

using Level = std::list<Order, BlockAllocator<Order>>;

class Orderbook {
public:
  explicit Orderbook(uint64_t blockCount); // # of blocks to allocate
  void addOrder(Side side, Price price, Quantity quantity);
  const std::vector<Level>& getBids() const { return m_bids; }
  const std::vector<Level>& getAsks() const { return m_asks; }
  const PriceBitset& getBidsBitset() const { return m_bidsBitset; }
  const PriceBitset& getAsksBitset() const { return m_asksBitset; }

private:
  FixedPool m_memPool;
  BlockAllocator<Order> m_allocator;
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
