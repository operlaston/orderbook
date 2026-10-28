#include <gtest/gtest.h>

#include <Order.h>
#include <Orderbook.h>
#include <Utils.h>
#include <vector>

class OrderbookTest : public testing::Test {
protected:
  // Small pool and id map so the heap fallback and id-map growth are also
  // exercised.
  Orderbook ob{10, 10};

  NewOrderResult bid(Price price, Quantity quantity) {
    return ob.addOrder(Side::Bid, price, quantity);
  }

  NewOrderResult ask(Price price, Quantity quantity) {
    return ob.addOrder(Side::Ask, price, quantity);
  }

  const Order &bidLevelTop(Price price) { return ob.getBids()[price].front(); }
  const Order &askLevelTop(Price price) { return ob.getAsks()[price].front(); }
  size_t numBidsAtLevel(Price price) { return ob.getBids()[price].size(); }
  size_t numAsksAtLevel(Price price) { return ob.getAsks()[price].size(); }

  // Ids of the orders at a level, front (oldest) first.
  std::vector<OrderId> idsAt(const std::vector<Level> &side, Price price) {
    std::vector<OrderId> ids;
    for (const Order &o : side[price]) {
      ids.push_back(o.getId());
    }
    return ids;
  }

  Quantity quantityOf(OrderId id) {
    const Order *o = ob.findOrder(id);
    return o ? o->getQuantity() : 0;
  }

  bool bookIsEmpty() { return !ob.bestBid() && !ob.bestAsk(); }
};

// ---------------------------------------------------------------- add / match

TEST_F(OrderbookTest, BidsCanBePlaced) {
  NewOrderResult res = bid(100, 10);
  EXPECT_EQ(res.status, Status::Success);
  EXPECT_EQ(res.orderId, 1u);
  ASSERT_EQ(numBidsAtLevel(100), 1u);
  EXPECT_EQ(bidLevelTop(100).getQuantity(), 10u);
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
}

TEST_F(OrderbookTest, AsksCanBePlaced) {
  NewOrderResult res = ask(100, 10);
  EXPECT_EQ(res.status, Status::Success);
  EXPECT_EQ(res.orderId, 1u);
  ASSERT_EQ(numAsksAtLevel(100), 1u);
  EXPECT_EQ(askLevelTop(100).getQuantity(), 10u);
  EXPECT_EQ(ob.bestAsk(), 100u);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
}

TEST_F(OrderbookTest, RejectsInvalidOrdersWithoutChangingTheBook) {
  EXPECT_EQ(bid(0, 10).status, Status::BadRequest);
  EXPECT_EQ(ask(0, 10).status, Status::BadRequest);
  EXPECT_EQ(bid(Orderbook::MAX_PRICE + 1, 10).status, Status::BadRequest);
  EXPECT_EQ(ask(Orderbook::MAX_PRICE + 1, 10).status, Status::BadRequest);
  EXPECT_EQ(bid(100, 0).status, Status::BadRequest);
  EXPECT_EQ(ask(100, 0).status, Status::BadRequest);
  EXPECT_EQ(ob.addOrder(static_cast<Side>(2), 100, 10).status,
            Status::BadRequest);
  EXPECT_EQ(ob.addOrder(static_cast<Side>(255), 100, 10).orderId, 0u);

  EXPECT_TRUE(bookIsEmpty());
  // Rejections don't use up ids.
  EXPECT_EQ(bid(100, 1).orderId, 1u);
}

TEST_F(OrderbookTest, RejectedOrderDoesNotMatch) {
  ask(100, 10);
  EXPECT_EQ(bid(Orderbook::MAX_PRICE + 1, 10).status, Status::BadRequest);
  EXPECT_EQ(quantityOf(1), 10u);
}

