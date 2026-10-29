#include "Protocol.h"
#include <gtest/gtest.h>

#include <Order.h>
#include <Orderbook.h>
#include <Utils.h>

#include <tuple>
#include <utility>
#include <vector>

using Protocol::Status;

// (id, quantity) of each order in a level, front of the queue first
using LevelContents = std::vector<std::pair<OrderId, Quantity>>;

// every resting order in the book as (side, price, id, quantity), used to check
// that rejected requests leave the book untouched
using BookSnapshot = std::vector<std::tuple<Side, Price, OrderId, Quantity>>;

LevelContents levelContents(const Level& level) {
  LevelContents out;
  for (const Order& o : level) {
    out.emplace_back(o.getId(), o.getQuantity());
  }
  return out;
}

BookSnapshot snapshot(const Orderbook& book) {
  BookSnapshot out;
  for (Price p = 0; p <= Orderbook::MAX_PRICE; p++) {
    for (const Order& o : book.getBids()[p]) {
      out.emplace_back(Side::Bid, p, o.getId(), o.getQuantity());
    }
    for (const Order& o : book.getAsks()[p]) {
      out.emplace_back(Side::Ask, p, o.getId(), o.getQuantity());
    }
  }
  return out;
}

class OrderbookTest : public testing::Test {
protected:
  Orderbook ob;

  OrderbookTest() : ob(10, 10) {}
  ~OrderbookTest() override = default;

  std::pair<OrderId, Status> bid(Price price, Quantity quantity) {
    return ob.addOrder(Side::Bid, price, quantity);
  }

  std::pair<OrderId, Status> ask(Price price, Quantity quantity) {
    return ob.addOrder(Side::Ask, price, quantity);
  }

  const Order& bidLevelTop(Price price) {
    return ob.getBids()[price].front();
  }

  const Order& askLevelTop(Price price) {
    return ob.getAsks()[price].front();
  }

  size_t numBidsAtLevel(Price price) {
    return ob.getBids()[price].size();
  }

  size_t numAsksAtLevel(Price price) {
    return ob.getAsks()[price].size();
  }

  LevelContents bidsAt(Price price) {
    return levelContents(ob.getBids()[price]);
  }

  LevelContents asksAt(Price price) {
    return levelContents(ob.getAsks()[price]);
  }

  std::optional<Level::iterator> getOrderIter(OrderId orderId) {
    return ob.getIdMap()[orderId];
  }

  // checks the id map points at a resting order with this id and quantity
  void expectResting(OrderId id, Side side, Price price, Quantity quantity) {
    ASSERT_LT(id, ob.getIdMap().size());
    auto it = getOrderIter(id);
    ASSERT_TRUE(it.has_value()) << "order " << id << " is not in the id map";
    const Order& o = **it;
    EXPECT_EQ(o.getId(), id);
    EXPECT_EQ(o.getSide(), side);
    EXPECT_EQ(o.getPrice(), price);
    EXPECT_EQ(o.getQuantity(), quantity);
  }

  void expectNotResting(OrderId id) {
    if (id < ob.getIdMap().size()) {
      EXPECT_FALSE(getOrderIter(id).has_value()) << "order " << id << " is still in the id map";
    }
  }

  void expectNotCrossed() {
    if (ob.bestBid() && ob.bestAsk()) {
      EXPECT_LT(*ob.bestBid(), *ob.bestAsk());
    }
  }
};

const std::pair<OrderId, Status> kFilled{0, Status::Success};
const std::pair<OrderId, Status> kRejected{0, Status::BadRequest};

std::pair<OrderId, Status> rested(OrderId id) {
  return {id, Status::Success};
}

// ---------------------------------------------------------------------------
// Add / match
// ---------------------------------------------------------------------------

TEST_F(OrderbookTest, BidsCanBePlaced) {
  EXPECT_EQ(bid(100, 10), rested(1));

  ASSERT_EQ(numBidsAtLevel(100), 1u);
  EXPECT_EQ(bidLevelTop(100).getQuantity(), 10u);
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
  EXPECT_TRUE(ob.getBidsBitset().get(100));
  expectResting(1, Side::Bid, 100, 10);
}

