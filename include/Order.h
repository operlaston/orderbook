#pragma once

#include <Utils.h>
#include <iostream>

class Order {
private:
  OrderId m_id;
  Quantity m_quantity;
  // Timestamp m_timestamp;
  Price m_price;
  Side m_side;

public:
  Order(OrderId id, Quantity quantity, Price price, Side side) :
    m_id(id), m_quantity(quantity), m_price(price), m_side(side) {}

  OrderId getId() const { return m_id; }
  Quantity getQuantity() const { return m_quantity; }
  Price getPrice() const { return m_price; }
  Side getSide() const { return m_side; }
  // Timestamp getTimestamp() const { return m_timestamp; }
  void setQuantity(Quantity quantity) { m_quantity = quantity; }

  void display() const {
    std::cout << "\nOrderId: " << m_id << "\nSide: " << static_cast<int>(m_side)
              << "\nPrice: " << m_price << "\nQuantity: " << m_quantity << std::endl;
  }
};
