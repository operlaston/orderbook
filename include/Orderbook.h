#pragma once

#include "BlockAllocator.h"
#include "Protocol.h"
#include "Utils.h"
#include <cstdint>
#include <list>
#include <optional>
#include <vector>
#include <Order.h>
#include <PriceBitset.h>

using Level = std::list<Order, BlockAllocator<Order>>;

class Orderbook {
public:
  static constexpr uint64_t MAX_PRICE = 100000;

  explicit Orderbook(uint64_t blockCount, uint32_t initialOrderMax); // # of blocks to allocate
  std::pair<OrderId, Protocol::Status> addOrder(Side side, Price price, Quantity quantity);
  Protocol::Status cancelOrder(OrderId orderId);
  Protocol::Status modifyOrder(OrderId orderId, Quantity newQuantity);
  const std::vector<Level>& getBids() const { return m_bids; }
  const std::vector<Level>& getAsks() const { return m_asks; }
  const PriceBitset& getBidsBitset() const { return m_bidsBitset; }
  const PriceBitset& getAsksBitset() const { return m_asksBitset; }
  const std::vector<std::optional<Level::iterator>>& getIdMap() const { return m_idToOrderPtr; }

private:
  FixedPool m_memPool;
  BlockAllocator<Order> m_allocator;
  std::vector<Level> m_bids;
  std::vector<Level> m_asks;
  const uint32_t m_initialOrderMax;
  std::vector<std::optional<Level::iterator>> m_idToOrderPtr;
  PriceBitset m_bidsBitset;
  PriceBitset m_asksBitset;
  uint32_t m_bestBidPrice;
  uint32_t m_bestAskPrice;
  uint64_t m_numBids;
  uint64_t m_numAsks;
  OrderId m_currId{1};

  void restOrder(const Order& order);
  void eraseOrder(Level::iterator orderIt);
};