TEST_F(OrderbookTest, AsksCanBePlaced) {
  EXPECT_EQ(ask(100, 10), rested(1));

  ASSERT_EQ(numAsksAtLevel(100), 1u);
  EXPECT_EQ(askLevelTop(100).getQuantity(), 10u);
  EXPECT_EQ(ob.bestAsk(), 100u);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
  EXPECT_TRUE(ob.getAsksBitset().get(100));
  expectResting(1, Side::Ask, 100, 10);
}

TEST_F(OrderbookTest, BidPartialFills) {
  EXPECT_EQ(ask(100, 10), rested(1));
  EXPECT_EQ(bid(110, 15), rested(2));

  EXPECT_EQ(numAsksAtLevel(100), 0u);
  EXPECT_FALSE(ob.getAsksBitset().get(100));
  EXPECT_EQ(bidsAt(110), (LevelContents{{2, 5}}));
  EXPECT_EQ(ob.bestBid(), 110u);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
  expectNotResting(1);
  expectResting(2, Side::Bid, 110, 5);
}

TEST_F(OrderbookTest, AskPartialFills) {
  EXPECT_EQ(bid(110, 10), rested(1));
  EXPECT_EQ(ask(100, 15), rested(2));

  EXPECT_EQ(numBidsAtLevel(110), 0u);
  EXPECT_FALSE(ob.getBidsBitset().get(110));
  EXPECT_EQ(asksAt(100), (LevelContents{{2, 5}}));
  EXPECT_EQ(ob.bestAsk(), 100u);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
  expectNotResting(1);
  expectResting(2, Side::Ask, 100, 5);
}

TEST_F(OrderbookTest, BidFullFills) {
  EXPECT_EQ(ask(100, 10), rested(1));
  EXPECT_EQ(bid(100, 10), kFilled);

  EXPECT_EQ(numAsksAtLevel(100), 0u);
  EXPECT_EQ(numBidsAtLevel(100), 0u);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
  expectNotResting(1);
}

TEST_F(OrderbookTest, AskFullFills) {
  EXPECT_EQ(bid(100, 10), rested(1));
  EXPECT_EQ(ask(100, 10), kFilled);

  EXPECT_EQ(numBidsAtLevel(100), 0u);
  EXPECT_EQ(numAsksAtLevel(100), 0u);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
  expectNotResting(1);
}

TEST_F(OrderbookTest, IncomingSmallerThanRestingLeavesRemainder) {
  EXPECT_EQ(ask(100, 10), rested(1));
  EXPECT_EQ(bid(100, 4), kFilled);

  EXPECT_EQ(asksAt(100), (LevelContents{{1, 6}}));
  EXPECT_EQ(ob.bestAsk(), 100u);
  expectResting(1, Side::Ask, 100, 6);
}

TEST_F(OrderbookTest, BidSweepsAsksLowestFirst) {
  // added out of price order so the sweep order can't come from insertion order
  EXPECT_EQ(ask(102, 10), rested(1));
  EXPECT_EQ(ask(100, 10), rested(2));
  EXPECT_EQ(ask(101, 5), rested(3));
  EXPECT_EQ(bid(110, 30), rested(4));

  EXPECT_EQ(numAsksAtLevel(100), 0u);
  EXPECT_EQ(numAsksAtLevel(101), 0u);
  EXPECT_EQ(numAsksAtLevel(102), 0u);
  EXPECT_EQ(bidsAt(110), (LevelContents{{4, 5}}));
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
  EXPECT_EQ(ob.bestBid(), 110u);
}

TEST_F(OrderbookTest, BidSweepStopsPartwayThroughLevel) {
  EXPECT_EQ(ask(102, 10), rested(1));
  EXPECT_EQ(ask(100, 10), rested(2));
  EXPECT_EQ(ask(101, 5), rested(3));
  // takes all of 100 and 101, then 3 of the 10 at 102
  EXPECT_EQ(bid(105, 18), kFilled);

  EXPECT_EQ(numAsksAtLevel(100), 0u);
  EXPECT_EQ(numAsksAtLevel(101), 0u);
  EXPECT_EQ(asksAt(102), (LevelContents{{1, 7}}));
  EXPECT_EQ(ob.bestAsk(), 102u);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
}

