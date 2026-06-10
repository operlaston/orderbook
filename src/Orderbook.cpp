#include "RequestTypes.h"
#include <Orderbook.h>
#include <ServerEngineContext.h>
#include <chrono>
#include <iostream>
#include <optional>
#include <variant>

// Orderbook::Orderbook() {}

Orderbook::Orderbook(ServerEngineContext &ctx) : m_ctx(ctx) {}

// returns the remaining quantity of the order
Quantity Orderbook::matchOrder(const Order &incomingOrder) {
  Side side = incomingOrder.getSide();
  Price price = incomingOrder.getPrice();
  Quantity quantity = incomingOrder.getQuantity();
  Timestamp timestamp = incomingOrder.getTimestamp();

  if (incomingOrder.getTimeInForce() == TimeInForce::FILL_OR_KILL &&
      !canFill(incomingOrder)) {
    return quantity;
  }

  if (side == Side::BUY && !m_asks.empty()) {
    auto lowestAskIt = m_asks.begin()->second.begin();
    Price lowestAskPrice = lowestAskIt->getPrice();
    // check the lowest ask
    while (price >= lowestAskPrice) {

      Quantity lowestAskQuantity = lowestAskIt->getQuantity();

      // fill the order and break out of loop if bid quantity does
      // not exceed lowest ask quantity
      if (lowestAskQuantity > quantity) {
        lowestAskIt->setQuantity(lowestAskQuantity - quantity);
        m_trades.emplace_back(m_currId, lowestAskIt->getId(), lowestAskPrice,
                              quantity, timestamp);
        quantity = 0;
        break;
      } else {
        removeOrder(lowestAskIt->getId());
        quantity -= lowestAskQuantity;
        m_trades.emplace_back(m_currId, lowestAskIt->getId(), lowestAskPrice,
                              lowestAskQuantity, timestamp);
        if (m_asks.empty())
          break;

        lowestAskIt = m_asks.begin()->second.begin();
        lowestAskPrice = lowestAskIt->getPrice();
      }
    }
  } else if (side == Side::SELL && !m_bids.empty()) {
    auto highestBidIt = m_bids.begin()->second.begin();
    Price highestBidPrice = highestBidIt->getPrice();
    // check the highest bid
    while (price <= highestBidPrice) {
      Quantity highestBidQuantity = highestBidIt->getQuantity();

      // fill the order and break out of loop if bid quantity does
      // not exceed lowest ask quantity
      if (highestBidQuantity > quantity) {
        highestBidIt->setQuantity(highestBidQuantity - quantity);
        m_trades.emplace_back(m_currId, highestBidIt->getId(), highestBidPrice,
                              quantity, timestamp);
        quantity = 0;
        break;
      } else {
        removeOrder(highestBidIt->getId());
        quantity -= highestBidQuantity;
        m_trades.emplace_back(m_currId, highestBidIt->getId(), highestBidPrice,
                              highestBidQuantity, timestamp);

        if (m_bids.empty())
          break;

        highestBidIt = m_bids.begin()->second.begin();
        highestBidPrice = highestBidIt->getPrice();
      }
    }
  }
  return quantity;
}

bool Orderbook::canFill(const Order &incomingOrder) {
  Side side = incomingOrder.getSide();
  Price price = incomingOrder.getPrice();
  Quantity quantity = incomingOrder.getQuantity();

  if (side == Side::BUY && !m_asks.empty()) {
    auto currPriceLevel = m_asks.begin();
    PriceLevel *currPriceList = &currPriceLevel->second;
    auto currPriceListIter = currPriceList->begin();
    // check the lowest ask
    while (price >= currPriceListIter->getPrice()) {

      Quantity lowestAskQuantity = currPriceListIter->getQuantity();

      // fill the order and break out of loop if bid quantity does
      // not exceed lowest ask quantity
      if (lowestAskQuantity >= quantity) {
        quantity = 0;
        break;
      } else {
        quantity -= lowestAskQuantity;
        currPriceListIter++;
        if (currPriceListIter == currPriceList->end()) {
          currPriceLevel++;
          if (currPriceLevel == m_asks.end()) {
            break;
          }
          currPriceList = &currPriceLevel->second;
          currPriceListIter = currPriceList->begin();
        }
      }
    }
  } else if (side == Side::SELL && !m_bids.empty()) {
    auto currPriceLevel = m_bids.begin();
    PriceLevel *currPriceList = &currPriceLevel->second;
    auto currPriceListIter = currPriceList->begin();
    // check the lowest bid
    while (price <= currPriceListIter->getPrice()) {
      Quantity highestBidQuantity = currPriceListIter->getQuantity();

      if (highestBidQuantity >= quantity) {
        quantity = 0;
        break;
      } else {
        quantity -= highestBidQuantity;
        currPriceListIter++;
        if (currPriceListIter == currPriceList->end()) {
          currPriceLevel++;
          if (currPriceLevel == m_asks.end()) {
            break;
          }
          currPriceList = &currPriceLevel->second;
          currPriceListIter = currPriceList->begin();
        }
      }
    }
  }

  return quantity == 0;
}

