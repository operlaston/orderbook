#pragma once

#include <Utils.h>
#include <iostream>

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
  void display() const {
    std::cout << "\nBid Order Id: " << m_bidId << "\nAsk Order Id: " << m_askId
              << "\nPrice: " << m_price << "\nQuantity: " << m_quantity
              << "\nTimestamp: " << m_timestamp << std::endl;
  }
  OrderId getBidId() const { return m_bidId; }
  OrderId getAskId() const { return m_askId; }
  Price getPrice() const { return m_price; }
  Quantity getQuantity() const { return m_quantity; }
};