TEST_F(OrderbookTest, AskSweepsBidsHighestFirst) {
  EXPECT_EQ(bid(108, 10), rested(1));
  EXPECT_EQ(bid(110, 10), rested(2));
  EXPECT_EQ(bid(109, 5), rested(3));
  EXPECT_EQ(ask(100, 30), rested(4));

  EXPECT_EQ(numBidsAtLevel(110), 0u);
  EXPECT_EQ(numBidsAtLevel(109), 0u);
  EXPECT_EQ(numBidsAtLevel(108), 0u);
  EXPECT_EQ(asksAt(100), (LevelContents{{4, 5}}));
  EXPECT_EQ(ob.bestBid(), std::nullopt);
  EXPECT_EQ(ob.bestAsk(), 100u);
}

TEST_F(OrderbookTest, AskSweepStopsPartwayThroughLevel) {
  EXPECT_EQ(bid(108, 10), rested(1));
  EXPECT_EQ(bid(110, 10), rested(2));
  EXPECT_EQ(bid(109, 5), rested(3));
  // takes all of 110 and 109, then 3 of the 10 at 108
  EXPECT_EQ(ask(100, 18), kFilled);

  EXPECT_EQ(numBidsAtLevel(110), 0u);
  EXPECT_EQ(numBidsAtLevel(109), 0u);
  EXPECT_EQ(bidsAt(108), (LevelContents{{1, 7}}));
  EXPECT_EQ(ob.bestBid(), 108u);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
}

TEST_F(OrderbookTest, BidDoesNotTradeThroughItsLimit) {
  EXPECT_EQ(ask(100, 5), rested(1));
  EXPECT_EQ(ask(105, 5), rested(2));
  EXPECT_EQ(bid(103, 20), rested(3));

  EXPECT_EQ(numAsksAtLevel(100), 0u);
  EXPECT_EQ(asksAt(105), (LevelContents{{2, 5}}));
  EXPECT_EQ(bidsAt(103), (LevelContents{{3, 15}}));
  EXPECT_EQ(ob.bestBid(), 103u);
  EXPECT_EQ(ob.bestAsk(), 105u);
  expectNotCrossed();
}

TEST_F(OrderbookTest, AskDoesNotTradeThroughItsLimit) {
  EXPECT_EQ(bid(105, 5), rested(1));
  EXPECT_EQ(bid(100, 5), rested(2));
  EXPECT_EQ(ask(103, 20), rested(3));

  EXPECT_EQ(numBidsAtLevel(105), 0u);
  EXPECT_EQ(bidsAt(100), (LevelContents{{2, 5}}));
  EXPECT_EQ(asksAt(103), (LevelContents{{3, 15}}));
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.bestAsk(), 103u);
  expectNotCrossed();
}

TEST_F(OrderbookTest, BidBelowBestAskDoesNotMatch) {
  EXPECT_EQ(ask(100, 10), rested(1));
  EXPECT_EQ(bid(99, 10), rested(2));

  EXPECT_EQ(asksAt(100), (LevelContents{{1, 10}}));
  EXPECT_EQ(bidsAt(99), (LevelContents{{2, 10}}));
  EXPECT_EQ(ob.bestBid(), 99u);
  EXPECT_EQ(ob.bestAsk(), 100u);
  expectNotCrossed();
}

TEST_F(OrderbookTest, AskAboveBestBidDoesNotMatch) {
  EXPECT_EQ(bid(100, 10), rested(1));
  EXPECT_EQ(ask(101, 10), rested(2));

  EXPECT_EQ(bidsAt(100), (LevelContents{{1, 10}}));
  EXPECT_EQ(asksAt(101), (LevelContents{{2, 10}}));
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.bestAsk(), 101u);
  expectNotCrossed();
}

TEST_F(OrderbookTest, FifoWithinAskLevel) {
  EXPECT_EQ(ask(100, 10), rested(1));
  EXPECT_EQ(ask(100, 10), rested(2));
  EXPECT_EQ(bid(100, 15), kFilled);

  // the first ask is filled completely before the second is touched
  EXPECT_EQ(asksAt(100), (LevelContents{{2, 5}}));
  expectNotResting(1);
  expectResting(2, Side::Ask, 100, 5);
}

TEST_F(OrderbookTest, FifoWithinBidLevel) {
  EXPECT_EQ(bid(100, 10), rested(1));
  EXPECT_EQ(bid(100, 10), rested(2));
  EXPECT_EQ(bid(100, 10), rested(3));
  EXPECT_EQ(ask(100, 5), kFilled);

  EXPECT_EQ(bidsAt(100), (LevelContents{{1, 5}, {2, 10}, {3, 10}}));
}

