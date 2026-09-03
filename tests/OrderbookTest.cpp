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

  void limitBidFok(Price price, Quantity quantity) {
    ob.addOrder(lastRes, Side::BUY, price, quantity, OrderType::LIMIT,
                TimeInForce::FILL_OR_KILL);
  }

  void limitBidIoc(Price price, Quantity quantity) {
    ob.addOrder(lastRes, Side::BUY, price, quantity, OrderType::LIMIT,
                TimeInForce::IMMEDIATE_OR_CANCEL);
  }

  Order getTopBid() { return *ob.getBids().begin(); }
  Order getTopAsk() { return *ob.getAsks().begin(); }
  Trade getLastTrade() { return ob.getTrades().back(); }
};

TEST_F(OrderbookTest, BidsCanBePlaced) {
  limitBidGtc(100.0, 10);

  // make sure book is properly updated
  ASSERT_EQ(lastRes.status, ResponseStatus::SUCCESS);
  ASSERT_EQ(ob.getBids().size(), 1u);
  Order topBid = getTopBid();
  EXPECT_DOUBLE_EQ(topBid.getPrice(), 100.0);
  EXPECT_EQ(topBid.getQuantity(), 10u);

  // make sure activeOrders is properly updated
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(lastRes.newOrderId));
}

TEST_F(OrderbookTest, AsksCanBePlaced) {
  limitAskGtc(100.0, 10);

  // make sure book is properly updated
  ASSERT_EQ(lastRes.status, ResponseStatus::SUCCESS);
  ASSERT_EQ(ob.getAsks().size(), 1u);
  Order topAsk = getTopAsk();
  EXPECT_DOUBLE_EQ(topAsk.getPrice(), 100.0);
  EXPECT_EQ(topAsk.getQuantity(), 10u);

  // make sure activeOrders is properly updated
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(lastRes.newOrderId));
}

TEST_F(OrderbookTest, IncomingAskMatchedToBid) {
  limitBidGtc(100.0, 10);
  limitAskGtc(100.0, 10);

  ASSERT_EQ(ob.getTrades().size(), 1u);
  EXPECT_DOUBLE_EQ(getLastTrade().getPrice(), 100.0);
  EXPECT_EQ(getLastTrade().getQuantity(), 10u);
  EXPECT_EQ(getLastTrade().getBidId(), 1u);
  EXPECT_EQ(getLastTrade().getAskId(), 2u);

  // there should be no resting orders
  EXPECT_TRUE(ob.getActiveOrders().empty());
}

TEST_F(OrderbookTest, IncomingBidMatchedToAsk) {
  limitAskGtc(100.0, 10);
  limitBidGtc(100.0, 10);

  ASSERT_EQ(ob.getTrades().size(), 1u);
  EXPECT_DOUBLE_EQ(getLastTrade().getPrice(), 100.0);
  EXPECT_EQ(getLastTrade().getQuantity(), 10u);
  EXPECT_EQ(getLastTrade().getAskId(), 1u);
  EXPECT_EQ(getLastTrade().getBidId(), 2u);

  // there should be no resting orders
  EXPECT_TRUE(ob.getActiveOrders().empty());
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

  EXPECT_EQ(ob.getActiveOrders().size(), 2u);
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

  // make sure the resting bid is present in active orders
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(lastRes.newOrderId));
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

  // make sure the resting ask is present in active orders
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(lastRes.newOrderId));
}

