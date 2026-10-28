#include <gtest/gtest.h>

#include <Protocol.h>
#include <vector>

using namespace Protocol;

namespace {
template <size_t N>
std::vector<uint8_t> toVec(const std::array<uint8_t, N> &arr) {
  return {arr.begin(), arr.end()};
}
} // namespace

TEST(ProtocolTest, MessageSizes) {
  EXPECT_EQ(messageSize(0), 14u);
  EXPECT_EQ(messageSize(1), 9u);
  EXPECT_EQ(messageSize(2), 17u);
  EXPECT_EQ(messageSize(3), std::nullopt);
  EXPECT_EQ(messageSize(255), std::nullopt);
}

TEST(ProtocolTest, NewOrderIsBigEndianByteExact) {
  auto buf = encodeNewOrder(Side::Ask, 0x01020304, 0x05060708090A0B0CULL);
  std::vector<uint8_t> expected = {0x00, 0x01, 0x01, 0x02, 0x03, 0x04, 0x05,
                                   0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};
  EXPECT_EQ(toVec(buf), expected);
}

TEST(ProtocolTest, NewOrderRoundTrip) {
  auto buf = encodeNewOrder(Side::Bid, 12345, 987654321);
  Request::NewOrder req = decodeNewOrder(buf.data() + 1, 7);
  EXPECT_EQ(req.sessionId, 7);
  EXPECT_EQ(req.side, Side::Bid);
  EXPECT_EQ(req.price, 12345u);
  EXPECT_EQ(req.quantity, 987654321u);
}

TEST(ProtocolTest, NewOrderInvalidSidePassesThroughUnchanged) {
  // The engine rejects invalid sides, so the decoder keeps the raw value.
  auto buf = encodeNewOrder(Side::Bid, 100, 10);
  buf[1] = 2;
  Request::NewOrder req = decodeNewOrder(buf.data() + 1, 3);
  EXPECT_EQ(static_cast<uint8_t>(req.side), 2);
}

TEST(ProtocolTest, CancelOrderByteExactAndRoundTrip) {
  auto buf = encodeCancelOrder(0x1122334455667788ULL);
  std::vector<uint8_t> expected = {0x01, 0x11, 0x22, 0x33, 0x44,
                                   0x55, 0x66, 0x77, 0x88};
  EXPECT_EQ(toVec(buf), expected);

  Request::CancelOrder req = decodeCancelOrder(buf.data() + 1, 4);
  EXPECT_EQ(req.sessionId, 4);
  EXPECT_EQ(req.orderId, 0x1122334455667788ULL);
}

TEST(ProtocolTest, ModifyOrderByteExactAndRoundTrip) {
  auto buf = encodeModifyOrder(1, 0x0102030405060708ULL);
  std::vector<uint8_t> expected = {0x02, 0, 0, 0, 0, 0, 0, 0, 1,
                                   0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                   0x08};
  EXPECT_EQ(toVec(buf), expected);

  Request::ModifyOrder req = decodeModifyOrder(buf.data() + 1, 9);
  EXPECT_EQ(req.sessionId, 9);
  EXPECT_EQ(req.orderId, 1u);
  EXPECT_EQ(req.newQuantity, 0x0102030405060708ULL);
}

TEST(ProtocolTest, NewOrderResponseIsAlwaysNineBytes) {
  auto ok = encodeNewOrderResponse(Status::Success, 0xAABBCCDDEEFF0011ULL);
  std::vector<uint8_t> expectedOk = {0x00, 0xAA, 0xBB, 0xCC, 0xDD,
                                     0xEE, 0xFF, 0x00, 0x11};
  EXPECT_EQ(toVec(ok), expectedOk);

  auto bad = encodeNewOrderResponse(Status::BadRequest, 0);
  std::vector<uint8_t> expectedBad = {0x01, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(toVec(bad), expectedBad);

  static_assert(sizeof(ok) == 9 && sizeof(bad) == 9);
}

TEST(ProtocolTest, NewOrderResponseRoundTrip) {
  auto buf = encodeNewOrderResponse(Status::ServerError, 42);
  NewOrderResult res = decodeNewOrderResponse(buf.data());
  EXPECT_EQ(res.status, Status::ServerError);
  EXPECT_EQ(res.orderId, 42u);
}
