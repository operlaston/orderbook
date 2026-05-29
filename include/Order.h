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
        Timestamp timestamp, OrderType orderType = OrderType::LIMIT,
        TimeInForce timeInForce = TimeInForce::GOOD_TILL_CANCEL)
      : m_id(id), m_side(side), m_price(price), m_quantity(quantity),
        m_timestamp(timestamp), m_orderType(orderType),
        m_timeInForce(timeInForce) {}

  OrderId getId() { return m_id; }
  Price getPrice() { return m_price; }
  Side getSide() { return m_side; }
  Quantity getQuantity() { return m_quantity; }
  void setQuantity(Quantity quantity) { m_quantity = quantity; }

  void display() const {
    std::cout << "\nOrderId: " << m_id << "\nSide: " << static_cast<int>(m_side)
              << "\nOrderType: " << static_cast<int>(m_orderType)
              << "\nTimeInForce: " << static_cast<int>(m_timeInForce)
              << "\nPrice: " << m_price << "\nQuantity: " << m_quantity
              << "\nTimestamp: " << m_timestamp << std::endl;
  }
};
