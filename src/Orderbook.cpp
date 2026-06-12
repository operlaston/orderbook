#include "GlobalConsts.h"
#include "RequestTypes.h"
#include "ResponseTypes.h"
#include <Orderbook.h>
#include <ServerEngineContext.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
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

  if (incomingOrder.getTimeInForce() == TimeInForce::FILL_OR_KILL) {
    std::cout << "\nFILL OR KILL ORDERED" << std::endl;
  }
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

void Orderbook::addOrder(Response::NewOrder &res, Side side, Price price,
                         Quantity quantity, OrderType orderType,
                         TimeInForce timeInForce) {

  if (side != Side::SELL && side != Side::BUY) {
    res.status = ResponseStatus::BAD_REQUEST;
    return;
  }

  if (orderType == OrderType::MARKET) {
    timeInForce = TimeInForce::IMMEDIATE_OR_CANCEL;
    if (side == Side::BUY) {
      if (m_asks.empty()) {
        res.status = ResponseStatus::CANT_FILL;
        return;
      }
      price = std::prev(m_asks.end())->first;
    } else {
      if (m_bids.empty()) {
        res.status = ResponseStatus::CANT_FILL;
        return;
      }
      price = std::prev(m_bids.end())->first;
    }
  }

  if (price <= 0 || quantity <= 0) {
    std::cout << "Error while adding order. Price and quantity must be "
                 "positive."
              << std::endl;
    res.status = ResponseStatus::BAD_REQUEST;
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
    if (quantity == remainingQuantity) {
      res.status = ResponseStatus::CANT_FILL;
    } else if (remainingQuantity > 0) {
      res.status = ResponseStatus::PARTIAL_FILL;
    } else {
      res.status = ResponseStatus::SUCCESS;
    }
    return;
  }

  if (side == Side::BUY) {

    if (remainingQuantity > 0) {
      PriceLevel &priceLevel = m_bids[price];
      priceLevel.push_back(order);
      m_activeOrders[m_currId] = std::prev(priceLevel.end());
    }

  } else if (side == Side::SELL) {

    if (remainingQuantity > 0) {
      PriceLevel &priceLevel = m_asks[price];
      priceLevel.push_back(order);
      m_activeOrders[m_currId] = std::prev(priceLevel.end());
    }
  }

  res.status = ResponseStatus::SUCCESS;
  res.newOrderId = m_currId;
  m_currId++;
}

bool Orderbook::removeOrder(OrderId orderId) {
  if (!m_activeOrders.contains(orderId)) {
    // order does not exist
    std::cout << "Error while cancelling order.\norderId: " << orderId
              << " doesn't exist" << std::endl;
    return false;
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
  return true;
}

void Orderbook::cancelOrder(Response::CancelOrder &res, OrderId orderId) {
  if (!removeOrder(orderId)) {
    res.status = ResponseStatus::BAD_REQUEST;
  } else {
    res.status = ResponseStatus::SUCCESS;
  }
}

void Orderbook::modifyOrder(Response::ModifyOrder &res, OrderId orderId,
                            Quantity newQuantity) {
  if (!m_activeOrders.contains(orderId)) {
    // invalid orderId
    std::cout << "Error while modifying order.\norderId: " << orderId
              << " doesn't exist" << std::endl;
    res.status = ResponseStatus::BAD_REQUEST;
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
    res.status = ResponseStatus::BAD_REQUEST;
    return;
  }

  it->setQuantity(newQuantity);
  res.status = ResponseStatus::SUCCESS;
}

void Orderbook::printOrderbook() {
  std::cout << "\n|==========Asks==========|" << std::endl;
  for (const auto &[priceLevel, orders] : m_asks) {
    for (const auto &order : orders) {
      order.display();
    }
  }

  std::cout << "\n|==========Bids==========|" << std::endl;
  for (const auto &[priceLevel, orders] : m_bids) {
    for (const auto &order : orders) {
      order.display();
    }
  }
}

void Orderbook::printTrades() {
  std::cout << "\n|==========Trades==========|" << std::endl;
  for (const auto &trade : m_trades) {
    trade.display();
  }
}

void Orderbook::run() {
  while (true) {
    std::optional<ClientRequest> req = m_ctx.incomingRequests.pop();
    if (req != std::nullopt) {
      std::visit(overload{[this](const Request::NewOrder &order) {
                            Response::NewOrder res{};
                            res.sessionId = order.sessionId;
                            addOrder(res, order.side, order.price,
                                     order.quantity);
                            m_ctx.outgoingResponses.push(res);
                            uint64_t cntrStep = 1;
                            write(m_ctx.eventFd, &cntrStep, 8);
                          },
                          [this](const Request::CancelOrder &order) {
                            Response::CancelOrder res{};
                            res.sessionId = order.sessionId;
                            cancelOrder(res, order.orderId);
                            m_ctx.outgoingResponses.push(res);
                            uint64_t cntrStep = 1;
                            write(m_ctx.eventFd, &cntrStep, 8);
                          },
                          [this](const Request::ModifyOrder &order) {
                            Response::ModifyOrder res{};
                            res.sessionId = order.sessionId;
                            modifyOrder(res, order.orderId, order.newQuantity);
                            m_ctx.outgoingResponses.push(res);
                            uint64_t cntrStep = 1;
                            write(m_ctx.eventFd, &cntrStep, 8);
                          }},
                 *req);
      // printOrderbook();
      // printTrades();
    }
  }
}
