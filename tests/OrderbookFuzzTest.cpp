// Drives Orderbook and a small, obviously-correct reference book with the same
// random add/cancel/modify sequence and checks they agree after every operation.

#include "Protocol.h"
#include <gtest/gtest.h>
#include "TestPrinters.h"

#include <Order.h>
#include <Orderbook.h>
#include <Utils.h>

#include <deque>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

using Protocol::Status;

namespace {

struct RefOrder {
  OrderId id;
  Quantity quantity;
};

// Reference implementation of the rules in SPEC.md §2: price-time priority,
// ids only used up when an order rests, modify may only shrink an order.
class ReferenceBook {
public:
  std::map<Price, std::deque<RefOrder>> bids;
  std::map<Price, std::deque<RefOrder>> asks;

  std::pair<OrderId, Status> add(Side side, Price price, Quantity quantity) {
    if (price == 0 || price > Orderbook::MAX_PRICE || quantity == 0 ||
        (side != Side::Bid && side != Side::Ask)) {
      return {0, Status::BadRequest};
    }

    auto& opposite = side == Side::Bid ? asks : bids;
    while (quantity > 0 && !opposite.empty()) {
      // best opposite level: lowest ask, or highest bid
      auto levelIt = side == Side::Bid ? opposite.begin() : std::prev(opposite.end());
      bool crosses = side == Side::Bid ? levelIt->first <= price : levelIt->first >= price;
      if (!crosses) break;

      RefOrder& front = levelIt->second.front();
      Quantity filled = std::min(quantity, front.quantity);
      quantity -= filled;
      front.quantity -= filled;
      if (front.quantity == 0) {
        live.erase(front.id);
        levelIt->second.pop_front();
        if (levelIt->second.empty()) opposite.erase(levelIt);
      }
    }

    if (quantity == 0) return {0, Status::Success};

    OrderId id = nextId++;
    (side == Side::Bid ? bids : asks)[price].push_back({id, quantity});
    live[id] = {side, price};
    return {id, Status::Success};
  }

  Status cancel(OrderId id) {
    auto it = live.find(id);
    if (it == live.end()) return Status::BadRequest;
    auto [side, price] = it->second;
    auto& book = side == Side::Bid ? bids : asks;
    auto& level = book[price];
    for (auto o = level.begin(); o != level.end(); ++o) {
      if (o->id == id) {
        level.erase(o);
        break;
      }
    }
    if (level.empty()) book.erase(price);
    live.erase(it);
    return Status::Success;
  }

  Status modify(OrderId id, Quantity newQuantity) {
    RefOrder* o = find(id);
    if (o == nullptr || newQuantity == 0 || newQuantity > o->quantity) {
      return Status::BadRequest;
    }
    o->quantity = newQuantity;
    return Status::Success;
  }

  std::optional<Price> bestBid() const {
    return bids.empty() ? std::nullopt : std::optional<Price>(bids.rbegin()->first);
  }

  std::optional<Price> bestAsk() const {
    return asks.empty() ? std::nullopt : std::optional<Price>(asks.begin()->first);
  }

  OrderId peekNextId() const { return nextId; }

  // a random id that is currently resting, or 0 if none
  OrderId randomLiveId(std::mt19937_64& rng) const {
    if (live.empty()) return 0;
    auto it = live.begin();
    std::advance(it, rng() % live.size());
    return it->first;
  }

private:
  std::unordered_map<OrderId, std::pair<Side, Price>> live;
  OrderId nextId{1};

