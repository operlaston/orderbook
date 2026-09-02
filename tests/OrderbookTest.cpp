#include <gtest/gtest.h>

#include "GlobalConsts.h"
#include "OrderType.h"
#include "Orderbook.h"
#include "ServerEngineContext.h"
#include "Side.h"
#include "TimeInForce.h"

class OrderbookTest : public testing::Test {
protected:
  ServerEngineContext ctx;
  Orderbook ob;
  Response::NewOrder lastRes{};

  OrderbookTest() : ob(ctx) {}
  ~OrderbookTest() override = default;

  void limitBidGtc(Price price, Quantity quantity) {
    ob.addOrder(lastRes, Side::BUY, price, quantity, OrderType::LIMIT,
                TimeInForce::GOOD_TILL_CANCEL);
  }

  void limitAskGtc(Price price, Quantity quantity) {
    ob.addOrder(lastRes, Side::SELL, price, quantity, OrderType::LIMIT,
                TimeInForce::GOOD_TILL_CANCEL);
  }

  void marketBid(Quantity quantity) {
    ob.addOrder(lastRes, Side::BUY, 0, quantity, OrderType::MARKET,
                TimeInForce::NONE);
  }

  void marketAsk(Quantity quantity) {
    ob.addOrder(lastRes, Side::SELL, 0, quantity, OrderType::MARKET,
                TimeInForce::NONE);
  }

  Order getTopBid() { return *ob.getBids().begin(); }
  Order getTopAsk() { return *ob.getAsks().begin(); }
  Trade getLastTrade() { return ob.getTrades().back(); }
};

TEST_F(OrderbookTest, BidsCanBePlaced) {
  limitBidGtc(100.0, 10);
  ASSERT_EQ(lastRes.status, ResponseStatus::SUCCESS);
  ASSERT_EQ(ob.getBids().size(), 1u);
  Order topBid = getTopBid();
  EXPECT_DOUBLE_EQ(topBid.getPrice(), 100.0);
  EXPECT_EQ(topBid.getQuantity(), 10u);
}

TEST_F(OrderbookTest, AsksCanBePlaced) {
  limitAskGtc(100.0, 10);
  ASSERT_EQ(lastRes.status, ResponseStatus::SUCCESS);
  ASSERT_EQ(ob.getAsks().size(), 1u);
  Order topAsk = getTopAsk();
  EXPECT_DOUBLE_EQ(topAsk.getPrice(), 100.0);
  EXPECT_EQ(topAsk.getQuantity(), 10u);
}

TEST_F(OrderbookTest, IncomingAskMatchedToBid) {
  limitBidGtc(100.0, 10);
  limitAskGtc(100.0, 10);

  ASSERT_EQ(ob.getTrades().size(), 1u);
  EXPECT_DOUBLE_EQ(getLastTrade().getPrice(), 100.0);
  EXPECT_EQ(getLastTrade().getQuantity(), 10u);
  EXPECT_EQ(getLastTrade().getBidId(), 1u);
  EXPECT_EQ(getLastTrade().getAskId(), 2u);
}

TEST_F(OrderbookTest, IncomingBidMatchedToAsk) {
  limitAskGtc(100.0, 10);
  limitBidGtc(100.0, 10);

  ASSERT_EQ(ob.getTrades().size(), 1u);
  EXPECT_DOUBLE_EQ(getLastTrade().getPrice(), 100.0);
  EXPECT_EQ(getLastTrade().getQuantity(), 10u);
  EXPECT_EQ(getLastTrade().getAskId(), 1u);
  EXPECT_EQ(getLastTrade().getBidId(), 2u);
}

