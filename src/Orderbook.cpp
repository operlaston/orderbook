#include "Utils.h"
#include <Orderbook.h>
#include <optional>

Orderbook::Orderbook(uint64_t blockCount, uint32_t initialOrderMax)
    : m_memPool(blockCount, CACHE_LINE_SIZE), m_allocator(m_memPool),
      m_bids(MAX_PRICE + 1, Level(m_allocator)),
      m_asks(MAX_PRICE + 1, Level(m_allocator)),
      m_idToOrderPtr(initialOrderMax + 1, std::nullopt), // ids start at 1
      m_bidsBitset(MAX_PRICE + 1), m_asksBitset(MAX_PRICE + 1),
      m_bestBidPrice(0), m_bestAskPrice(0), m_numBids(0), m_numAsks(0) {}

NewOrderResult Orderbook::addOrder(Side side, Price price, Quantity quantity) {
  // Price 0 is reserved: PriceBitset uses 0 to mean "no price".
  if (price == 0 || price > MAX_PRICE || quantity == 0 ||
      (side != Side::Bid && side != Side::Ask)) {
    return {Status::BadRequest, 0};
  }

  Quantity remaining = side == Side::Bid ? matchBid(price, quantity)
                                         : matchAsk(price, quantity);
  if (remaining == 0) {
    return {Status::Success, 0};
  }

  OrderId id = m_currId++;
  restOrder(Order{id, remaining, price, side});
  return {Status::Success, id};
}

// Fills an incoming bid against the asks, best (lowest) price first and oldest
// first within a level, at the resting order's price. Returns the unfilled
// quantity.
Quantity Orderbook::matchBid(Price price, Quantity quantity) {
  while (quantity > 0 && m_numAsks > 0 && m_bestAskPrice <= price) {
    Level::iterator bestAskIt = m_asks[m_bestAskPrice].begin();
    Order &bestAsk = *bestAskIt;
    Quantity filled = std::min(quantity, bestAsk.getQuantity());
    quantity -= filled;
    if (bestAsk.getQuantity() == filled) {
      eraseOrder(bestAskIt); // may move m_bestAskPrice to the next level
    } else {
      bestAsk.setQuantity(bestAsk.getQuantity() - filled);
    }
  }
  return quantity;
}

// Mirror of matchBid: fills an incoming ask against the bids, highest first.
Quantity Orderbook::matchAsk(Price price, Quantity quantity) {
  while (quantity > 0 && m_numBids > 0 && m_bestBidPrice >= price) {
    Level::iterator bestBidIt = m_bids[m_bestBidPrice].begin();
    Order &bestBid = *bestBidIt;
    Quantity filled = std::min(quantity, bestBid.getQuantity());
    quantity -= filled;
    if (bestBid.getQuantity() == filled) {
      eraseOrder(bestBidIt);
    } else {
      bestBid.setQuantity(bestBid.getQuantity() - filled);
    }
  }
  return quantity;
}

// Returns the iterator for a resting order, or nullopt if the id is out of
// range or not resting. Ids come from the network, so check the range.
std::optional<Level::iterator> Orderbook::lookup(OrderId orderId) const {
  if (orderId >= m_idToOrderPtr.size()) {
    return std::nullopt;
  }
  return m_idToOrderPtr[orderId];
}

Status Orderbook::cancelOrder(OrderId orderId) {
  std::optional<Level::iterator> it = lookup(orderId);
  if (!it) {
    return Status::BadRequest;
  }
  eraseOrder(*it);
  return Status::Success;
}

Status Orderbook::modifyOrder(OrderId orderId, Quantity newQuantity) {
  std::optional<Level::iterator> it = lookup(orderId);
  if (!it) {
    return Status::BadRequest;
  }
  Order &order = **it;
  // Compare against the remaining quantity (after any partial fills).
  if (newQuantity == 0 || newQuantity > order.getQuantity()) {
    return Status::BadRequest;
  }
  // The price doesn't change, so the book can't become crossed and nothing
  // needs to be re-matched.
  order.setQuantity(newQuantity);
  return Status::Success;
}

std::optional<Price> Orderbook::bestBid() const {
  return m_numBids > 0 ? std::optional<Price>(m_bestBidPrice) : std::nullopt;
}

std::optional<Price> Orderbook::bestAsk() const {
  return m_numAsks > 0 ? std::optional<Price>(m_bestAskPrice) : std::nullopt;
}

const Order *Orderbook::findOrder(OrderId orderId) const {
  std::optional<Level::iterator> it = lookup(orderId);
  return it ? &**it : nullptr;
}

void Orderbook::restOrder(const Order &order) {
  bool isBid = order.getSide() == Side::Bid;

  Level &level = isBid ? m_bids[order.getPrice()] : m_asks[order.getPrice()];
  Level::iterator levelIt = level.insert(level.end(), order);

  // Ids are handed out in order, so a new id is at most one past the end.
  // Grow the map with push_back (amortized O(1)) once the initial size runs
  // out.
  OrderId id = order.getId();
  while (id >= m_idToOrderPtr.size()) {
    m_idToOrderPtr.emplace_back(std::nullopt);
  }
  m_idToOrderPtr[id] = levelIt;

  if (isBid) {
    if (m_numBids == 0 || m_bestBidPrice < order.getPrice()) {
      m_bestBidPrice = order.getPrice();
    }
    m_numBids++;
    m_bidsBitset.set(order.getPrice());
  } else {
    if (m_numAsks == 0 || m_bestAskPrice > order.getPrice()) {
      m_bestAskPrice = order.getPrice();
    }
    m_numAsks++;
    m_asksBitset.set(order.getPrice());
  }
}

// Removes a resting order from its level and from the id map.
//
// A level's bitset bit stays set while the level has any orders, so it is
// only cleared when this erase empties the level. If that level was the best
// price and the side still has orders, unsetAndGetPrev/Next clears the bit and
// finds the new best price in one scan. Otherwise clearing the bit is enough:
// either the best price didn't change, or the side is now empty and the best
// price is stale (guarded by m_numBids/m_numAsks).
void Orderbook::eraseOrder(Level::iterator orderIt) {
  const Order order = *orderIt;
  m_idToOrderPtr[order.getId()] = std::nullopt;
  Price price = order.getPrice();

  if (order.getSide() == Side::Bid) {
    m_bids[price].erase(orderIt);
    m_numBids--;

    if (m_bids[price].empty()) {
      if (price == m_bestBidPrice && m_numBids > 0) {
        m_bestBidPrice = m_bidsBitset.unsetAndGetPrev(price);
      } else {
        m_bidsBitset.unset(price);
      }
    }
  } else {
    m_asks[price].erase(orderIt);
    m_numAsks--;

    if (m_asks[price].empty()) {
      if (price == m_bestAskPrice && m_numAsks > 0) {
        m_bestAskPrice = m_asksBitset.unsetAndGetNext(price);
      } else {
        m_asksBitset.unset(price);
      }
    }
  }
}