  RefOrder* find(OrderId id) {
    auto it = live.find(id);
    if (it == live.end()) return nullptr;
    auto [side, price] = it->second;
    for (RefOrder& o : (side == Side::Bid ? bids : asks)[price]) {
      if (o.id == id) return &o;
    }
    return nullptr;
  }
};

// Orderbook's reject paths print to stdout; keep 100k ops from flooding the log
class SilenceStdout {
public:
  SilenceStdout() : m_old(std::cout.rdbuf(m_sink.rdbuf())) {}
  ~SilenceStdout() { std::cout.rdbuf(m_old); }

private:
  std::ostringstream m_sink;
  std::streambuf* m_old;
};

std::string describeLevel(const Level& level) {
  std::ostringstream out;
  for (const Order& o : level) out << "(" << o.getId() << "," << o.getQuantity() << ")";
  return out.str();
}

std::string describeLevel(const std::deque<RefOrder>& level) {
  std::ostringstream out;
  for (const RefOrder& o : level) out << "(" << o.id << "," << o.quantity << ")";
  return out.str();
}

bool sameLevel(const Level& actual, const std::deque<RefOrder>* expected) {
  if (expected == nullptr) return actual.empty();
  if (actual.size() != expected->size()) return false;
  auto e = expected->begin();
  for (const Order& o : actual) {
    if (o.getId() != e->id || o.getQuantity() != e->quantity) return false;
    ++e;
  }
  return true;
}

// Prices straddle the PriceBitset word boundary at 128 and are close together
// so orders cross often.
constexpr Price kMinPrice = 110;
constexpr Price kMaxPrice = 145;

void compareBooks(const Orderbook& ob, const ReferenceBook& ref, int op) {
  ASSERT_EQ(ob.bestBid(), ref.bestBid()) << "after op " << op;
  ASSERT_EQ(ob.bestAsk(), ref.bestAsk()) << "after op " << op;

  for (Price p = kMinPrice - 1; p <= kMaxPrice + 1; p++) {
    auto bidIt = ref.bids.find(p);
    auto askIt = ref.asks.find(p);
    const std::deque<RefOrder>* refBid = bidIt == ref.bids.end() ? nullptr : &bidIt->second;
    const std::deque<RefOrder>* refAsk = askIt == ref.asks.end() ? nullptr : &askIt->second;

    ASSERT_TRUE(sameLevel(ob.getBids()[p], refBid))
        << "bid level " << p << " after op " << op << ": got " << describeLevel(ob.getBids()[p])
        << ", want " << (refBid ? describeLevel(*refBid) : "");
    ASSERT_TRUE(sameLevel(ob.getAsks()[p], refAsk))
        << "ask level " << p << " after op " << op << ": got " << describeLevel(ob.getAsks()[p])
        << ", want " << (refAsk ? describeLevel(*refAsk) : "");

    // the bitset bit must be set exactly when the level has orders
    ASSERT_EQ(ob.getBidsBitset().get(p), refBid != nullptr) << "bid bit " << p << " after op " << op;
    ASSERT_EQ(ob.getAsksBitset().get(p), refAsk != nullptr) << "ask bit " << p << " after op " << op;
  }
}

void runFuzz(uint64_t seed, uint64_t blockCount, uint32_t initialOrderMax, int numOps) {
  SilenceStdout silence;
  std::mt19937_64 rng(seed);
  Orderbook ob(blockCount, initialOrderMax);
  ReferenceBook ref;

  auto randPrice = [&] { return static_cast<Price>(kMinPrice + rng() % (kMaxPrice - kMinPrice + 1)); };
  auto randQuantity = [&] { return static_cast<Quantity>(1 + rng() % 20); };

  for (int op = 0; op < numOps; op++) {
    uint64_t kind = rng() % 100;

    if (kind < 55) {
      Side side = rng() % 2 == 0 ? Side::Bid : Side::Ask;
      Price price = randPrice();
      Quantity quantity = randQuantity();
      auto expected = ref.add(side, price, quantity);
      auto actual = ob.addOrder(side, price, quantity);
      ASSERT_EQ(actual, expected) << "add side=" << static_cast<int>(side) << " price=" << price
                                  << " qty=" << quantity << " at op " << op << " seed " << seed;
    }
    else if (kind < 58) {
      // invalid new orders
      Side side = rng() % 2 == 0 ? Side::Bid : Side::Ask;
      Price price = randPrice();
      Quantity quantity = randQuantity();
      switch (rng() % 4) {
        case 0: price = 0; break;
        case 1: price = Orderbook::MAX_PRICE + 1; break;
        case 2: quantity = 0; break;
        case 3: side = static_cast<Side>(2); break;
      }
      auto expected = ref.add(side, price, quantity);
      auto actual = ob.addOrder(side, price, quantity);
      ASSERT_EQ(actual, expected) << "invalid add side=" << static_cast<int>(side) << " price=" << price
                                  << " qty=" << quantity << " at op " << op << " seed " << seed;
    }
    else if (kind < 80) {
      // mostly live ids, sometimes stale or never-issued ones
      OrderId id = rng() % 4 == 0 ? rng() % (ref.peekNextId() + 5) : ref.randomLiveId(rng);
      ASSERT_EQ(ob.cancelOrder(id), ref.cancel(id)) << "cancel " << id << " at op " << op << " seed " << seed;
    }
    else {
      OrderId id = rng() % 4 == 0 ? rng() % (ref.peekNextId() + 5) : ref.randomLiveId(rng);
      // covers decrease, equal, increase and zero
      Quantity newQuantity = rng() % 22;
      ASSERT_EQ(ob.modifyOrder(id, newQuantity), ref.modify(id, newQuantity))
          << "modify " << id << " to " << newQuantity << " at op " << op << " seed " << seed;
    }

    compareBooks(ob, ref, op);
    if (testing::Test::HasFatalFailure()) {
      std::cerr << "fuzz failed with seed " << seed << " at op " << op << "\n";
      return;
    }
  }
}

}  // namespace

TEST(OrderbookFuzzTest, MatchesReferenceBook) {
  runFuzz(1, 1 << 16, 1 << 16, 100'000);
}

// small pool and id map so the heap fallback and id-map growth get exercised
TEST(OrderbookFuzzTest, MatchesReferenceBookWithSmallPool) {
  runFuzz(2, 8, 8, 100'000);
}

TEST(OrderbookFuzzTest, MatchesReferenceBookManySeeds) {
  for (uint64_t seed = 100; seed < 120; seed++) {
    runFuzz(seed, 32, 16, 5'000);
    if (HasFatalFailure()) return;
  }
}