TEST_F(OrderbookTest, NewOrdersJoinBackOfLevel) {
  EXPECT_EQ(ask(100, 10), rested(1));
  EXPECT_EQ(ask(100, 10), rested(2));
  EXPECT_EQ(bid(100, 5), kFilled);    // order 1 now has 5 left but keeps its place
  EXPECT_EQ(ask(100, 10), rested(3));

  EXPECT_EQ(asksAt(100), (LevelContents{{1, 5}, {2, 10}, {3, 10}}));
}

TEST_F(OrderbookTest, ReturnedIdsSkipFullyFilledOrders) {
  EXPECT_EQ(ask(100, 10), rested(1));
  EXPECT_EQ(bid(100, 10), kFilled);   // fills in full, doesn't use up id 2
  EXPECT_EQ(bid(90, 10), rested(2));
  EXPECT_EQ(ask(95, 5), rested(3));
  EXPECT_EQ(bid(95, 5), kFilled);
  EXPECT_EQ(bid(95, 1), rested(4));

  expectResting(2, Side::Bid, 90, 10);
  expectResting(4, Side::Bid, 95, 1);
}

TEST_F(OrderbookTest, PriceAtMaxIsAccepted) {
  EXPECT_EQ(ask(Orderbook::MAX_PRICE, 10), rested(1));
  EXPECT_EQ(ob.bestAsk(), Orderbook::MAX_PRICE);
  EXPECT_TRUE(ob.getAsksBitset().get(Orderbook::MAX_PRICE));

  EXPECT_EQ(bid(Orderbook::MAX_PRICE, 4), kFilled);
  EXPECT_EQ(asksAt(Orderbook::MAX_PRICE), (LevelContents{{1, 6}}));

  EXPECT_EQ(bid(Orderbook::MAX_PRICE, 10), rested(2));
  EXPECT_EQ(ob.bestBid(), Orderbook::MAX_PRICE);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
}

TEST_F(OrderbookTest, PriceAboveMaxIsRejected) {
  bid(100, 10);
  ask(200, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(bid(Orderbook::MAX_PRICE + 1, 10), kRejected);
  EXPECT_EQ(ask(Orderbook::MAX_PRICE + 1, 10), kRejected);

  EXPECT_EQ(snapshot(ob), before);
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.bestAsk(), 200u);
  EXPECT_EQ(bid(101, 1), rested(3));  // rejected orders don't use up ids
}

