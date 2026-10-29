#pragma once
#include <cstdint>
#include <Utils.h>

namespace Protocol {


enum class Status: uint8_t {
  Success = 0,
  BadRequest = 1,
  ServerError = 2
};

enum class MessageType: uint8_t {
  NewOrder = 0,
  CancelOrder = 1,
  ModifyOrder = 2
};

// we don't include the size of the message type byte
constexpr uint8_t NEW_ORDER_SIZE = sizeof(Side) + sizeof(Price) + sizeof(Quantity);
constexpr uint8_t CANCEL_ORDER_SIZE = sizeof(OrderId);
constexpr uint8_t MODIFY_ORDER_SIZE = sizeof(OrderId) + sizeof(Quantity);

constexpr uint8_t NEW_ORDER_RES_SIZE = sizeof(Status) + sizeof(OrderId);
constexpr uint8_t CANCEL_ORDER_RES_SIZE = sizeof(Status);
constexpr uint8_t MODIFY_ORDER_RES_SIZE = sizeof(Status);


}
