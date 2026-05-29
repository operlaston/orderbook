#include <Order.h>
#include <Trade.h>
#include <Using.h>
#include <assert.h>
#include <chrono>
#include <functional>
#include <list>
#include <map>

using PriceLevel = std::list<Order>;

class Orderbook {
private:
  std::map<Price, std::list<Order>, std::greater<Price>> m_bids;
  std::map<Price, std::list<Order>> m_asks;
  std::unordered_map<OrderId, std::list<Order>::iterator> m_activeOrders;
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
        Order &lowestAsk = m_asks.begin()->second.front();
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
            // erase the ask from both the activeOrders map and the asks queue
            m_asks.erase(m_asks.begin());
            m_activeOrders.erase(lowestAsk.getId());
            quantity -= lowestAskQuantity;
            m_trades.emplace_back(currId, lowestAsk.getId(), lowestAskPrice,
                                  lowestAskQuantity, currNs);
          }
          lowestAsk = m_asks.begin()->second.front();
          lowestAskPrice = lowestAsk.getPrice();
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
        Order &highestBid = m_bids.begin()->second.front();
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
            // erase the order from both the activeOrders map and the bids queue
            m_bids.erase(m_bids.begin());
            m_activeOrders.erase(highestBid.getId());
            quantity -= highestBidQuantity;
            m_trades.emplace_back(currId, highestBid.getId(), highestBidPrice,
                                  highestBidQuantity, currNs);
          }
          highestBid = m_bids.begin()->second.front();
          highestBidPrice = highestBid.getPrice();
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
