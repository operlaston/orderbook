#pragma once

// Teaches GoogleTest to print these enums by name instead of as raw bytes.

#include <Protocol.h>
#include <Utils.h>
#include <ostream>

namespace Protocol {
inline void PrintTo(Status status, std::ostream* os) {
  switch (status) {
    case Status::Success: *os << "Success"; return;
    case Status::BadRequest: *os << "BadRequest"; return;
    case Status::ServerError: *os << "ServerError"; return;
  }
  *os << "Status(" << static_cast<int>(status) << ")";
}
}  // namespace Protocol

inline void PrintTo(Side side, std::ostream* os) {
  switch (side) {
    case Side::Bid: *os << "Bid"; return;
    case Side::Ask: *os << "Ask"; return;
  }
  *os << "Side(" << static_cast<int>(side) << ")";
}