TEST_F(OrderbookTest, ZeroQuantityIsRejected) {
  bid(100, 10);
  ask(200, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(bid(150, 0), kRejected);
  EXPECT_EQ(ask(150, 0), kRejected);
  // would cross the book if it were accepted
  EXPECT_EQ(bid(200, 0), kRejected);

  EXPECT_EQ(snapshot(ob), before);
  EXPECT_EQ(bid(101, 1), rested(3));
}

// SPEC §2.2: price 0 is used as "no price" by PriceBitset, so it must never rest
TEST_F(OrderbookTest, ZeroPriceIsRejected) {
  bid(100, 10);
  ask(200, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(bid(0, 10), kRejected);
  EXPECT_EQ(ask(0, 10), kRejected);

  EXPECT_EQ(snapshot(ob), before);
  EXPECT_EQ(ob.bestBid(), 100u);
  EXPECT_EQ(ob.bestAsk(), 200u);
  EXPECT_EQ(bid(101, 1), rested(3));
}

// SPEC §2.2: a side that is neither Bid nor Ask is rejected
TEST_F(OrderbookTest, InvalidSideIsRejected) {
  bid(100, 10);
  ask(200, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(ob.addOrder(static_cast<Side>(2), 150, 10), kRejected);

  EXPECT_EQ(snapshot(ob), before);
  EXPECT_EQ(bid(101, 1), rested(3));
}

TEST_F(OrderbookTest, BestAskUpdatesAcrossBitsetWords) {
  // 63 | 64 is a word boundary in PriceBitset
  ask(65, 1);
  ask(64, 1);
  ask(63, 1);
  EXPECT_EQ(ob.bestAsk(), 63u);

  EXPECT_EQ(bid(200, 1), kFilled);
  EXPECT_EQ(ob.bestAsk(), 64u);
  EXPECT_EQ(bid(200, 1), kFilled);
  EXPECT_EQ(ob.bestAsk(), 65u);
  EXPECT_EQ(bid(200, 1), kFilled);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
}

TEST_F(OrderbookTest, BestBidUpdatesAcrossBitsetWords) {
  // 127 | 128 is a word boundary in PriceBitset
  bid(127, 1);
  bid(128, 1);
  bid(63, 1);
  EXPECT_EQ(ob.bestBid(), 128u);

  EXPECT_EQ(ask(1, 1), kFilled);
  EXPECT_EQ(ob.bestBid(), 127u);
  EXPECT_EQ(ask(1, 1), kFilled);
  EXPECT_EQ(ob.bestBid(), 63u);
  EXPECT_EQ(ask(1, 1), kFilled);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
}

TEST_F(OrderbookTest, BestPriceJumpsToFarAwayLevel) {
  bid(5, 1);
  bid(90000, 1);
  ask(90001, 1);
  ask(99999, 1);

  EXPECT_EQ(ask(90000, 1), kFilled);
  EXPECT_EQ(ob.bestBid(), 5u);
  EXPECT_EQ(bid(90001, 1), kFilled);
  EXPECT_EQ(ob.bestAsk(), 99999u);

  // the new best prices are used for matching, not just reported
  EXPECT_EQ(ask(5, 1), kFilled);
  EXPECT_EQ(bid(99999, 1), kFilled);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
}

TEST_F(OrderbookTest, LowestValidPriceWorksOnBothSides) {
  EXPECT_EQ(bid(1, 5), rested(1));
  EXPECT_EQ(bid(70, 5), rested(2));
  EXPECT_EQ(ask(70, 5), kFilled);
  EXPECT_EQ(ob.bestBid(), 1u);

  EXPECT_EQ(ask(1, 5), kFilled);
  EXPECT_EQ(ob.bestBid(), std::nullopt);

  EXPECT_EQ(ask(1, 5), rested(3));
  EXPECT_EQ(ob.bestAsk(), 1u);
}

TEST_F(OrderbookTest, BookEmptiesThenRefills) {
  bid(100, 10);
  bid(99, 10);
  ask(105, 10);
  ask(106, 10);

  EXPECT_EQ(ask(1, 20), kFilled);
  EXPECT_EQ(bid(200, 20), kFilled);
  EXPECT_EQ(ob.bestBid(), std::nullopt);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
  EXPECT_TRUE(snapshot(ob).empty());
  for (Price p : {99u, 100u, 105u, 106u}) {
    EXPECT_FALSE(ob.getBidsBitset().get(p));
    EXPECT_FALSE(ob.getAsksBitset().get(p));
  }

  // new levels on the far side of where the old best prices were
  EXPECT_EQ(bid(50, 3), rested(5));
  EXPECT_EQ(ask(300, 4), rested(6));
  EXPECT_EQ(ob.bestBid(), 50u);
  EXPECT_EQ(ob.bestAsk(), 300u);

  EXPECT_EQ(bid(40, 3), rested(7));
  EXPECT_EQ(ask(400, 4), rested(8));
  EXPECT_EQ(ob.bestBid(), 50u);
  EXPECT_EQ(ob.bestAsk(), 300u);

  EXPECT_EQ(ask(50, 3), kFilled);
  EXPECT_EQ(ob.bestBid(), 40u);
  EXPECT_EQ(bid(300, 4), kFilled);
  EXPECT_EQ(ob.bestAsk(), 400u);
}

// ---------------------------------------------------------------------------
// Cancel
// ---------------------------------------------------------------------------

TEST_F(OrderbookTest, BidCanBeCancelled) {
  bid(100, 10);
  ASSERT_NE(getOrderIter(1), std::nullopt);

  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(getOrderIter(1), std::nullopt);
  EXPECT_EQ(numBidsAtLevel(100), 0u);
  EXPECT_FALSE(ob.getBidsBitset().get(100));
  EXPECT_EQ(ob.bestBid(), std::nullopt);
}

TEST_F(OrderbookTest, AskCanBeCancelled) {
  ask(100, 10);
  ASSERT_NE(getOrderIter(1), std::nullopt);

  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(getOrderIter(1), std::nullopt);
  EXPECT_EQ(numAsksAtLevel(100), 0u);
  EXPECT_FALSE(ob.getAsksBitset().get(100));
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
}

TEST_F(OrderbookTest, CancelledOrderIsNotMatched) {
  ask(100, 10);
  ask(101, 10);
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);

  EXPECT_EQ(bid(100, 10), rested(3));
  EXPECT_EQ(asksAt(101), (LevelContents{{2, 10}}));
}

TEST_F(OrderbookTest, CancelBestBidLevelUpdatesBestBid) {
  bid(100, 1);  // 1
  bid(101, 1);  // 2
  bid(102, 1);  // 3

  EXPECT_EQ(ob.cancelOrder(3), Status::Success);
  EXPECT_EQ(ob.bestBid(), 101u);
  EXPECT_FALSE(ob.getBidsBitset().get(102));
  EXPECT_EQ(bidsAt(101), (LevelContents{{2, 1}}));
  EXPECT_EQ(bidsAt(100), (LevelContents{{1, 1}}));
}

TEST_F(OrderbookTest, CancelBestAskLevelUpdatesBestAsk) {
  ask(100, 1);  // 1
  ask(101, 1);  // 2
  ask(102, 1);  // 3

  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.bestAsk(), 101u);
  EXPECT_FALSE(ob.getAsksBitset().get(100));
}

TEST_F(OrderbookTest, CancelBestAskAcrossBitsetWord) {
  ask(127, 1);  // 1
  ask(128, 1);  // 2
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.bestAsk(), 128u);
  EXPECT_EQ(bid(127, 1), rested(3));
  expectNotCrossed();
}

