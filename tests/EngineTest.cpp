#include <gtest/gtest.h>

#include <Engine.h>
#include <chrono>
#include <memory>
#include <thread>
#include <unistd.h>
#include <variant>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// Pops one response, failing the test if none arrives within the timeout.
ServerResponse popResponse(ServerEngineContext &ctx) {
  auto deadline = Clock::now() + std::chrono::seconds(5);
  while (Clock::now() < deadline) {
    if (auto res = ctx.outgoingResponses.pop()) {
      return *res;
    }
  }
  ADD_FAILURE() << "timed out waiting for a response";
  return Response::CancelOrder{-1, Status::ServerError};
}

bool waitForEventFd(int fd) {
  auto deadline = Clock::now() + std::chrono::seconds(5);
  uint64_t value;
  while (Clock::now() < deadline) {
    if (::read(fd, &value, sizeof(value)) == sizeof(value)) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST(EngineTest, ProcessesRequestsInOrderAndSignalsServer) {
  auto ctx = std::make_unique<ServerEngineContext>();
  Orderbook ob{16, 16};

  std::vector<ClientRequest> requests = {
      Request::NewOrder{5, Side::Bid, 100, 10}, // rests as id 1
      Request::NewOrder{6, Side::Ask, 100, 4},  // fully fills, id 0
      Request::ModifyOrder{5, 1, 3},
      Request::ModifyOrder{5, 1, 4}, // increase -> rejected
      Request::CancelOrder{5, 1},
      Request::CancelOrder{7, 1}, // already gone
      Request::NewOrder{8, static_cast<Side>(2), 100, 1}, // invalid side
  };
  for (const auto &req : requests) {
    ASSERT_TRUE(ctx->incomingRequests.push(req));
  }

  std::jthread engine(
      [&](std::stop_token st) { runEngine(ob, *ctx, st); });

  auto r0 = std::get<Response::NewOrder>(popResponse(*ctx));
  EXPECT_EQ(r0.sessionId, 5);
  EXPECT_EQ(r0.status, Status::Success);
  EXPECT_EQ(r0.orderId, 1u);

  auto r1 = std::get<Response::NewOrder>(popResponse(*ctx));
  EXPECT_EQ(r1.sessionId, 6);
  EXPECT_EQ(r1.status, Status::Success);
  EXPECT_EQ(r1.orderId, 0u);

  auto r2 = std::get<Response::ModifyOrder>(popResponse(*ctx));
  EXPECT_EQ(r2.status, Status::Success);
  auto r3 = std::get<Response::ModifyOrder>(popResponse(*ctx));
  EXPECT_EQ(r3.status, Status::BadRequest);

  auto r4 = std::get<Response::CancelOrder>(popResponse(*ctx));
  EXPECT_EQ(r4.sessionId, 5);
  EXPECT_EQ(r4.status, Status::Success);
  auto r5 = std::get<Response::CancelOrder>(popResponse(*ctx));
  EXPECT_EQ(r5.sessionId, 7);
  EXPECT_EQ(r5.status, Status::BadRequest);

  auto r6 = std::get<Response::NewOrder>(popResponse(*ctx));
  EXPECT_EQ(r6.sessionId, 8);
  EXPECT_EQ(r6.status, Status::BadRequest);
  EXPECT_EQ(r6.orderId, 0u);

  EXPECT_TRUE(waitForEventFd(ctx->eventFd));

  engine.request_stop();
  engine.join();
  EXPECT_EQ(ob.bestBid(), std::nullopt);
}

TEST(EngineTest, NoResponsesDroppedWhenOutgoingQueueFills) {
  auto ctx = std::make_unique<ServerEngineContext>();
  Orderbook ob{16, 16};
  // More than the response queue holds, so the engine has to wait for the
  // consumer.
  constexpr int NUM_REQUESTS = 200'000;

  std::jthread engine(
      [&](std::stop_token st) { runEngine(ob, *ctx, st); });
  std::jthread producer([&] {
    for (int i = 0; i < NUM_REQUESTS; i++) {
      ClientRequest req = Request::CancelOrder{i, 1};
      while (!ctx->incomingRequests.push(req)) {
      }
    }
  });

  // Let the response queue fill before consuming.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  for (int i = 0; i < NUM_REQUESTS; i++) {
    auto res = std::get<Response::CancelOrder>(popResponse(*ctx));
    ASSERT_EQ(res.sessionId, i);
    ASSERT_EQ(res.status, Status::BadRequest);
  }

  producer.join();
  engine.request_stop();
  engine.join();
}

TEST(EngineTest, StopsWhenIdle) {
  auto ctx = std::make_unique<ServerEngineContext>();
  Orderbook ob{16, 16};
  std::jthread engine(
      [&](std::stop_token st) { runEngine(ob, *ctx, st); });
  engine.request_stop();
  engine.join();
  SUCCEED();
}