TEST_F(OrderbookTest, MaxPriceIsAccepted) {
  NewOrderResult res = ask(Orderbook::MAX_PRICE, 5);
  EXPECT_EQ(res.status, Status::Success);
  EXPECT_EQ(ob.bestAsk(), Orderbook::MAX_PRICE);

  res = bid(Orderbook::MAX_PRICE, 5);
  EXPECT_EQ(res.status, Status::Success);
  EXPECT_EQ(res.orderId, 0u); // fully filled
  EXPECT_TRUE(bookIsEmpty());
}

TEST_F(OrderbookTest, PriceOneIsAccepted) {
  EXPECT_EQ(bid(1, 5).status, Status::Success);
  EXPECT_EQ(ob.bestBid(), 1u);
  EXPECT_EQ(ask(1, 5).orderId, 0u);
  EXPECT_TRUE(bookIsEmpty());
}

TEST_F(OrderbookTest, BidPartialFills) {
  ask(100, 10);
  NewOrderResult res = bid(110, 15);

  EXPECT_EQ(res.status, Status::Success);
  EXPECT_EQ(res.orderId, 2u);
  EXPECT_EQ(numAsksAtLevel(100), 0u);
  ASSERT_EQ(numBidsAtLevel(110), 1u);
  EXPECT_EQ(bidLevelTop(110).getQuantity(), 5u);
  EXPECT_EQ(ob.findOrder(1), nullptr);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
  EXPECT_EQ(ob.bestBid(), 110u);
}

TEST_F(OrderbookTest, AskPartialFills) {
  bid(110, 10);
  NewOrderResult res = ask(100, 15);

  EXPECT_EQ(res.orderId, 2u);
  EXPECT_EQ(numBidsAtLevel(110), 0u);
  ASSERT_EQ(numAsksAtLevel(100), 1u);
  EXPECT_EQ(askLevelTop(100).getQuantity(), 5u);
}

TEST_F(OrderbookTest, BidFullFills) {
  ask(100, 10);
  NewOrderResult res = bid(100, 10);

  EXPECT_EQ(res.status, Status::Success);
  EXPECT_EQ(res.orderId, 0u);
  EXPECT_TRUE(bookIsEmpty());
}

TEST_F(OrderbookTest, AskFullFills) {
  bid(100, 10);
  NewOrderResult res = ask(100, 10);

  EXPECT_EQ(res.status, Status::Success);
  EXPECT_EQ(res.orderId, 0u);
  EXPECT_TRUE(bookIsEmpty());
}

TEST_F(OrderbookTest, IncomingOrderPartiallyFillsResting) {
  ask(100, 10);
  EXPECT_EQ(bid(100, 4).orderId, 0u);
  EXPECT_EQ(quantityOf(1), 6u);
  EXPECT_EQ(ob.bestAsk(), 100u);
}

TEST_F(OrderbookTest, FullyFilledOrdersDoNotUseUpIds) {
  EXPECT_EQ(ask(100, 10).orderId, 1u);
  EXPECT_EQ(bid(100, 10).orderId, 0u);
  EXPECT_EQ(bid(100, 3).orderId, 2u); // next resting order gets id 2
  EXPECT_EQ(ask(105, 1).orderId, 3u);
}

TEST_F(OrderbookTest, BidFillsMultipleLevelsLowestFirst) {
  ask(102, 10); // id 1
  ask(100, 10); // id 2
  ask(101, 5);  // id 3
  ask(120, 7);  // id 4, above the bid's limit
  NewOrderResult res = bid(110, 30);

  EXPECT_EQ(numAsksAtLevel(100), 0u);
  EXPECT_EQ(numAsksAtLevel(101), 0u);
  EXPECT_EQ(numAsksAtLevel(102), 0u);
  EXPECT_EQ(quantityOf(4), 7u);
  EXPECT_EQ(res.orderId, 5u);
  EXPECT_EQ(quantityOf(5), 5u);
  EXPECT_EQ(ob.bestBid(), 110u);
  EXPECT_EQ(ob.bestAsk(), 120u);
}

