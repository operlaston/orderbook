#include <assert.h>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

using OrderId = uint64_t;
using Price = uint64_t;
using Quantity = uint64_t;
using Timestamp = uint64_t;

enum class Side { BUY, SELL };

class Order {
private:
  OrderId m_id;
  Side m_side;
  Price m_price;
  Quantity m_quantity;
  Timestamp m_timestamp;

public:
  Order(OrderId id, Side side, Price price, Quantity quantity,
        Timestamp timestamp)
      : m_id(id), m_side(side), m_price(price), m_quantity(quantity),
        m_timestamp(timestamp) {}

  OrderId getId() { return m_id; }
  Price getPrice() { return m_price; }
  Quantity getQuantity() { return m_quantity; }
  void setQuantity(Quantity quantity) { m_quantity = quantity; }
};

class Trade {
private:
  OrderId m_bidId;
  OrderId m_askId;
  Price m_price;
  Quantity m_quantity;
  Timestamp m_timestamp;

public:
  Trade(OrderId bidId, OrderId askId, Price price, Quantity quantity,
        Timestamp timestamp)
      : m_bidId(bidId), m_askId(askId), m_price(price), m_quantity(quantity),
        m_timestamp(timestamp) {}
};

class Orderbook {
private:
  std::map<OrderId, Order, std::greater<OrderId>> m_bids;
  std::map<OrderId, Order> m_asks;
  std::vector<Trade> m_trades;
  OrderId currId;

public:
  Orderbook() { currId = 0; }
  bool addOrder(Side side, Price price, Quantity quantity) {
    // make sure side is either sell or buy
    if (side != Side::SELL && side != Side::BUY) {
      return false;
    }

    // get current time in nanoseconds since the epoch
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    Timestamp currNs =
        std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    // first check if the order can immediately be filled
    if (side == Side::BUY) {
      if (!m_asks.empty()) {
        Order &lowestAsk = m_asks.begin()->second;
        Price lowestAskPrice = lowestAsk.getPrice();
        // check the lowest ask
        while (!m_asks.empty() && price >= lowestAskPrice) {
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
            m_asks.erase(m_asks.begin());
            quantity -= lowestAskQuantity;
            m_trades.emplace_back(currId, lowestAsk.getId(), lowestAskPrice,
                                  lowestAskQuantity, currNs);
          }
          lowestAsk = m_asks.begin()->second;
          lowestAskPrice = lowestAsk.getPrice();
        }
      }

      if (quantity > 0) {
        // if the order isn't filled completely, add it to the list
        auto [it, success] = m_bids.try_emplace(
            currId, currId, side, price, quantity,
            currNs); // try_emplace constructs object in place in map
        assert(success);
      }

    } else if (side == Side::SELL) {
      if (!m_bids.empty()) {
        Order &highestBid = m_bids.begin()->second;
        Price highestBidPrice = highestBid.getPrice();
        // check the lowest bid
        while (!m_bids.empty() && price <= highestBidPrice) {
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
            m_bids.erase(m_bids.begin());
            quantity -= highestBidQuantity;
            m_trades.emplace_back(currId, highestBid.getId(), highestBidPrice,
                                  highestBidQuantity, currNs);
          }
          highestBid = m_bids.begin()->second;
          highestBidPrice = highestBid.getPrice();
        }
      }

      if (quantity > 0) {
        // if the order isn't filled completely, add it to the list
        auto [it, success] =
            m_asks.try_emplace(currId, currId, side, price, quantity, currNs);
        assert(success);
      }
    }

    currId++;
    return true;
  }

  bool cancelOrder(OrderId orderId) {
    // TODO: cancel order
    return true;
  }

  bool modifyOrder(OrderId orderId, Quantity newQuantity) {
    // TODO: modify order
    // only decreasing quantity should be allowed
    return true;
  }
};

int main() { return 0; }
