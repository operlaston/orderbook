#pragma once

#include <cstdint>

namespace ResponseStatus {
const uint8_t SUCCESS = 0;
const uint8_t BAD_REQUEST = 1;
const uint8_t INVALID_MESSAGE_TYPE = 2;
const uint8_t SERVER_ERROR = 3;
const uint8_t PARTIAL_FILL = 4;
const uint8_t CANT_FILL = 5;
} // namespace ResponseStatus

namespace GlobalLengths {
const uint8_t NEW_ORDER_MESSAGE = 19;
const uint8_t CANCEL_ORDER_MESSAGE = 8;
const uint8_t MODIFY_ORDER_MESSAGE = 16;
const uint32_t SPSC_QUEUE = 65536;
} // namespace GlobalLengths