TEST_F(OrderbookTest, AskFillsMultipleLevelsHighestFirst) {
  bid(108, 10); // id 1
  bid(110, 10); // id 2
  bid(109, 5);  // id 3
  bid(90, 7);   // id 4, below the ask's limit
  NewOrderResult res = ask(100, 30);

  EXPECT_EQ(numBidsAtLevel(110), 0u);
  EXPECT_EQ(numBidsAtLevel(109), 0u);
  EXPECT_EQ(numBidsAtLevel(108), 0u);
  EXPECT_EQ(quantityOf(4), 7u);
  EXPECT_EQ(quantityOf(res.orderId), 5u);
  EXPECT_EQ(ob.bestAsk(), 100u);
  EXPECT_EQ(ob.bestBid(), 90u);
}

TEST_F(OrderbookTest, PriceLevelIsFifo) {
  ask(100, 5); // id 1
  ask(100, 5); // id 2
  ask(100, 5); // id 3

  bid(100, 7); // fills all of 1, 2 of id 2
  EXPECT_EQ(ob.findOrder(1), nullptr);
  EXPECT_EQ(quantityOf(2), 3u);
  EXPECT_EQ(quantityOf(3), 5u);
  EXPECT_EQ(idsAt(ob.getAsks(), 100), (std::vector<OrderId>{2, 3}));
}

TEST_F(OrderbookTest, LaterOrdersJoinTheBackOfTheLevel) {
  bid(100, 5); // id 1
  bid(100, 5); // id 2
  ask(100, 2); // partially fills id 1, which stays at the front
  bid(100, 5); // id 3
  EXPECT_EQ(idsAt(ob.getBids(), 100), (std::vector<OrderId>{1, 2, 3}));
  EXPECT_EQ(quantityOf(1), 3u);
}

TEST_F(OrderbookTest, NonCrossingOrdersRest) {
  ask(101, 10);
  NewOrderResult res = bid(100, 10);
  EXPECT_EQ(res.orderId, 2u);
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.bestAsk(), 101u);
  EXPECT_EQ(quantityOf(1), 10u);
  EXPECT_EQ(quantityOf(2), 10u);
}

TEST_F(OrderbookTest, BestPricesTrackLevelsAcrossBitsetWords) {
  // Bitset words cover prices 0-63, 64-127, ... so these straddle boundaries.
  for (Price p : {63u, 64u, 65u, 127u, 128u, 1000u}) {
    ask(p, 1);
  }
  EXPECT_EQ(ob.bestAsk(), 63u);
  Price current = 63;
  for (Price expected : {64u, 65u, 127u, 128u, 1000u}) {
    bid(current, 1); // consumes the current best ask
    EXPECT_EQ(ob.bestAsk(), expected);
    current = expected;
  }
  bid(1000, 1);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
}

TEST_F(OrderbookTest, BestBidTracksLevelsAcrossBitsetWords) {
  for (Price p : {1u, 63u, 64u, 65u, 127u, 128u}) {
    bid(p, 1);
  }
  EXPECT_EQ(ob.bestBid(), 128u);
  for (Price expected : {127u, 65u, 64u, 63u, 1u}) {
    ask(1, 1); // consumes current best bid
    EXPECT_EQ(ob.bestBid(), expected);
  }
  ask(1, 1);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
}

TEST_F(OrderbookTest, BestPriceJumpsAcrossLargeGap) {
  bid(5, 1);
  bid(90000, 1);
  ask(90000, 1);
  EXPECT_EQ(ob.bestBid(), 5u);
  ask(10, 1);
  ask(99999, 1);
  bid(10, 1);
  EXPECT_EQ(ob.bestAsk(), 99999u);
}

TEST_F(OrderbookTest, BookCanEmptyAndRefill) {
  bid(100, 5);
  ask(100, 5);
  EXPECT_TRUE(bookIsEmpty());

  // Both sides refill correctly after going empty. The stale best prices must
  // not be used.
  EXPECT_EQ(bid(50, 5).orderId, 2u);
  EXPECT_EQ(ask(60, 5).orderId, 3u);
  EXPECT_EQ(ob.bestBid(), 50u);
  EXPECT_EQ(ob.bestAsk(), 60u);
  EXPECT_EQ(ask(50, 5).orderId, 0u);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
  EXPECT_EQ(bid(40, 1).orderId, 4u);
  EXPECT_EQ(ob.bestBid(), 40u);
}

