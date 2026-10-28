#pragma once

#include "BlockAllocator.h"
#include "Utils.h"
#include <Order.h>
#include <PriceBitset.h>
#include <cstdint>
#include <list>
#include <optional>
#include <vector>

// One price level: resting orders at a single price in time priority (front is
// oldest). List nodes come from the Orderbook's FixedPool.
using Level = std::list<Order, BlockAllocator<Order>>;

// A std::list node holds the Order plus prev/next pointers. It must fit in one
// pool block, or every allocation falls back to the heap.
static_assert(sizeof(Order) + 2 * sizeof(void *) <= CACHE_LINE_SIZE,
              "list node no longer fits in a FixedPool block");

// Single-threaded limit order book. All orders are limit orders that rest until
// filled or cancelled (GTC).
//
// Layout:
// - m_bids / m_asks are indexed directly by price (0..MAX_PRICE), one Level per
//   price. Price 0 is never used.
// - m_bidsBitset / m_asksBitset have bit p set iff level p is non-empty, which
//   lets us find the next best price quickly when the best level empties.
// - m_idToOrderPtr is indexed by order id. It holds the order's list iterator
//   while the order rests and nullopt otherwise. Slot 0 is always nullopt.
class Orderbook {
public:
  static constexpr Price MAX_PRICE = 100000;

  // blockCount: number of list nodes to preallocate in the pool (more orders
  //   than this still work, they just come from the heap).
  // initialOrderMax: initial id-map size. It grows as needed.
  explicit Orderbook(uint64_t blockCount, uint32_t initialOrderMax);

  NewOrderResult addOrder(Side side, Price price, Quantity quantity);
  Status cancelOrder(OrderId orderId);
  // Lowers a resting order's quantity in place. The order keeps its place in
  // its level's queue. Increasing or setting it to 0 is rejected.
  Status modifyOrder(OrderId orderId, Quantity newQuantity);

  std::optional<Price> bestBid() const;
  std::optional<Price> bestAsk() const;
  // The resting order with this id, or nullptr if it isn't resting.
  const Order *findOrder(OrderId orderId) const;

  const std::vector<Level> &getBids() const { return m_bids; }
  const std::vector<Level> &getAsks() const { return m_asks; }
  const PriceBitset &getBidsBitset() const { return m_bidsBitset; }
  const PriceBitset &getAsksBitset() const { return m_asksBitset; }
  const std::vector<std::optional<Level::iterator>> &getIdMap() const {
    return m_idToOrderPtr;
  }

private:
  FixedPool m_memPool;
  BlockAllocator<Order> m_allocator;
  std::vector<Level> m_bids;
  std::vector<Level> m_asks;
  std::vector<std::optional<Level::iterator>> m_idToOrderPtr;
  PriceBitset m_bidsBitset;
  PriceBitset m_asksBitset;
  // Only meaningful while the matching count is non-zero. When a side empties
  // these go stale, and the next resting order on that side resets them.
  Price m_bestBidPrice;
  Price m_bestAskPrice;
  uint64_t m_numBids;
  uint64_t m_numAsks;

  // Ids are used up only when an order rests. Fully filled orders never need
  // to be looked up, so the id map stays dense.
  OrderId m_currId{1};

  Quantity matchBid(Price price, Quantity quantity);
  Quantity matchAsk(Price price, Quantity quantity);
  void restOrder(const Order &order);
  void eraseOrder(Level::iterator orderIt);
  std::optional<Level::iterator> lookup(OrderId orderId) const;
};
