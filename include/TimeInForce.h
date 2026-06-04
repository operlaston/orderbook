#pragma once

enum class TimeInForce {
  NONE,
  GOOD_TILL_CANCEL,
  IMMEDIATE_OR_CANCEL,
  FILL_OR_KILL,
};
