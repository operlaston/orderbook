#include "Utils.h"
#include <Orderbook.h>

Orderbook::Orderbook() :
  m_bids(MAX_PRICE + 1), m_asks(MAX_PRICE + 1) {}

void Orderbook::addOrder(Side side, Price price, Quantity quantity) {
  Order order{m_currId, quantity, price, side};

  if (order.getPrice() > MAX_PRICE || order.getQuantity() == 0) {
    std::cout << "can't add orders with prices greater than " << MAX_PRICE << "\n";
    return;
  }

  // increm id once we know there's no errors with the input
  m_currId++;

  if (order.getSide() == Side::Bid) {
    for (Price i = 1; i <= order.getPrice(); i++) {
      if (order.getQuantity() == 0) {
        break;
      }
      while (!m_asks[i].empty() && order.getQuantity() > 0) {
        auto firstAsk = m_asks[i].begin();
        Quantity reduceQuantity = std::min(order.getQuantity(), firstAsk->getQuantity());
        order.setQuantity(order.getQuantity() - reduceQuantity);
        if (firstAsk->getQuantity() == reduceQuantity) {
          m_asks[i].erase(m_asks[i].begin());
        }
        else {
          firstAsk->setQuantity(firstAsk->getQuantity() - reduceQuantity);
        }
      }
    }
    if (order.getQuantity() > 0) {
      m_bids[order.getPrice()].push_back(order);
    }
  }
  else if (order.getSide() == Side::Ask) {
    for (Price i = MAX_PRICE; i >= order.getPrice(); i--) {
      if (order.getQuantity() == 0) {
        break;
      }
      while (!m_bids[i].empty() && order.getQuantity() > 0) {
        auto firstBid = m_bids[i].begin();
        Quantity reduceQuantity = std::min(order.getQuantity(), firstBid->getQuantity());
        order.setQuantity(order.getQuantity() - reduceQuantity);
        if (firstBid->getQuantity() == reduceQuantity) {
          m_bids[i].erase(m_bids[i].begin());
        }
        else {
          firstBid->setQuantity(firstBid->getQuantity() - reduceQuantity);
        }
      }
    }
    if (order.getQuantity() > 0) {
      m_asks[order.getPrice()].push_back(order);
    }
  }
}
