#include <Orderbook.h>
#include <chrono>
#include <iostream>

Orderbook::Orderbook() { currId = 0; }

void Orderbook::addOrder(Side side, Price price, Quantity quantity) {
  // make sure side is either sell or buy
  if (side != Side::SELL && side != Side::BUY) {
    return;
  }

  if (price <= 0 || quantity <= 0) {
    std::cout << "Error while adding order. Price and quantity must be "
                 "positive."
              << std::endl;
    return;
  }

  // get current time in nanoseconds since the epoch
  auto now = std::chrono::high_resolution_clock::now();
  auto duration = now.time_since_epoch();
  Timestamp currNs =
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

  // first check if the order can immediately be filled
  if (side == Side::BUY) {
    if (!m_asks.empty()) {
      PriceLevel &lowestAskPriceLevel = m_asks.begin()->second;
      Order &lowestAsk = lowestAskPriceLevel.front();
      Price lowestAskPrice = lowestAsk.getPrice();
      // check the lowest ask
      while (price >= lowestAskPrice) {

        Quantity lowestAskQuantity = lowestAsk.getQuantity();

        // fill the order and break out of loop if bid quantity does
        // not exceed lowest ask quantity
        if (lowestAskQuantity >= quantity) {
          lowestAsk.setQuantity(lowestAskQuantity - quantity);
          m_trades.emplace_back(currId, lowestAsk.getId(), lowestAskPrice,
                                quantity, currNs);
          quantity = 0;
          break;
        } else {
          // erase the ask from both the activeOrders map and the asks queue
          lowestAskPriceLevel.erase(lowestAskPriceLevel.begin());
          m_activeOrders.erase(lowestAsk.getId());

          // if the list for the price level is empty, erase the priceLevel
          // entry as well
          if (lowestAskPriceLevel.empty()) {
            m_asks.erase(m_asks.begin());
          }

          quantity -= lowestAskQuantity;
          m_trades.emplace_back(currId, lowestAsk.getId(), lowestAskPrice,
                                lowestAskQuantity, currNs);

          // break if there are no more asks
          if (m_asks.empty())
            break;

          lowestAsk = m_asks.begin()->second.front();
          lowestAskPrice = lowestAsk.getPrice();
        }
      }
    }

    if (quantity > 0) {
      // if the order isn't filled completely, add it to the list
      PriceLevel &priceLevel = m_bids[price];
      // add to end of PriceLevel list
      auto it = priceLevel.emplace(priceLevel.end(), currId, side, price,
                                   quantity, currNs);
      // add to activeOrders map
      m_activeOrders[currId] = it;
    }

  } else if (side == Side::SELL) {
    if (!m_bids.empty()) {
      PriceLevel &highestBidPriceLevel = m_bids.begin()->second;
      Order &highestBid = highestBidPriceLevel.front();
      Price highestBidPrice = highestBid.getPrice();
      // check the lowest bid
      while (price <= highestBidPrice) {
        Quantity highestBidQuantity = highestBid.getQuantity();

        // fill the order and break out of loop if bid quantity does
        // not exceed lowest ask quantity
        if (highestBidQuantity >= quantity) {
          highestBid.setQuantity(highestBidQuantity - quantity);
          m_trades.emplace_back(currId, highestBid.getId(), highestBidPrice,
                                quantity, currNs);
          quantity = 0;
          break;
        } else {
          // erase the order from both the activeOrders map and the bids queue
          highestBidPriceLevel.erase(highestBidPriceLevel.begin());
          m_activeOrders.erase(highestBid.getId());

          // if the list for the price level is empty, erase the priceLevel
          // entry as well
          if (highestBidPriceLevel.empty()) {
            m_bids.erase(m_bids.begin());
          }

          quantity -= highestBidQuantity;
          m_trades.emplace_back(currId, highestBid.getId(), highestBidPrice,
                                highestBidQuantity, currNs);

          if (m_bids.empty())
            break;

          highestBid = m_bids.begin()->second.front();
          highestBidPrice = highestBid.getPrice();
        }
      }
    }

    if (quantity > 0) {
      // if the order isn't filled completely, add it to the list
      PriceLevel &priceLevel = m_asks[price];
      auto it = priceLevel.emplace(priceLevel.end(), currId, side, price,
                                   quantity, currNs);
      m_activeOrders[currId] = it;
    }
  }

  currId++;
}

void Orderbook::cancelOrder(OrderId orderId) {
  if (!m_activeOrders.contains(orderId)) {
    // order does not exist
    std::cout << "Error while cancelling order.\norderId: " << orderId
              << " doesn't exist" << std::endl;
    return;
  }

  // remove the order from its queue and the activeOrders map
  auto it = m_activeOrders[orderId];
  Order order = *it;
  if (order.getSide() == Side::BUY) {
    m_bids[order.getPrice()].erase(it);
  } else if (order.getSide() == Side::SELL) {
    m_asks[order.getPrice()].erase(it);
  }
  m_activeOrders.erase(orderId);
}

void Orderbook::modifyOrder(OrderId orderId, Quantity newQuantity) {
  if (!m_activeOrders.contains(orderId)) {
    // invalid orderId
    std::cout << "Error while modifying order.\norderId: " << orderId
              << " doesn't exist" << std::endl;
    return;
  }

  // modify the order in place
  auto it = m_activeOrders[orderId];
  Quantity oldQuantity = it->getQuantity();
  if (oldQuantity < newQuantity || newQuantity <= 0) {
    // newQuantity must be less than the old quantity
    // and must be a positive number
    std::cout << "Error while modifying order: newQuantity must be a less "
                 "than the old quantity "
                 "and must be a positive number"
              << std::endl;
    return;
  }

  it->setQuantity(newQuantity);
}

void Orderbook::printOrderbook() {
  std::cout << "\nOrderbook State" << std::endl;
  std::cout << "Asks: " << std::endl;
  for (const auto &[priceLevel, orders] : m_asks) {
    for (const auto &order : orders) {
      order.display();
    }
  }

  std::cout << "Bids: " << std::endl;
  for (const auto &[priceLevel, orders] : m_bids) {
    for (const auto &order : orders) {
      order.display();
    }
  }
}

void Orderbook::printTrades() {
  std::cout << "\nCurrent Trades" << std::endl;
  for (const auto &trade : m_trades) {
    trade.display();
  }
}
