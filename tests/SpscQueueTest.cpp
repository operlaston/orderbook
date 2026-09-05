#include <gtest/gtest.h>

#include "SpscQueue.h"
#include <optional>

class SpscQueueTest : public testing::Test {
protected:
  SpscQueue<int, 2> q;
};

TEST_F(SpscQueueTest, ElementsCanBePushedAndPopped) {
  bool res = q.push(5);
  ASSERT_TRUE(res);

  auto x = q.pop();
  ASSERT_NE(x, std::nullopt);
  EXPECT_EQ(*x, 5);
}

TEST_F(SpscQueueTest, PushingToFullQueueFails) {
  q.push(1);
  q.push(2);
  EXPECT_FALSE(q.push(3));

  const auto &buffer = q.getBuffer();
  EXPECT_EQ(buffer[0], 1);
  EXPECT_EQ(buffer[1], 2);
  EXPECT_EQ(buffer[2], 0);
}

TEST_F(SpscQueueTest, PoppingFromEmptyQueueFails) {
  EXPECT_EQ(q.pop(), std::nullopt);
}

TEST_F(SpscQueueTest, ElementsWrapAround) {
  q.push(1);
  q.push(2);
  q.pop();
  q.pop();
  q.push(3);
  q.push(4);

  const auto &buffer = q.getBuffer();
  EXPECT_EQ(buffer[2], 3);
  EXPECT_EQ(buffer[0], 4);
}