void Orderbook::addOrder(Side side, Price price, Quantity quantity,
                         OrderType orderType, TimeInForce timeInForce) {

  if (side != Side::SELL && side != Side::BUY) {
    return;
  }

  if (orderType == OrderType::MARKET) {
    timeInForce = TimeInForce::IMMEDIATE_OR_CANCEL;
    if (side == Side::BUY) {
      if (m_asks.empty())
        return;
      price = std::prev(m_asks.end())->first;
    } else {
      if (m_bids.empty())
        return;
      price = std::prev(m_bids.end())->first;
    }
  }

  if (price <= 0 || quantity <= 0) {
    std::cout << "Error while adding order. Price and quantity must be "
                 "positive."
              << std::endl;
    return;
  }

  Timestamp timestamp = std::chrono::system_clock::now();

  Order order(m_currId, side, price, quantity, timestamp, orderType,
              timeInForce);

  Quantity remainingQuantity = matchOrder(order);

  // we do not add the order to the orderbook if immediate or cancel
  // or fill or kill
  if (timeInForce == TimeInForce::IMMEDIATE_OR_CANCEL ||
      timeInForce == TimeInForce::FILL_OR_KILL) {
    return;
  }

  if (side == Side::BUY) {

    if (remainingQuantity > 0) {
      PriceLevel &priceLevel = m_bids[price];
      priceLevel.push_back(order);
      m_activeOrders[m_currId] = std::prev(priceLevel.end());
      // auto it = priceLevel.emplace(priceLevel.end(), m_currId, side, price,
      //                              remainingQuantity, timestamp, orderType,
      //                              timeInForce);
      // m_activeOrders[m_currId] = it;
    }

  } else if (side == Side::SELL) {

    if (remainingQuantity > 0) {
      PriceLevel &priceLevel = m_asks[price];
      priceLevel.push_back(order);
      m_activeOrders[m_currId] = std::prev(priceLevel.end());
      // auto it = priceLevel.emplace(priceLevel.end(), m_currId, side, price,
      //                              remainingQuantity, timestamp, orderType,
      //                              timeInForce);
      // m_activeOrders[m_currId] = it;
    }
  }

  m_currId++;
}

void Orderbook::removeOrder(OrderId orderId) {
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
    // if the list for the price level is empty, then erase the entry list
    if (m_bids[order.getPrice()].empty()) {
      m_bids.erase(order.getPrice());
    }
  } else if (order.getSide() == Side::SELL) {
    m_asks[order.getPrice()].erase(it);
    // if the list for the price level is empty, then erase the entry list
    if (m_asks[order.getPrice()].empty()) {
      m_asks.erase(order.getPrice());
    }
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
  std::cout << "\nAsks: " << std::endl;
  for (const auto &[priceLevel, orders] : m_asks) {
    for (const auto &order : orders) {
      order.display();
    }
  }

  std::cout << "\nBids: " << std::endl;
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

void Orderbook::run() {
  while (true) {
    std::optional<ClientRequest> req = m_ctx.incomingRequests.pop();
    if (req != std::nullopt) {
      switch (req->index()) {
        // didn't feel like making an enum or consts for this
      case 0: {
        Request::NewOrder order = std::get<0>(*req);
        addOrder(order.side, order.price, order.quantity);
        break;
      }
      case 1: {
        Request::CancelOrder order = std::get<1>(*req);
        removeOrder(order.orderId);
        break;
      }
      case 2: {
        Request::ModifyOrder order = std::get<2>(*req);
        modifyOrder(order.orderId, order.newQuantity);
        break;
      }
      }
    }
  }
}
