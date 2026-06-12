#pragma once

enum class TimeInForce {
  // NONE = 0,
  GOOD_TILL_CANCEL = 1,
  IMMEDIATE_OR_CANCEL = 2,
  FILL_OR_KILL = 3,
};
