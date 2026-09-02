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

// matches the incoming order against the resting/opposite-side book
template <typename Compare>
Quantity Orderbook::matchAgainst(BookSide<Compare> &restingBook,
                                 const Order &incomingOrder, bool doFill) {
  // gives us std::less if restingBook is asks
  // gives us std::greater if restingBook is bids
  // we want price >= lowestAskPrice and price <= highestBidPrice
  Compare reversePriceCmp{};
  Price incomingPrice = incomingOrder.getPrice();
  Quantity incomingQuantity = incomingOrder.getQuantity();

  auto bookIt = restingBook.begin();
  while (bookIt != restingBook.end() && incomingQuantity > 0 &&
         !reversePriceCmp(incomingPrice, bookIt->getPrice())) {
    Quantity restingQuantity = bookIt->getQuantity();
    Quantity filledQuantity = std::min(incomingQuantity, restingQuantity);

    incomingQuantity -= filledQuantity;

    if (!doFill) {
      continue;
    }

    OrderId bidId =
        (incomingOrder.getSide() == Side::BUY) ? m_currId : bookIt->getId();
    OrderId askId =
        (incomingOrder.getSide() == Side::BUY) ? bookIt->getId() : m_currId;

    m_trades.emplace_back(bidId, askId, bookIt->getPrice(), filledQuantity,
                          incomingOrder.getTimestamp());
    if (filledQuantity == restingQuantity) {
      bookIt = restingBook.erase(bookIt, m_activeOrders);
    } else {
      bookIt->setQuantity(restingQuantity - filledQuantity);
    }
  }

  return incomingQuantity;
}

// returns the remaining quantity of the order
Quantity Orderbook::matchOrder(const Order &incomingOrder) {

  // initialize a const here to avoid the "magic number"
  const bool doFill = true;

  if (incomingOrder.getSide() == Side::BUY) {
    if (incomingOrder.getTimeInForce() == TimeInForce::FILL_OR_KILL) {
      Quantity remainingQuantity = matchAgainst(m_asks, incomingOrder, !doFill);
      if (remainingQuantity > 0)
        return remainingQuantity;
    }
    return matchAgainst(m_asks, incomingOrder, doFill);
  }

  // incoming is an ask
  if (incomingOrder.getTimeInForce() == TimeInForce::FILL_OR_KILL) {
    Quantity remainingQuantity = matchAgainst(m_bids, incomingOrder, !doFill);
    if (remainingQuantity > 0)
      return remainingQuantity;
  }

  return matchAgainst(m_bids, incomingOrder, doFill);
}

void Orderbook::addOrder(Response::NewOrder &res, Side side, Price price,
                         Quantity quantity, OrderType orderType,
                         TimeInForce timeInForce) {

  // std::cout << "\n<==============ADDING NEW ORDER==================>\n"
  //           << "Side: " << static_cast<int>(side)
  //           << "\nPrice: " << static_cast<int>(price)
  //           << "\nQuantity: " << quantity
  //           << "\nOrderType: " << static_cast<int>(orderType)
  //           << "\nTimeInForce: " << static_cast<int>(timeInForce) <<
  //           std::endl;

  m_currId++;

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

  if (remainingQuantity > 0 && quantity != remainingQuantity) {
    res.status = ResponseStatus::PARTIAL_FILL;
  } else {
    res.status = ResponseStatus::SUCCESS;
  }
  // order is not fok or ioc so we change the quantity to
  // what remains
  order.setQuantity(remainingQuantity);

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

  res.newOrderId = m_currId;
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

namespace {
void writeToServer(int eventFd) {
  uint64_t inc = 1;
  ssize_t n;
  do {
    n = ::write(eventFd, &inc, sizeof(inc));
  } while (n < 0 && errno == EINTR);

  // this should never happen
  if (n != sizeof(inc)) {
    ::perror("eventFd write");
    std::abort();
  }
}
} // namespace

void Orderbook::run() {
  while (true) {
    std::optional<ClientRequest> req = m_ctx.incomingRequests.pop();
    if (req != std::nullopt) {
      std::visit(overload{[this](const Request::NewOrder &order) {
                            Response::NewOrder res{};
                            res.sessionId = order.sessionId;
                            addOrder(res, order.side, order.price,
                                     order.quantity, order.orderType,
                                     order.timeInForce);
                            m_ctx.outgoingResponses.push(res);
                            writeToServer(m_ctx.eventFd);
                          },
                          [this](const Request::CancelOrder &order) {
                            Response::CancelOrder res{};
                            res.sessionId = order.sessionId;
                            cancelOrder(res, order.orderId);
                            m_ctx.outgoingResponses.push(res);
                            writeToServer(m_ctx.eventFd);
                          },
                          [this](const Request::ModifyOrder &order) {
                            Response::ModifyOrder res{};
                            res.sessionId = order.sessionId;
                            modifyOrder(res, order.orderId, order.newQuantity);
                            m_ctx.outgoingResponses.push(res);
                            writeToServer(m_ctx.eventFd);
                          }},
                 *req);
      // printOrderbook();
      // printTrades();
    }
  }
}