TEST_F(OrderbookTest, LimitOrderWithNoMatchesRests) {

  limitBidGtc(100.0, 1);
  limitAskGtc(101.0, 1);

  EXPECT_TRUE(ob.getTrades().empty());

  ASSERT_EQ(ob.getBids().size(), 1u);
  Order topBid = getTopBid();
  EXPECT_DOUBLE_EQ(topBid.getPrice(), 100.0);
  EXPECT_DOUBLE_EQ(topBid.getQuantity(), 1);

  ASSERT_EQ(ob.getAsks().size(), 1u);
  Order topAsk = getTopAsk();
  EXPECT_DOUBLE_EQ(topAsk.getPrice(), 101.0);
  EXPECT_DOUBLE_EQ(topAsk.getQuantity(), 1);
}

TEST_F(OrderbookTest, LargeBidPartialFills) {
  limitAskGtc(100.0, 2);
  limitBidGtc(105.0, 3);

  EXPECT_EQ(lastRes.status, ResponseStatus::PARTIAL_FILL);
  EXPECT_EQ(ob.getTrades().size(), 1u);
  EXPECT_EQ(ob.getAsks().size(), 0u);
  ASSERT_EQ(ob.getBids().size(), 1u);
  Order topBid = getTopBid();
  EXPECT_DOUBLE_EQ(topBid.getPrice(), 105.0);
  EXPECT_EQ(topBid.getQuantity(), 1u);
}

TEST_F(OrderbookTest, LargeAskPartialFills) {
  // larger quantity ask to test partial fill
  limitBidGtc(105.0, 2);
  limitAskGtc(100.0, 3);

  EXPECT_EQ(lastRes.status, ResponseStatus::PARTIAL_FILL);
  EXPECT_EQ(ob.getTrades().size(), 1u);
  EXPECT_EQ(ob.getBids().size(), 0u);
  ASSERT_EQ(ob.getAsks().size(), 1u);
  Order topAsk = getTopAsk();
  EXPECT_DOUBLE_EQ(topAsk.getPrice(), 100.0);
  EXPECT_EQ(topAsk.getQuantity(), 1u);
}

TEST_F(OrderbookTest, CompleteFillBidPricePriorityRespected) {
  // asks placed out of order
  limitAskGtc(105.0, 1);
  limitAskGtc(100.0, 1);
  limitAskGtc(102.0, 2);
  limitBidGtc(110.0, 2);

  // should fill 100 for quantity 1 and 102 for quantity 1
  EXPECT_EQ(lastRes.status, ResponseStatus::SUCCESS);
  // check trades were logged properly
  ASSERT_EQ(ob.getTrades().size(), 2u);
  EXPECT_EQ(ob.getTrades()[1].getPrice(), 102.0);
  EXPECT_EQ(ob.getTrades()[0].getPrice(), 100.0);
  EXPECT_EQ(ob.getTrades()[1].getQuantity(), 1u);
  EXPECT_EQ(ob.getTrades()[0].getQuantity(), 1u);

  // check correctness of book state
  EXPECT_TRUE(ob.getBids().empty());
  ASSERT_EQ(ob.getAsks().size(), 2u);
  auto it = ob.getAsks().begin();
  EXPECT_EQ(it->getPrice(), 102.0);
  EXPECT_EQ(it->getQuantity(), 1u);
  it++;
  EXPECT_EQ(it->getPrice(), 105.0);
  EXPECT_EQ(it->getQuantity(), 1u);
}

TEST_F(OrderbookTest, MarketOrderBidFilled) {
  // check if market bid fills correctly
  limitAskGtc(105.0, 1);
  limitAskGtc(100.0, 1);
  marketBid(1);
  ASSERT_EQ(ob.getAsks().size(), 1u);
  Order topAsk = getTopAsk();
  EXPECT_DOUBLE_EQ(topAsk.getPrice(), 105.0);
  EXPECT_EQ(topAsk.getQuantity(), 1u);
  EXPECT_EQ(ob.getTrades().size(), 1u);
  EXPECT_DOUBLE_EQ(ob.getTrades().front().getPrice(), 100.0);
  EXPECT_EQ(ob.getTrades().front().getQuantity(), 1u);
}
