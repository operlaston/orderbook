#pragma once

#include <OrderType.h>
#include <Side.h>
#include <TimeInForce.h>
#include <Using.h>
#include <iostream>

class Order {
private:
  OrderId m_id;
  Side m_side;
  Price m_price;
  Quantity m_quantity;
  Timestamp m_timestamp;
  OrderType m_orderType;
  TimeInForce m_timeInForce;

public:
  Order(OrderId id, Side side, Price price, Quantity quantity,
        Timestamp timestamp, OrderType orderType, TimeInForce timeInForce)
      : m_id(id), m_side(side), m_price(price), m_quantity(quantity),
        m_timestamp(timestamp), m_orderType(orderType),
        m_timeInForce(timeInForce) {}

  OrderId getId() const { return m_id; }
  Price getPrice() const { return m_price; }
  Side getSide() const { return m_side; }
  Timestamp getTimestamp() const { return m_timestamp; }
  OrderType getOrderType() const { return m_orderType; }
  TimeInForce getTimeInForce() const { return m_timeInForce; }
  Quantity getQuantity() const { return m_quantity; }
  void setQuantity(Quantity quantity) { m_quantity = quantity; }

  void display() const {
    std::cout << "\nOrderId: " << m_id << "\nSide: " << static_cast<int>(m_side)
              << "\nOrderType: " << static_cast<int>(m_orderType)
              << "\nTimeInForce: " << static_cast<int>(m_timeInForce)
              << "\nPrice: " << m_price << "\nQuantity: " << m_quantity
              << "\nTimestamp: " << m_timestamp << std::endl;
  }
};
