#pragma once

#include <cstdint>

namespace ResponseStatus {
const uint8_t SUCCESS = 0;
const uint8_t MALFORMED_REQUEST = 1;
const uint8_t INVALID_MESSAGE_TYPE = 2;
const uint8_t SERVER_ERROR = 3;
} // namespace ResponseStatus

namespace MessageLength {
const uint8_t ORDER = 19;
}