TEST_F(OrderbookTest, FullFillBidPricePriorityRespected) {
  // asks placed out of order
  limitAskGtc(105.0, 1);
  OrderId askId1 = lastRes.newOrderId;
  limitAskGtc(100.0, 1);
  limitAskGtc(102.0, 2);
  OrderId askId2 = lastRes.newOrderId;
  limitBidGtc(110.0, 2);

  // should fill 100 for quantity 1 and 102 for quantity 1
  EXPECT_EQ(lastRes.status, ResponseStatus::SUCCESS);
  // check trades were logged properly
  ASSERT_EQ(ob.getTrades().size(), 2u);
  EXPECT_EQ(ob.getTrades()[0].getPrice(), 100.0);
  EXPECT_EQ(ob.getTrades()[1].getPrice(), 102.0);
  EXPECT_EQ(ob.getTrades()[0].getQuantity(), 1u);
  EXPECT_EQ(ob.getTrades()[1].getQuantity(), 1u);

  // check correctness of book state
  EXPECT_TRUE(ob.getBids().empty());
  ASSERT_EQ(ob.getAsks().size(), 2u);
  auto it = ob.getAsks().begin();
  EXPECT_EQ(it->getPrice(), 102.0);
  EXPECT_EQ(it->getQuantity(), 1u);
  it++;
  EXPECT_EQ(it->getPrice(), 105.0);
  EXPECT_EQ(it->getQuantity(), 1u);

  // check activeOrders
  EXPECT_EQ(ob.getActiveOrders().size(), 2u);
  EXPECT_TRUE(ob.getActiveOrders().contains(askId1));
  EXPECT_TRUE(ob.getActiveOrders().contains(askId2));
}

TEST_F(OrderbookTest, FullFillAskPricePriorityRespected) {
  // bids placed out of order
  limitBidGtc(100.0, 1);
  OrderId bidId1 = lastRes.newOrderId;
  limitBidGtc(105.0, 1);
  limitBidGtc(102.0, 2);
  OrderId bidId2 = lastRes.newOrderId;
  limitAskGtc(90.0, 2);

  // should fill 105 for quantity 1 and 102 for quantity 1
  EXPECT_EQ(lastRes.status, ResponseStatus::SUCCESS);
  // check trades were logged properly
  ASSERT_EQ(ob.getTrades().size(), 2u);
  EXPECT_EQ(ob.getTrades()[0].getPrice(), 105.0);
  EXPECT_EQ(ob.getTrades()[1].getPrice(), 102.0);
  EXPECT_EQ(ob.getTrades()[0].getQuantity(), 1u);
  EXPECT_EQ(ob.getTrades()[1].getQuantity(), 1u);

  // check correctness of book state
  EXPECT_TRUE(ob.getAsks().empty());
  ASSERT_EQ(ob.getBids().size(), 2u);
  auto it = ob.getBids().begin();
  EXPECT_EQ(it->getPrice(), 102.0);
  EXPECT_EQ(it->getQuantity(), 1u);
  it++;
  EXPECT_EQ(it->getPrice(), 100.0);
  EXPECT_EQ(it->getQuantity(), 1u);

  // check activeOrders
  EXPECT_EQ(ob.getActiveOrders().size(), 2u);
  EXPECT_TRUE(ob.getActiveOrders().contains(bidId1));
  EXPECT_TRUE(ob.getActiveOrders().contains(bidId2));
}

TEST_F(OrderbookTest, MarketOrderBidFilled) {
  // check if market bid fills correctly
  limitAskGtc(105.0, 1);
  OrderId askId = lastRes.newOrderId;
  limitAskGtc(100.0, 1);
  marketBid(1);

  ASSERT_EQ(ob.getAsks().size(), 1u);
  Order topAsk = getTopAsk();
  EXPECT_DOUBLE_EQ(topAsk.getPrice(), 105.0);
  EXPECT_EQ(topAsk.getQuantity(), 1u);
  EXPECT_EQ(ob.getTrades().size(), 1u);
  EXPECT_DOUBLE_EQ(ob.getTrades().front().getPrice(), 100.0);
  EXPECT_EQ(ob.getTrades().front().getQuantity(), 1u);

  // check activeOrders
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(askId));
}

TEST_F(OrderbookTest, MarketOrderAskFilled) {
  limitBidGtc(100.0, 1);
  OrderId bidId = lastRes.newOrderId;
  limitBidGtc(105.0, 1);
  marketAsk(1);

  ASSERT_EQ(ob.getBids().size(), 1u);
  Order topBid = getTopBid();
  EXPECT_DOUBLE_EQ(topBid.getPrice(), 100.0);
  EXPECT_EQ(topBid.getQuantity(), 1u);
  EXPECT_EQ(ob.getTrades().size(), 1u);
  EXPECT_DOUBLE_EQ(ob.getTrades().front().getPrice(), 105.0);
  EXPECT_EQ(ob.getTrades().front().getQuantity(), 1u);

  // check activeOrders
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(bidId));
}

