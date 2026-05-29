#pragma once

#include <Side.h>
#include <Using.h>

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