// --------------------------------------------------------------------- cancel

TEST_F(OrderbookTest, BidCanBeCancelled) {
  bid(100, 10);
  ASSERT_NE(ob.findOrder(1), nullptr);

  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.findOrder(1), nullptr);
  EXPECT_EQ(numBidsAtLevel(100), 0u);
  EXPECT_TRUE(bookIsEmpty());
}

TEST_F(OrderbookTest, AskCanBeCancelled) {
  ask(100, 10);
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.findOrder(1), nullptr);
  EXPECT_TRUE(bookIsEmpty());
}

TEST_F(OrderbookTest, CancelBestLevelUpdatesBestPrice) {
  bid(100, 1); // 1
  bid(90, 1);  // 2
  ask(110, 1); // 3
  ask(120, 1); // 4
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.bestBid(), 90u);
  EXPECT_EQ(ob.cancelOrder(3), Status::Success);
  EXPECT_EQ(ob.bestAsk(), 120u);
}

TEST_F(OrderbookTest, CancelMiddleLevelKeepsBestPrice) {
  bid(100, 1); // 1
  bid(90, 1);  // 2
  bid(80, 1);  // 3
  EXPECT_EQ(ob.cancelOrder(2), Status::Success);
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.bestBid(), 80u); // skips the emptied level at 90
}

TEST_F(OrderbookTest, CancelMiddleOfQueueKeepsOrder) {
  ask(100, 1); // 1
  ask(100, 2); // 2
  ask(100, 3); // 3
  EXPECT_EQ(ob.cancelOrder(2), Status::Success);
  EXPECT_EQ(idsAt(ob.getAsks(), 100), (std::vector<OrderId>{1, 3}));
  EXPECT_EQ(ob.bestAsk(), 100u);
}

TEST_F(OrderbookTest, CancelOneOfSeveralAtBestLevelKeepsBestPrice) {
  bid(100, 1); // 1
  bid(100, 1); // 2
  bid(90, 1);  // 3
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.bestBid(), 100u);
}

TEST_F(OrderbookTest, CancelTwiceFails) {
  bid(100, 10);
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.cancelOrder(1), Status::BadRequest);
}

TEST_F(OrderbookTest, CannotCancelBadIds) {
  bid(100, 10);
  EXPECT_EQ(ob.cancelOrder(0), Status::BadRequest);
  EXPECT_EQ(ob.cancelOrder(2), Status::BadRequest);
  EXPECT_EQ(ob.cancelOrder(1'000'000), Status::BadRequest);
  EXPECT_EQ(ob.cancelOrder(UINT64_MAX), Status::BadRequest);
  EXPECT_EQ(quantityOf(1), 10u);
}

TEST_F(OrderbookTest, CannotCancelFilledOrder) {
  ask(100, 10); // 1
  bid(100, 10);
  EXPECT_EQ(ob.cancelOrder(1), Status::BadRequest);
}

TEST_F(OrderbookTest, IdMapGrowsPastInitialSize) {
  // Regression: with initialOrderMax = 10, id 10 used to be stored at the
  // wrong index, and every later id one slot off.
  for (Price p = 1; p <= 15; p++) {
    EXPECT_EQ(bid(p, p).orderId, static_cast<OrderId>(p));
  }
  for (OrderId id : {10u, 11u, 12u}) {
    const Order *o = ob.findOrder(id);
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->getId(), id);
    EXPECT_EQ(o->getPrice(), id);
  }
  EXPECT_EQ(ob.cancelOrder(10), Status::Success);
  EXPECT_EQ(numBidsAtLevel(10), 0u);
  EXPECT_EQ(ob.cancelOrder(11), Status::Success);
  EXPECT_EQ(numBidsAtLevel(11), 0u);
  EXPECT_EQ(ob.cancelOrder(12), Status::Success);
  EXPECT_EQ(numBidsAtLevel(12), 0u);
  EXPECT_EQ(numBidsAtLevel(13), 1u);
  EXPECT_EQ(ob.bestBid(), 15u);
}

