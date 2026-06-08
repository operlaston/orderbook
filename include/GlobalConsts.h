#pragma once

#include <cstdint>

namespace ResponseStatus {
const uint8_t SUCCESS = 0;
const uint8_t MALFORMED_REQUEST = 1;
const uint8_t INVALID_MESSAGE_TYPE = 2;
const uint8_t SERVER_ERROR = 3;
} // namespace ResponseStatus

namespace GlobalLengths {
const uint8_t ORDER_MESSAGE = 19;
const uint32_t SPSC_QUEUE = 65536;
} // namespace GlobalLengths