// didn't feel like writing tests anymore so
// everything below this comment is claude code generated

TEST_F(OrderbookTest, FullFillBidTimePriorityRespected) {
  // two resting bids at the same price; the first placed must fill first
  limitBidGtc(100.0, 1);
  OrderId firstBid = lastRes.newOrderId;
  limitBidGtc(100.0, 1);
  OrderId secondBid = lastRes.newOrderId;

  // incoming ask only crosses one unit
  limitAskGtc(100.0, 1);

  ASSERT_EQ(ob.getTrades().size(), 1u);
  EXPECT_EQ(getLastTrade().getBidId(), firstBid);
  EXPECT_DOUBLE_EQ(getLastTrade().getPrice(), 100.0);
  EXPECT_EQ(getLastTrade().getQuantity(), 1u);

  // the later bid is the one left resting
  ASSERT_EQ(ob.getBids().size(), 1u);
  EXPECT_EQ(getTopBid().getId(), secondBid);
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(secondBid));
  EXPECT_FALSE(ob.getActiveOrders().contains(firstBid));
}

TEST_F(OrderbookTest, FullFillAskTimePriorityRespected) {
  // two resting asks at the same price; the first placed must fill first
  limitAskGtc(100.0, 1);
  OrderId firstAsk = lastRes.newOrderId;
  limitAskGtc(100.0, 1);
  OrderId secondAsk = lastRes.newOrderId;

  // incoming bid only crosses one unit
  limitBidGtc(100.0, 1);

  ASSERT_EQ(ob.getTrades().size(), 1u);
  EXPECT_EQ(getLastTrade().getAskId(), firstAsk);
  EXPECT_DOUBLE_EQ(getLastTrade().getPrice(), 100.0);
  EXPECT_EQ(getLastTrade().getQuantity(), 1u);

  // the later ask is the one left resting
  ASSERT_EQ(ob.getAsks().size(), 1u);
  EXPECT_EQ(getTopAsk().getId(), secondAsk);
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(secondAsk));
  EXPECT_FALSE(ob.getActiveOrders().contains(firstAsk));
}

TEST_F(OrderbookTest, IncomingBidCrossesMultiplePriceLevels) {
  limitAskGtc(100.0, 1);
  limitAskGtc(101.0, 1);
  limitAskGtc(102.0, 1);

  // one bid that sweeps all three ask levels and rests the remainder
  limitBidGtc(103.0, 5);

  EXPECT_EQ(lastRes.status, ResponseStatus::PARTIAL_FILL);
  ASSERT_EQ(ob.getTrades().size(), 3u);
  EXPECT_DOUBLE_EQ(ob.getTrades()[0].getPrice(), 100.0);
  EXPECT_DOUBLE_EQ(ob.getTrades()[1].getPrice(), 101.0);
  EXPECT_DOUBLE_EQ(ob.getTrades()[2].getPrice(), 102.0);

  EXPECT_TRUE(ob.getAsks().empty());
  ASSERT_EQ(ob.getBids().size(), 1u);
  EXPECT_DOUBLE_EQ(getTopBid().getPrice(), 103.0);
  EXPECT_EQ(getTopBid().getQuantity(), 2u);

  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(lastRes.newOrderId));
}

TEST_F(OrderbookTest, IncomingAskCrossesMultiplePriceLevels) {
  limitBidGtc(102.0, 1);
  limitBidGtc(101.0, 1);
  limitBidGtc(100.0, 1);

  // one ask that sweeps all three bid levels and rests the remainder
  limitAskGtc(100.0, 5);

  EXPECT_EQ(lastRes.status, ResponseStatus::PARTIAL_FILL);
  ASSERT_EQ(ob.getTrades().size(), 3u);
  EXPECT_DOUBLE_EQ(ob.getTrades()[0].getPrice(), 102.0);
  EXPECT_DOUBLE_EQ(ob.getTrades()[1].getPrice(), 101.0);
  EXPECT_DOUBLE_EQ(ob.getTrades()[2].getPrice(), 100.0);

  EXPECT_TRUE(ob.getBids().empty());
  ASSERT_EQ(ob.getAsks().size(), 1u);
  EXPECT_DOUBLE_EQ(getTopAsk().getPrice(), 100.0);
  EXPECT_EQ(getTopAsk().getQuantity(), 2u);

  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(lastRes.newOrderId));
}