TEST_F(OrderbookTest, CancelBestBidAcrossBitsetWord) {
  bid(63, 1);   // 1
  bid(64, 1);   // 2
  EXPECT_EQ(ob.cancelOrder(2), Status::Success);
  EXPECT_EQ(ob.bestBid(), 63u);
  EXPECT_EQ(ask(64, 1), rested(3));
  expectNotCrossed();
}

TEST_F(OrderbookTest, CancelMiddleLevelKeepsBest) {
  bid(100, 1);  // 1
  bid(101, 1);  // 2
  bid(102, 1);  // 3

  EXPECT_EQ(ob.cancelOrder(2), Status::Success);
  EXPECT_EQ(ob.bestBid(), 102u);
  EXPECT_FALSE(ob.getBidsBitset().get(101));

  // once the best level goes, the cancelled middle level must be skipped
  EXPECT_EQ(ob.cancelOrder(3), Status::Success);
  EXPECT_EQ(ob.bestBid(), 100u);
}

TEST_F(OrderbookTest, CancelMiddleOfQueueKeepsOthersInOrder) {
  ask(100, 1);  // 1
  ask(100, 2);  // 2
  ask(100, 3);  // 3

  EXPECT_EQ(ob.cancelOrder(2), Status::Success);
  EXPECT_EQ(asksAt(100), (LevelContents{{1, 1}, {3, 3}}));
  EXPECT_TRUE(ob.getAsksBitset().get(100));
  EXPECT_EQ(ob.bestAsk(), 100u);

  EXPECT_EQ(bid(100, 2), kFilled);
  EXPECT_EQ(asksAt(100), (LevelContents{{3, 2}}));
}

TEST_F(OrderbookTest, CancelFrontOfQueueKeepsLevel) {
  bid(100, 1);  // 1
  bid(100, 2);  // 2

  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(bidsAt(100), (LevelContents{{2, 2}}));
  EXPECT_TRUE(ob.getBidsBitset().get(100));
  EXPECT_EQ(ob.bestBid(), 100u);
}

TEST_F(OrderbookTest, CancelPartiallyFilledOrder) {
  ask(100, 10);
  EXPECT_EQ(bid(100, 4), kFilled);
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.bestAsk(), std::nullopt);
  EXPECT_TRUE(snapshot(ob).empty());
}

TEST_F(OrderbookTest, CannotCancelBadId) {
  bid(100, 10);

  EXPECT_EQ(ob.cancelOrder(2), Status::BadRequest);
  expectResting(1, Side::Bid, 100, 10);
}

TEST_F(OrderbookTest, CancelTwiceFails) {
  bid(100, 10);
  bid(100, 5);
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.cancelOrder(1), Status::BadRequest);
  EXPECT_EQ(bidsAt(100), (LevelContents{{2, 5}}));
}

