#include <gtest/gtest.h>

#include <Orderbook.h>
#include <Order.h>
#include <Utils.h>

class OrderbookTest : public testing::Test {
protected:
  Orderbook ob;

  OrderbookTest() : ob(10, 10) {}
  ~OrderbookTest() override = default;

  void bid(Price price, Quantity quantity) {
    ob.addOrder(Side::Bid, price, quantity);
  }

  void ask(Price price, Quantity quantity) {
    ob.addOrder(Side::Ask, price, quantity);
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

  std::optional<Level::iterator> getOrderIter(OrderId orderId) {
    return ob.getIdMap()[orderId];
  }
};

TEST_F(OrderbookTest, BidsCanBePlaced) {
  bid(100, 10);

  // make sure book is properly updated
  ASSERT_EQ(numBidsAtLevel(100), 1u);
  ASSERT_EQ(bidLevelTop(100).getQuantity(), 10);
}

TEST_F(OrderbookTest, BidPartialFills) {
  ask(100, 10);
  bid(110, 15);

  // make sure book is properly updated
  ASSERT_EQ(numAsksAtLevel(100), 0u);
  ASSERT_EQ(numBidsAtLevel(110), 1u);
  ASSERT_EQ(bidLevelTop(110).getQuantity(), 5);
}

TEST_F(OrderbookTest, BidFullFills) {
  ask(100, 10);
  bid(100, 10);

  // make sure book is properly updated
  ASSERT_EQ(numAsksAtLevel(100), 0u);
  ASSERT_EQ(numBidsAtLevel(100), 0u);
}

TEST_F(OrderbookTest, BidFillsMultiple) {
  ask(100, 10);
  ask(101, 5);
  ask(102, 10);
  bid(110, 30);

  // make sure book is properly updated
  ASSERT_EQ(numAsksAtLevel(100), 0u);
  ASSERT_EQ(numAsksAtLevel(101), 0u);
  ASSERT_EQ(numAsksAtLevel(102), 0u);
  ASSERT_EQ(numBidsAtLevel(110), 1u);
  ASSERT_EQ(bidLevelTop(110).getQuantity(), 5);
}

TEST_F(OrderbookTest, AsksCanBePlaced) {
  ask(100, 10);

  // make sure book is properly updated
  ASSERT_EQ(numAsksAtLevel(100), 1u);
  ASSERT_EQ(askLevelTop(100).getQuantity(), 10);
}

TEST_F(OrderbookTest, AskPartialFills) {
  bid(110, 10);
  ask(100, 15);

  // make sure book is properly updated
  ASSERT_EQ(numBidsAtLevel(110), 0u);
  ASSERT_EQ(numAsksAtLevel(100), 1u);
  ASSERT_EQ(askLevelTop(100).getQuantity(), 5);
}

TEST_F(OrderbookTest, AskFullFills) {
  bid(100, 10);
  ask(100, 10);

  // make sure book is properly updated
  ASSERT_EQ(numBidsAtLevel(100), 0u);
  ASSERT_EQ(numAsksAtLevel(100), 0u);
}

TEST_F(OrderbookTest, AskFillsMultiple) {
  bid(110, 10);
  bid(109, 5);
  bid(108, 10);
  ask(100, 30);

  // make sure book is properly updated
  ASSERT_EQ(numBidsAtLevel(110), 0u);
  ASSERT_EQ(numBidsAtLevel(109), 0u);
  ASSERT_EQ(numBidsAtLevel(108), 0u);
  ASSERT_EQ(numAsksAtLevel(100), 1u);
  ASSERT_EQ(askLevelTop(100).getQuantity(), 5);
}

TEST_F(OrderbookTest, BidCanBeCancelled) {
  bid(100, 10);
  ASSERT_NE(getOrderIter(1), std::nullopt);

  bool res = ob.cancelOrder(1);
  ASSERT_TRUE(res);
  ASSERT_EQ(getOrderIter(1), std::nullopt);
}

TEST_F(OrderbookTest, AskCanBeCancelled) {
  ask(100, 10);
  ASSERT_NE(getOrderIter(1), std::nullopt);

  bool res = ob.cancelOrder(1);
  ASSERT_TRUE(res);
  ASSERT_EQ(getOrderIter(1), std::nullopt);
}

TEST_F(OrderbookTest, CannotCancelBadId) {
  bid(100, 10);

  bool res = ob.cancelOrder(2);
  ASSERT_FALSE(res);
}