TEST_F(OrderbookTest, MarketBidCantFillAgainstEmptyBook) {
  marketBid(5);

  EXPECT_EQ(lastRes.status, ResponseStatus::CANT_FILL);
  EXPECT_TRUE(ob.getTrades().empty());
  EXPECT_TRUE(ob.getBids().empty());
  EXPECT_TRUE(ob.getAsks().empty());
  EXPECT_TRUE(ob.getActiveOrders().empty());
}

TEST_F(OrderbookTest, MarketAskCantFillAgainstEmptyBook) {
  marketAsk(5);

  EXPECT_EQ(lastRes.status, ResponseStatus::CANT_FILL);
  EXPECT_TRUE(ob.getTrades().empty());
  EXPECT_TRUE(ob.getBids().empty());
  EXPECT_TRUE(ob.getAsks().empty());
  EXPECT_TRUE(ob.getActiveOrders().empty());
}

TEST_F(OrderbookTest, FillOrKillDoesNotPartiallyFillOrRest) {
  limitAskGtc(100.0, 5);
  OrderId askId = lastRes.newOrderId;

  // cannot be filled completely -> must be killed entirely
  limitBidFok(100.0, 10);

  EXPECT_EQ(lastRes.status, ResponseStatus::CANT_FILL);
  EXPECT_TRUE(ob.getTrades().empty());
  EXPECT_TRUE(ob.getBids().empty());

  // the resting ask must be untouched
  ASSERT_EQ(ob.getAsks().size(), 1u);
  EXPECT_EQ(getTopAsk().getQuantity(), 5u);
  EXPECT_EQ(ob.getActiveOrders().size(), 1u);
  EXPECT_TRUE(ob.getActiveOrders().contains(askId));
}

TEST_F(OrderbookTest, FillOrKillCompletelyFillsWhenPossible) {
  limitAskGtc(100.0, 5);
  limitAskGtc(101.0, 5);

  limitBidFok(101.0, 10);

  EXPECT_EQ(lastRes.status, ResponseStatus::SUCCESS);
  ASSERT_EQ(ob.getTrades().size(), 2u);
  EXPECT_DOUBLE_EQ(ob.getTrades()[0].getPrice(), 100.0);
  EXPECT_DOUBLE_EQ(ob.getTrades()[1].getPrice(), 101.0);
  EXPECT_EQ(ob.getTrades()[0].getQuantity(), 5u);
  EXPECT_EQ(ob.getTrades()[1].getQuantity(), 5u);

  // nothing rests: the FoK order is gone and both asks are consumed
  EXPECT_TRUE(ob.getAsks().empty());
  EXPECT_TRUE(ob.getBids().empty());
  EXPECT_TRUE(ob.getActiveOrders().empty());
}

TEST_F(OrderbookTest, ImmediateOrCancelDoesNotRest) {
  limitAskGtc(100.0, 3);

  // more than can be filled; the unfilled remainder must not rest
  limitBidIoc(100.0, 10);

  EXPECT_EQ(lastRes.status, ResponseStatus::PARTIAL_FILL);
  ASSERT_EQ(ob.getTrades().size(), 1u);
  EXPECT_EQ(getLastTrade().getQuantity(), 3u);

  EXPECT_TRUE(ob.getBids().empty());
  EXPECT_TRUE(ob.getAsks().empty());
  EXPECT_TRUE(ob.getActiveOrders().empty());
}

TEST_F(OrderbookTest, CancelUnknownOrderIdReturnsBadRequest) {
  Response::CancelOrder res{};
  ob.cancelOrder(res, 999u);
  EXPECT_EQ(res.status, ResponseStatus::BAD_REQUEST);
}

