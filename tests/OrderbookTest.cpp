#include <gtest/gtest.h>

#include "GlobalConsts.h"
#include "OrderType.h"
#include "Orderbook.h"
#include "ServerEngineContext.h"
#include "Side.h"
#include "TimeInForce.h"

TEST(OrderbookTest, BidsAndAsksCanBePlaced) {
  ServerEngineContext ctx;
  Orderbook ob(ctx);

  Response::NewOrder buyRes{};
  ob.addOrder(buyRes, Side::BUY, 100.0, 10, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  ASSERT_EQ(buyRes.status, ResponseStatus::SUCCESS);
  ASSERT_EQ(ob.getBids().size(), 1u);
  auto restingBid = ob.getBids().begin()->second.begin();
  EXPECT_DOUBLE_EQ(restingBid->getPrice(), 100.0);
  EXPECT_EQ(restingBid->getQuantity(), 10u);

  Response::NewOrder sellRes{};
  ob.addOrder(sellRes, Side::SELL, 200.0, 10, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);
  ASSERT_EQ(sellRes.status, ResponseStatus::SUCCESS);
  ASSERT_EQ(ob.getAsks().size(), 1u);
  auto restingAsk = ob.getAsks().begin()->second.begin();
  EXPECT_DOUBLE_EQ(restingAsk->getPrice(), 200.0);
  EXPECT_EQ(restingAsk->getQuantity(), 10u);
}

TEST(OrderbookTest, CrossingLimitOrdersProduceATrade) {
  ServerEngineContext ctx;
  Orderbook ob(ctx);

  Response::NewOrder buyRes{};
  ob.addOrder(buyRes, Side::BUY, 100.0, 10, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  Response::NewOrder sellRes{};
  ob.addOrder(sellRes, Side::SELL, 100.0, 10, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  ASSERT_EQ(ob.getTrades().size(), 1u);
  EXPECT_DOUBLE_EQ(ob.getTrades().front().getPrice(), 100.0);
  EXPECT_EQ(ob.getTrades().front().getQuantity(), 10u);
  EXPECT_EQ(ob.getTrades().front().getBidId(), 1u);
  EXPECT_EQ(ob.getTrades().front().getAskId(), 2u);
}

TEST(OrderbookTest, LimitOrderWithNoMatchesRests) {
  ServerEngineContext ctx;
  Orderbook ob(ctx);

  Response::NewOrder buyRes{};
  ob.addOrder(buyRes, Side::BUY, 100.0, 1, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  Response::NewOrder sellRes{};
  ob.addOrder(sellRes, Side::SELL, 101.0, 1, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  EXPECT_TRUE(ob.getTrades().empty());

  ASSERT_EQ(ob.getBids().size(), 1u);
  auto restingBid = ob.getBids().begin();
  EXPECT_DOUBLE_EQ(restingBid->first, 100.0);
  EXPECT_DOUBLE_EQ((restingBid->second.begin())->getQuantity(), 1);

  ASSERT_EQ(ob.getAsks().size(), 1u);
  auto restingAsk = ob.getAsks().begin();
  EXPECT_DOUBLE_EQ(restingAsk->first, 101.0);
  EXPECT_DOUBLE_EQ((restingAsk->second.begin())->getQuantity(), 1);
}

TEST(OrderbookTest, LargeBidPartialFills) {
  ServerEngineContext ctx;
  Orderbook ob(ctx);

  Response::NewOrder sellRes{};
  ob.addOrder(sellRes, Side::SELL, 100.0, 1, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  ob.addOrder(sellRes, Side::SELL, 105.0, 1, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  Response::NewOrder buyRes{};
  ob.addOrder(buyRes, Side::BUY, 105.0, 3, OrderType::LIMIT,
              TimeInForce::GOOD_TILL_CANCEL);

  EXPECT_EQ(buyRes.status, ResponseStatus::PARTIAL_FILL);

  EXPECT_EQ(ob.getAsks().size(), 0u);

  ASSERT_EQ(ob.getBids().size(), 1u);
  auto restingBid = ob.getBids().begin();
  EXPECT_DOUBLE_EQ(restingBid->first, 105.0);
  EXPECT_EQ((restingBid->second.begin())->getQuantity(), 1u);
}
