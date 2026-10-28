#pragma once
#include <cstdint>
#include <Utils.h>

namespace Protocol {

const uint8_t NEW_ORDER_SIZE = sizeof(MessageType) + sizeof(Side) + sizeof(Price) + sizeof(Quantity);
const uint8_t CANCEL_ORDER_SIZE = sizeof(MessageType) + sizeof(OrderId);
const uint8_t MODIFY_ORDER_SIZE = sizeof(MessageType) + sizeof(OrderId) + sizeof(Quantity);

const uint8_t NEW_ORDER_RESPONSE_SIZE = sizeof(Status) + sizeof(OrderId);
const uint8_t CANCEL_ORDER_RESPONSE_SIZE = sizeof(Status);
const uint8_t MODIFY_ORDER_RESPONSE_SIZE = sizeof(Status);

}