TEST_F(OrderbookTest, CancelRemovesOrderAndEmptiesPriceLevel) {
  limitBidGtc(100.0, 5);
  OrderId lowerBid = lastRes.newOrderId;
  limitBidGtc(101.0, 5);
  OrderId topBid = lastRes.newOrderId;

  Response::CancelOrder res{};
  ob.cancelOrder(res, topBid);
  EXPECT_EQ(res.status, ResponseStatus::SUCCESS);

  // the 101 level is gone entirely, only the 100 level remains
  ASSERT_EQ(ob.getBids().size(), 1u);
  EXPECT_DOUBLE_EQ(getTopBid().getPrice(), 100.0);
  EXPECT_EQ(getTopBid().getId(), lowerBid);
  EXPECT_FALSE(ob.getActiveOrders().contains(topBid));

  // cancelling the last order empties the book
  ob.cancelOrder(res, lowerBid);
  EXPECT_EQ(res.status, ResponseStatus::SUCCESS);
  EXPECT_TRUE(ob.getBids().empty());
  EXPECT_EQ(ob.getBids().size(), 0u);
  EXPECT_TRUE(ob.getActiveOrders().empty());
}

TEST_F(OrderbookTest, ModifyCannotIncreaseQuantity) {
  limitBidGtc(100.0, 5);
  OrderId id = lastRes.newOrderId;

  Response::ModifyOrder res{};
  ob.modifyOrder(res, id, 10u);

  EXPECT_EQ(res.status, ResponseStatus::BAD_REQUEST);
  EXPECT_EQ(getTopBid().getQuantity(), 5u);
}

TEST_F(OrderbookTest, ModifyCannotSetNonPositiveQuantity) {
  limitBidGtc(100.0, 5);
  OrderId id = lastRes.newOrderId;

  Response::ModifyOrder res{};
  ob.modifyOrder(res, id, 0u);

  EXPECT_EQ(res.status, ResponseStatus::BAD_REQUEST);
  EXPECT_EQ(getTopBid().getQuantity(), 5u);
}

TEST_F(OrderbookTest, ModifyCanReduceQuantity) {
  limitBidGtc(100.0, 5);
  OrderId id = lastRes.newOrderId;

  Response::ModifyOrder res{};
  ob.modifyOrder(res, id, 2u);

  EXPECT_EQ(res.status, ResponseStatus::SUCCESS);
  ASSERT_EQ(ob.getBids().size(), 1u);
  EXPECT_EQ(getTopBid().getQuantity(), 2u);
}

TEST_F(OrderbookTest, ModifyUnknownOrderIdReturnsBadRequest) {
  Response::ModifyOrder res{};
  ob.modifyOrder(res, 999u, 1u);
  EXPECT_EQ(res.status, ResponseStatus::BAD_REQUEST);
}

TEST_F(OrderbookTest, RejectsInvalidSide) {
  Response::NewOrder res{};
  ob.addOrder(res, static_cast<Side>(42), 100.0, 10, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  EXPECT_EQ(res.status, ResponseStatus::BAD_REQUEST);
  EXPECT_TRUE(ob.getBids().empty());
  EXPECT_TRUE(ob.getAsks().empty());
  EXPECT_TRUE(ob.getActiveOrders().empty());
}

TEST_F(OrderbookTest, RejectsNonPositivePrice) {
  limitBidGtc(0.0, 10);
  EXPECT_EQ(lastRes.status, ResponseStatus::BAD_REQUEST);

  limitBidGtc(-5.0, 10);
  EXPECT_EQ(lastRes.status, ResponseStatus::BAD_REQUEST);

  EXPECT_TRUE(ob.getBids().empty());
  EXPECT_TRUE(ob.getActiveOrders().empty());
}

TEST_F(OrderbookTest, RejectsNonPositiveQuantity) {
  limitBidGtc(100.0, 0);

  EXPECT_EQ(lastRes.status, ResponseStatus::BAD_REQUEST);
  EXPECT_TRUE(ob.getBids().empty());
  EXPECT_TRUE(ob.getActiveOrders().empty());
}
