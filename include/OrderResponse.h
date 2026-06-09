#pragma once

#include <cstdint>
struct OrderResponse {
  int sessionId; // file descriptor number
  uint8_t status;
};
