#include "Protocol.h"
#include "Utils.h"
#include <Orderbook.h>
#include <optional>
#include <utility>

Orderbook::Orderbook(uint64_t blockCount, uint32_t initialOrderMax) :
  m_memPool(blockCount, CACHE_LINE_SIZE), // allocate a buffer to hold blockCount Order objects on the heap
  m_allocator(m_memPool),
  m_bids(MAX_PRICE + 1, Level(m_allocator)),
  m_asks(MAX_PRICE + 1, Level(m_allocator)),
  m_initialOrderMax(initialOrderMax),
  m_idToOrderPtr(initialOrderMax + 1, std::nullopt), // +1 because we start at id 1
  m_bidsBitset(MAX_PRICE + 1),
  m_asksBitset(MAX_PRICE + 1),
  m_bestBidPrice(0),
  m_bestAskPrice(0),
  m_numBids(0),
  m_numAsks(0)
{}

std::pair<OrderId, Protocol::Status> Orderbook::addOrder(Side side, Price price, Quantity quantity) {
  Order order{m_currId, quantity, price, side};

  if (order.getPrice() > MAX_PRICE || order.getPrice() == 0) {
    std::cout << "can't add - price must be between in the range [1, " << MAX_PRICE << "]\n";
    return {0, Protocol::Status::BadRequest};
  }

  if (side != Side::Bid && side != Side::Ask) {
    std::cout << "can't add - side must be bid or ask\n";
    return {0, Protocol::Status::BadRequest};
  }

  if (quantity == 0) {
    std::cout << "can't add - quantity must be nonzero\n";
    return {0, Protocol::Status::BadRequest};
  }


  // check opposing asks on bid order
  if (order.getSide() == Side::Bid) {
    while (order.getQuantity() > 0 && m_numAsks > 0) {
      Level::iterator bestAskIt = m_asks[m_bestAskPrice].begin();
      Order& bestAsk = *bestAskIt;
      if (bestAsk.getPrice() > order.getPrice()) {
        break;
      }
      Quantity reduceQuantity = std::min(order.getQuantity(), bestAsk.getQuantity());
      order.setQuantity(order.getQuantity() - reduceQuantity);
      if (bestAsk.getQuantity() == reduceQuantity) {
        eraseOrder(bestAskIt);
      }
      else {
        bestAsk.setQuantity(bestAsk.getQuantity() - reduceQuantity);
      }
    }
  }

  // check opposing bids on ask order
  else if (order.getSide() == Side::Ask) {
    while (order.getQuantity() > 0 && m_numBids > 0) {
      Level::iterator bestBidIt = m_bids[m_bestBidPrice].begin();
      Order& bestBid = *bestBidIt;
      if (bestBid.getPrice() < order.getPrice()) {
        break;
      }
      Quantity reduceQuantity = std::min(order.getQuantity(), bestBid.getQuantity());
      order.setQuantity(order.getQuantity() - reduceQuantity);
      if (bestBid.getQuantity() == reduceQuantity) {
        eraseOrder(bestBidIt);
      }
      else {
        bestBid.setQuantity(bestBid.getQuantity() - reduceQuantity);
      }
    }
  }

  // if there is any remaining quantity, the order must rest
  if (order.getQuantity() > 0) {
    restOrder(order);
    return {order.getId(), Protocol::Status::Success};
  }
  return {0, Protocol::Status::Success};
}

Protocol::Status Orderbook::cancelOrder(OrderId orderId) {
  if (orderId >= m_idToOrderPtr.size()) {
    std::cout << "invalid order id";
    return Protocol::Status::BadRequest;
  }
  std::optional<Level::iterator> orderItOpt = m_idToOrderPtr[orderId];
  if (!orderItOpt.has_value()) {
    std::cout << "order id is no longer valid" << std::endl;
    return Protocol::Status::BadRequest;
  }
  auto orderIt = *orderItOpt;
  eraseOrder(orderIt);
  m_idToOrderPtr[orderId] = std::nullopt;
  return Protocol::Status::Success;
}

Protocol::Status Orderbook::modifyOrder(OrderId orderId, Quantity newQuantity) {
  if (orderId >= m_idToOrderPtr.size()) {
    std::cout << "invalid order id";
    return Protocol::Status::BadRequest;
  }
  std::optional<Level::iterator> orderItOpt = m_idToOrderPtr[orderId];
  if (!orderItOpt.has_value()) {
    std::cout << "order id is no longer valid" << std::endl;
    return Protocol::Status::BadRequest;
  }
  Order& order = **orderItOpt;
  if (order.getQuantity() < newQuantity || newQuantity == 0) {
    std::cout << "new quantity must be nonzero and <= old quantity" << std::endl;
    return Protocol::Status::BadRequest;
  }
  order.setQuantity(newQuantity);
  return Protocol::Status::Success;
}

void Orderbook::restOrder(const Order& order) {
  bool isBid = order.getSide() == Side::Bid;

  Level &pl = isBid ? m_bids[order.getPrice()] : m_asks[order.getPrice()];
  Level::iterator levelIt = pl.insert(pl.end(), order);
  if (order.getId() <= m_initialOrderMax) {
    m_idToOrderPtr[order.getId()] = levelIt;
  }
  else {
    // fallback to push_back when we have run out of space
    m_idToOrderPtr.push_back(levelIt);
  }

  if (isBid) {
    if (m_numBids == 0 || m_bestBidPrice < order.getPrice()) {
      m_bestBidPrice = order.getPrice();
    }
    m_numBids++;
    m_bidsBitset.set(order.getPrice());
  }
  else {
    if (m_numAsks == 0 || m_bestAskPrice > order.getPrice()) {
      m_bestAskPrice = order.getPrice();
    }
    m_numAsks++;
    m_asksBitset.set(order.getPrice());
  }

  // we increment the global id counter only after resting an order 
  m_currId++;
}

void Orderbook::eraseOrder(Level::iterator orderIt) {
  const Order order = *orderIt;
  const Price price = order.getPrice();
  m_idToOrderPtr[order.getId()] = std::nullopt;
  if (order.getSide() == Side::Bid) {
    m_bids[price].erase(orderIt);
    m_numBids--;

    if (m_bids[price].empty()) {
      if (price == m_bestBidPrice && m_numBids > 0) {
        m_bestBidPrice = m_bidsBitset.unsetAndGetPrev(price);
      }
      else {
        m_bidsBitset.unset(price);
      }
    }
  }
  else {
    m_asks[price].erase(orderIt);
    m_numAsks--;

    if (m_asks[price].empty()) {
      if (price == m_bestAskPrice && m_numAsks > 0) {
        m_bestAskPrice = m_asksBitset.unsetAndGetNext(price);
      }
      else {
        m_asksBitset.unset(price);
      }
    }
  }
}

