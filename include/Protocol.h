#pragma once

#include <RequestTypes.h>
#include <Utils.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <optional>

// Binary wire protocol over TCP. All multi-byte integers are big-endian and
// there is no padding.
//
// Requests (client -> server), first byte is the MessageType:
//   NewOrder:    type(1) side(1) price(4) quantity(8)      = 14 bytes
//   CancelOrder: type(1) orderId(8)                        =  9 bytes
//   ModifyOrder: type(1) orderId(8) newQuantity(8)         = 17 bytes
//
// Responses (server -> client), in the same order as the requests:
//   NewOrder:    status(1) orderId(8)                      =  9 bytes
//   CancelOrder: status(1)                                 =  1 byte
//   ModifyOrder: status(1)                                 =  1 byte
namespace Protocol {

enum class MessageType : uint8_t {
  NewOrder = 0,
  CancelOrder = 1,
  ModifyOrder = 2
};

// Request sizes, including the 1-byte message type.
inline constexpr size_t NEW_ORDER_SIZE = 14;
inline constexpr size_t CANCEL_ORDER_SIZE = 9;
inline constexpr size_t MODIFY_ORDER_SIZE = 17;
inline constexpr size_t MAX_MESSAGE_SIZE = MODIFY_ORDER_SIZE;

// Payload sizes (everything after the type byte).
inline constexpr size_t NEW_ORDER_PAYLOAD_SIZE = NEW_ORDER_SIZE - 1;
inline constexpr size_t CANCEL_ORDER_PAYLOAD_SIZE = CANCEL_ORDER_SIZE - 1;
inline constexpr size_t MODIFY_ORDER_PAYLOAD_SIZE = MODIFY_ORDER_SIZE - 1;

// Response sizes.
inline constexpr size_t NEW_ORDER_RESPONSE_SIZE = 9;
inline constexpr size_t CANCEL_ORDER_RESPONSE_SIZE = 1;
inline constexpr size_t MODIFY_ORDER_RESPONSE_SIZE = 1;

namespace detail {
inline void putU32(uint8_t *dst, uint32_t value) {
  value = htobe32(value);
  std::memcpy(dst, &value, sizeof(value));
}

inline void putU64(uint8_t *dst, uint64_t value) {
  value = htobe64(value);
  std::memcpy(dst, &value, sizeof(value));
}

inline uint32_t getU32(const uint8_t *src) {
  uint32_t value;
  std::memcpy(&value, src, sizeof(value));
  return be32toh(value);
}

inline uint64_t getU64(const uint8_t *src) {
  uint64_t value;
  std::memcpy(&value, src, sizeof(value));
  return be64toh(value);
}
} // namespace detail

// Total size of a request with the given type byte, or nullopt if the type is
// unknown (the stream can't be parsed past it).
inline std::optional<size_t> messageSize(uint8_t type) {
  switch (static_cast<MessageType>(type)) {
  case MessageType::NewOrder:
    return NEW_ORDER_SIZE;
  case MessageType::CancelOrder:
    return CANCEL_ORDER_SIZE;
  case MessageType::ModifyOrder:
    return MODIFY_ORDER_SIZE;
  }
  return std::nullopt;
}

// ---- Request decoders (server side) ----
// Each takes a pointer to the payload, i.e. the bytes after the type byte, and
// the buffer must hold at least that message's payload size.

// The side byte is passed through unchecked. The engine rejects invalid sides,
// which keeps the BAD_REQUEST reply in order with the client's other replies.
inline Request::NewOrder decodeNewOrder(const uint8_t *payload, int sessionId) {
  return Request::NewOrder{sessionId, static_cast<Side>(payload[0]),
                           detail::getU32(payload + 1),
                           detail::getU64(payload + 5)};
}

inline Request::CancelOrder decodeCancelOrder(const uint8_t *payload,
                                              int sessionId) {
  return Request::CancelOrder{sessionId, detail::getU64(payload)};
}

inline Request::ModifyOrder decodeModifyOrder(const uint8_t *payload,
                                              int sessionId) {
  return Request::ModifyOrder{sessionId, detail::getU64(payload),
                              detail::getU64(payload + 8)};
}

// ---- Request encoders (client side) ----

inline std::array<uint8_t, NEW_ORDER_SIZE>
encodeNewOrder(Side side, Price price, Quantity quantity) {
  std::array<uint8_t, NEW_ORDER_SIZE> buf;
  buf[0] = static_cast<uint8_t>(MessageType::NewOrder);
  buf[1] = static_cast<uint8_t>(side);
  detail::putU32(&buf[2], price);
  detail::putU64(&buf[6], quantity);
  return buf;
}

inline std::array<uint8_t, CANCEL_ORDER_SIZE> encodeCancelOrder(OrderId id) {
  std::array<uint8_t, CANCEL_ORDER_SIZE> buf;
  buf[0] = static_cast<uint8_t>(MessageType::CancelOrder);
  detail::putU64(&buf[1], id);
  return buf;
}

inline std::array<uint8_t, MODIFY_ORDER_SIZE>
encodeModifyOrder(OrderId id, Quantity newQuantity) {
  std::array<uint8_t, MODIFY_ORDER_SIZE> buf;
  buf[0] = static_cast<uint8_t>(MessageType::ModifyOrder);
  detail::putU64(&buf[1], id);
  detail::putU64(&buf[9], newQuantity);
  return buf;
}

// ---- Responses ----

inline std::array<uint8_t, NEW_ORDER_RESPONSE_SIZE>
encodeNewOrderResponse(Status status, OrderId id) {
  std::array<uint8_t, NEW_ORDER_RESPONSE_SIZE> buf;
  buf[0] = static_cast<uint8_t>(status);
  detail::putU64(&buf[1], id);
  return buf;
}

inline NewOrderResult decodeNewOrderResponse(const uint8_t *buf) {
  return NewOrderResult{static_cast<Status>(buf[0]), detail::getU64(buf + 1)};
}

} // namespace Protocol