TEST_F(OrderbookTest, CancelIdZeroFails) {
  bid(100, 10);
  EXPECT_EQ(ob.cancelOrder(0), Status::BadRequest);
  expectResting(1, Side::Bid, 100, 10);
}

TEST_F(OrderbookTest, CancelIdPastEndOfMapFails) {
  bid(100, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(ob.cancelOrder(ob.getIdMap().size()), Status::BadRequest);
  EXPECT_EQ(ob.cancelOrder(1'000'000), Status::BadRequest);
  EXPECT_EQ(ob.cancelOrder(UINT64_MAX), Status::BadRequest);
  EXPECT_EQ(snapshot(ob), before);
}

TEST_F(OrderbookTest, CancelFullyFilledOrderFails) {
  ask(100, 10);                 // 1
  EXPECT_EQ(bid(100, 10), kFilled);
  EXPECT_EQ(ob.cancelOrder(1), Status::BadRequest);
  EXPECT_TRUE(snapshot(ob).empty());
}

// SPEC §2.4.1: id initialOrderMax and later must still map to the right order
// after the id map grows past its initial size
TEST_F(OrderbookTest, CancelWorksAfterIdMapGrows) {
  // the fixture is Orderbook(10, 10); price 100 + id makes each order easy to find
  for (OrderId id = 1; id <= 13; id++) {
    ASSERT_EQ(bid(static_cast<Price>(100 + id), 1), rested(id));
  }
  for (OrderId id = 1; id <= 13; id++) {
    expectResting(id, Side::Bid, static_cast<Price>(100 + id), 1);
  }

  for (OrderId id : {10u, 11u, 12u}) {
    EXPECT_EQ(ob.cancelOrder(id), Status::Success) << "cancelling id " << id;
    EXPECT_EQ(numBidsAtLevel(static_cast<Price>(100 + id)), 0u) << "cancelling id " << id;
    expectNotResting(id);
  }
  // nothing else was removed
  for (OrderId id : {1u, 2u, 9u, 13u}) {
    expectResting(id, Side::Bid, static_cast<Price>(100 + id), 1);
  }
  EXPECT_EQ(ob.bestBid(), 113u);
  EXPECT_EQ(ob.cancelOrder(13), Status::Success);
  EXPECT_EQ(ob.bestBid(), 109u);
}

// ---------------------------------------------------------------------------
// Modify
// ---------------------------------------------------------------------------

TEST_F(OrderbookTest, ModifyDecreaseKeepsQueuePosition) {
  ask(100, 10);  // 1
  ask(100, 10);  // 2

  EXPECT_EQ(ob.modifyOrder(1, 4), Status::Success);
  EXPECT_EQ(asksAt(100), (LevelContents{{1, 4}, {2, 10}}));
  expectResting(1, Side::Ask, 100, 4);

  // order 1 is still first in line
  EXPECT_EQ(bid(100, 6), kFilled);
  EXPECT_EQ(asksAt(100), (LevelContents{{2, 8}}));
  expectNotResting(1);
}

TEST_F(OrderbookTest, ModifyDecreaseOnBid) {
  bid(100, 10);  // 1
  bid(101, 10);  // 2

  EXPECT_EQ(ob.modifyOrder(2, 1), Status::Success);
  EXPECT_EQ(ob.bestBid(), 101u);
  EXPECT_EQ(ask(100, 5), kFilled);
  EXPECT_EQ(bidsAt(101), (LevelContents{}));
  EXPECT_EQ(bidsAt(100), (LevelContents{{1, 6}}));
}

// SPEC §2.3: equal quantity is a no-op that succeeds
TEST_F(OrderbookTest, ModifyEqualQuantitySucceedsWithoutChange) {
  ask(100, 10);
  ask(100, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(ob.modifyOrder(1, 10), Status::Success);
  EXPECT_EQ(snapshot(ob), before);
}

TEST_F(OrderbookTest, ModifyIncreaseFails) {
  ask(100, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(ob.modifyOrder(1, 11), Status::BadRequest);
  EXPECT_EQ(snapshot(ob), before);
}

TEST_F(OrderbookTest, ModifyToZeroFails) {
  ask(100, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(ob.modifyOrder(1, 0), Status::BadRequest);
  EXPECT_EQ(snapshot(ob), before);
}

TEST_F(OrderbookTest, ModifyComparesAgainstRemainingQuantity) {
  ask(100, 10);                      // 1
  EXPECT_EQ(bid(100, 4), kFilled);   // 6 left

  EXPECT_EQ(ob.modifyOrder(1, 8), Status::BadRequest);
  EXPECT_EQ(ob.modifyOrder(1, 7), Status::BadRequest);
  expectResting(1, Side::Ask, 100, 6);

  EXPECT_EQ(ob.modifyOrder(1, 5), Status::Success);
  expectResting(1, Side::Ask, 100, 5);
}

TEST_F(OrderbookTest, ModifyRemainingQuantityExactlySucceeds) {
  ask(100, 10);
  EXPECT_EQ(bid(100, 4), kFilled);
  EXPECT_EQ(ob.modifyOrder(1, 6), Status::Success);
  expectResting(1, Side::Ask, 100, 6);
}

TEST_F(OrderbookTest, ModifyUnknownIdFails) {
  ask(100, 10);
  BookSnapshot before = snapshot(ob);

  EXPECT_EQ(ob.modifyOrder(0, 5), Status::BadRequest);
  EXPECT_EQ(ob.modifyOrder(2, 5), Status::BadRequest);
  EXPECT_EQ(ob.modifyOrder(ob.getIdMap().size(), 5), Status::BadRequest);
  EXPECT_EQ(ob.modifyOrder(UINT64_MAX, 5), Status::BadRequest);
  EXPECT_EQ(snapshot(ob), before);
}

TEST_F(OrderbookTest, ModifyCancelledOrderFails) {
  ask(100, 10);
  EXPECT_EQ(ob.cancelOrder(1), Status::Success);
  EXPECT_EQ(ob.modifyOrder(1, 5), Status::BadRequest);
}

TEST_F(OrderbookTest, ModifyFilledOrderFails) {
  ask(100, 10);
  EXPECT_EQ(bid(100, 10), kFilled);
  EXPECT_EQ(ob.modifyOrder(1, 5), Status::BadRequest);
}

TEST_F(OrderbookTest, ModifyWorksAfterIdMapGrows) {
  for (OrderId id = 1; id <= 12; id++) {
    ASSERT_EQ(ask(static_cast<Price>(100 + id), 10), rested(id));
  }
  for (OrderId id : {10u, 11u, 12u}) {
    EXPECT_EQ(ob.modifyOrder(id, id - 5), Status::Success) << "modifying id " << id;
    expectResting(id, Side::Ask, static_cast<Price>(100 + id), id - 5);
  }
  expectResting(9, Side::Ask, 109, 10);
}

// ---------------------------------------------------------------------------
// Memory pool
// ---------------------------------------------------------------------------

// with more resting orders than pool blocks, the allocator falls back to the heap
TEST(OrderbookPoolTest, MoreOrdersThanBlocks) {
  Orderbook book(4, 4);

  for (OrderId id = 1; id <= 20; id++) {
    // half share one level, half are on their own levels
    Price price = id % 2 == 0 ? 500 : static_cast<Price>(100 + id);
    ASSERT_EQ(book.addOrder(Side::Bid, price, id), (std::pair<OrderId, Status>{id, Status::Success}));
  }
  ASSERT_EQ(book.getBids()[500].size(), 10u);
  EXPECT_EQ(book.bestBid(), 500u);

  for (OrderId id = 1; id <= 20; id++) {
    EXPECT_EQ(book.cancelOrder(id), Status::Success) << "cancelling id " << id;
  }
  EXPECT_EQ(book.bestBid(), std::nullopt);
  EXPECT_TRUE(snapshot(book).empty());
}

TEST(OrderbookPoolTest, BlocksAreReusedAfterFills) {
  Orderbook book(4, 4);

  // repeatedly fill the pool, then empty it by matching, mixing pool and heap nodes
  OrderId nextId = 1;
  for (int round = 0; round < 50; round++) {
    for (int i = 0; i < 6; i++) {
      ASSERT_EQ(book.addOrder(Side::Ask, static_cast<Price>(100 + i), 2),
                (std::pair<OrderId, Status>{nextId, Status::Success}));
      nextId++;
    }
    ASSERT_EQ(book.addOrder(Side::Bid, 200, 12), (std::pair<OrderId, Status>{0, Status::Success}));
    ASSERT_EQ(book.bestAsk(), std::nullopt);
  }
  EXPECT_TRUE(snapshot(book).empty());
}
