#include "Utils.h"
#include <Orderbook.h>

Orderbook::Orderbook(uint64_t blockCount) :
  m_memPool(blockCount, CACHE_LINE_SIZE), // allocate a buffer to hold blockCount Order objects on the heap
  m_allocator(m_memPool),
  m_bids(MAX_PRICE + 1, Level(m_allocator)),
  m_asks(MAX_PRICE + 1, Level(m_allocator)),
  m_bidsBitset(MAX_PRICE + 1),
  m_asksBitset(MAX_PRICE + 1),
  m_bestBidPrice(0),
  m_bestAskPrice(0),
  m_numBids(0),
  m_numAsks(0)
{}

void Orderbook::addOrder(Side side, Price price, Quantity quantity) {
  Order order{m_currId, quantity, price, side};

  if (order.getPrice() > MAX_PRICE || order.getQuantity() == 0) {
    std::cout << "can't add orders with prices greater than " << MAX_PRICE << "\n";
    return;
  }

  // increm id once we know there's no errors with the input
  m_currId++;

  // check opposing asks on bid order
  if (order.getSide() == Side::Bid) {
    while (order.getQuantity() > 0 && m_numAsks > 0) {
      Order& bestAsk = m_asks[m_bestAskPrice].front();
      if (bestAsk.getPrice() > order.getPrice()) {
        break;
      }
      Quantity reduceQuantity = std::min(order.getQuantity(), bestAsk.getQuantity());
      order.setQuantity(order.getQuantity() - reduceQuantity);
      if (bestAsk.getQuantity() == reduceQuantity) {
        m_asks[m_bestAskPrice].erase(m_asks[m_bestAskPrice].begin());
        m_numAsks--;
      }
      else {
        bestAsk.setQuantity(bestAsk.getQuantity() - reduceQuantity);
      }

      if (m_asks[m_bestAskPrice].empty()) {
        if (m_numAsks > 0) {
          m_bestAskPrice = m_asksBitset.unsetAndGetNext(m_bestAskPrice);
        }
        else {
          m_asksBitset.unset(m_bestAskPrice);
        }
      }
    }
    if (order.getQuantity() > 0) {
      m_bids[order.getPrice()].push_back(order);
      if (m_numBids == 0 || m_bestBidPrice < order.getPrice()) {
        m_bestBidPrice = order.getPrice();
      }
      m_numBids++;
      m_bidsBitset.set(order.getPrice());
    }
  }

  // check opposing bids on ask order
  else if (order.getSide() == Side::Ask) {
    while (order.getQuantity() > 0 && m_numBids > 0) {
      Order& bestBid = m_bids[m_bestBidPrice].front();
      if (bestBid.getPrice() < order.getPrice()) {
        break;
      }
      Quantity reduceQuantity = std::min(order.getQuantity(), bestBid.getQuantity());
      order.setQuantity(order.getQuantity() - reduceQuantity);
      if (bestBid.getQuantity() == reduceQuantity) {
        m_bids[m_bestBidPrice].erase(m_bids[m_bestBidPrice].begin());
        m_numBids--;
      }
      else {
        bestBid.setQuantity(bestBid.getQuantity() - reduceQuantity);
      }

      if (m_bids[m_bestBidPrice].empty()) {
        if (m_numBids > 0) {
          m_bestBidPrice = m_bidsBitset.unsetAndGetPrev(m_bestBidPrice);
        }
        else {
          m_bidsBitset.unset(m_bestBidPrice);
        }
      }
    }
    if (order.getQuantity() > 0) {
      m_asks[order.getPrice()].push_back(order);
      if (m_numAsks == 0 || m_bestAskPrice > order.getPrice()) {
        m_bestAskPrice = order.getPrice();
      }
      m_numAsks++;
      m_asksBitset.set(order.getPrice());
    }
  }
}