// --------------------------------------------------------------------- modify

TEST_F(OrderbookTest, ModifyDecreaseKeepsQueuePosition) {
  ask(100, 10); // 1
  ask(100, 10); // 2
  EXPECT_EQ(ob.modifyOrder(1, 4), Status::Success);
  EXPECT_EQ(quantityOf(1), 4u);
  EXPECT_EQ(idsAt(ob.getAsks(), 100), (std::vector<OrderId>{1, 2}));

  // A partial fill hits id 1 first, proving it kept its place.
  bid(100, 6);
  EXPECT_EQ(ob.findOrder(1), nullptr);
  EXPECT_EQ(quantityOf(2), 8u);
}

TEST_F(OrderbookTest, ModifyEqualQuantityIsNoop) {
  bid(100, 10);
  EXPECT_EQ(ob.modifyOrder(1, 10), Status::Success);
  EXPECT_EQ(quantityOf(1), 10u);
}

TEST_F(OrderbookTest, ModifyIncreaseIsRejected) {
  bid(100, 10);
  EXPECT_EQ(ob.modifyOrder(1, 11), Status::BadRequest);
  EXPECT_EQ(quantityOf(1), 10u);
}

TEST_F(OrderbookTest, ModifyToZeroIsRejected) {
  bid(100, 10);
  EXPECT_EQ(ob.modifyOrder(1, 0), Status::BadRequest);
  EXPECT_EQ(quantityOf(1), 10u);
}

TEST_F(OrderbookTest, ModifyComparesAgainstRemainingQuantity) {
  bid(100, 10); // 1
  ask(100, 6);  // bid now has 4 left
  EXPECT_EQ(ob.modifyOrder(1, 5), Status::BadRequest);
  EXPECT_EQ(ob.modifyOrder(1, 4), Status::Success);
  EXPECT_EQ(ob.modifyOrder(1, 3), Status::Success);
  EXPECT_EQ(quantityOf(1), 3u);
}

TEST_F(OrderbookTest, ModifyBadIdsAreRejected) {
  bid(100, 10); // 1
  EXPECT_EQ(ob.modifyOrder(0, 1), Status::BadRequest);
  EXPECT_EQ(ob.modifyOrder(2, 1), Status::BadRequest);
  EXPECT_EQ(ob.modifyOrder(UINT64_MAX, 1), Status::BadRequest);
  ob.cancelOrder(1);
  EXPECT_EQ(ob.modifyOrder(1, 1), Status::BadRequest);
}

TEST_F(OrderbookTest, ModifyDoesNotChangeBestPrices) {
  bid(100, 10);
  ask(110, 10);
  ob.modifyOrder(1, 1);
  ob.modifyOrder(2, 1);
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.bestAsk(), 110u);
}

// ---------------------------------------------------------------- memory pool

TEST(OrderbookPoolTest, MoreOrdersThanPoolBlocksUseHeapFallback) {
  Orderbook small{4, 4};
  for (Price p = 1; p <= 20; p++) {
    EXPECT_EQ(small.addOrder(Side::Bid, p, 1).orderId,
              static_cast<OrderId>(p));
  }
  EXPECT_EQ(small.bestBid(), 20u);
  for (OrderId id = 1; id <= 20; id++) {
    EXPECT_EQ(small.cancelOrder(id), Status::Success);
  }
  EXPECT_EQ(small.bestBid(), std::nullopt);

  // Freed pool blocks are reused.
  for (Price p = 1; p <= 20; p++) {
    EXPECT_EQ(small.addOrder(Side::Ask, p, 1).status, Status::Success);
  }
  EXPECT_EQ(small.bestAsk(), 1u);
}
