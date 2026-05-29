#pragma once

#include <Side.h>
#include <Using.h>
#include <iostream>
#include <string>

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
  Side getSide() { return m_side; }
  Quantity getQuantity() { return m_quantity; }
  void setQuantity(Quantity quantity) { m_quantity = quantity; }

  void display() const {
    std::string side;
    if (m_side == Side::BUY) {
      side = "BUY";
    } else if (m_side == Side::SELL) {
      side = "SELL";
    } else {
      side = "UNKNOWN/ERROR";
    }
    std::cout << "\nOrderId: " << m_id << "\nSide: " << side
              << "\nPrice: " << m_price << "\nQuantity: " << m_quantity
              << "\nTimestamp: " << m_timestamp << std::endl;
  }
};
