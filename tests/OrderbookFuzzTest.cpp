#include <gtest/gtest.h>

#include <Orderbook.h>
#include <deque>
#include <functional>
#include <map>
#include <random>
#include <unordered_map>

namespace {

// Deliberately simple reference book: std::map of FIFO queues per side.
// Slow but easy to check by eye. Follows the same rules as Orderbook.
class ReferenceBook {
public:
  struct RefOrder {
    OrderId id;
    Quantity quantity;
  };

  std::map<Price, std::deque<RefOrder>, std::greater<>> bids;
  std::map<Price, std::deque<RefOrder>, std::less<>> asks;

  NewOrderResult add(Side side, Price price, Quantity quantity) {
    if (price == 0 || price > Orderbook::MAX_PRICE || quantity == 0 ||
        (side != Side::Bid && side != Side::Ask)) {
      return {Status::BadRequest, 0};
    }
    quantity = side == Side::Bid
                   ? match(asks, quantity,
                           [&](Price p) { return p <= price; })
                   : match(bids, quantity,
                           [&](Price p) { return p >= price; });
    if (quantity == 0) {
      return {Status::Success, 0};
    }
    OrderId id = m_nextId++;
    if (side == Side::Bid) {
      bids[price].push_back({id, quantity});
    } else {
      asks[price].push_back({id, quantity});
    }
    m_where[id] = {side, price};
    return {Status::Success, id};
  }

  Status cancel(OrderId id) {
    auto *queue = find(id);
    if (!queue) {
      return Status::BadRequest;
    }
    auto [side, price] = m_where[id];
    for (auto it = queue->begin(); it != queue->end(); ++it) {
      if (it->id == id) {
        queue->erase(it);
        break;
      }
    }
    if (queue->empty()) {
      if (side == Side::Bid) {
        bids.erase(price);
      } else {
        asks.erase(price);
      }
    }
    m_where.erase(id);
    return Status::Success;
  }

  Status modify(OrderId id, Quantity newQuantity) {
    auto *queue = find(id);
    if (!queue) {
      return Status::BadRequest;
    }
    for (RefOrder &o : *queue) {
      if (o.id == id) {
        if (newQuantity == 0 || newQuantity > o.quantity) {
          return Status::BadRequest;
        }
        o.quantity = newQuantity;
        return Status::Success;
      }
    }
    return Status::BadRequest;
  }

  OrderId nextId() const { return m_nextId; }

private:
  OrderId m_nextId = 1;
  std::unordered_map<OrderId, std::pair<Side, Price>> m_where;

  std::deque<RefOrder> *find(OrderId id) {
    auto it = m_where.find(id);
    if (it == m_where.end()) {
      return nullptr;
    }
    auto [side, price] = it->second;
    return side == Side::Bid ? &bids[price] : &asks[price];
  }

  template <typename Map, typename Crosses>
  Quantity match(Map &book, Quantity quantity, Crosses crosses) {
    while (quantity > 0 && !book.empty() && crosses(book.begin()->first)) {
      auto &queue = book.begin()->second;
      RefOrder &front = queue.front();
      Quantity filled = std::min(quantity, front.quantity);
      quantity -= filled;
      front.quantity -= filled;
      if (front.quantity == 0) {
        m_where.erase(front.id);
        queue.pop_front();
        if (queue.empty()) {
          book.erase(book.begin());
        }
      }
    }
    return quantity;
  }
};

// Checks every level of one side, order by order, plus the id map.
template <typename Map>
void expectSideMatches(const Orderbook &ob, const std::vector<Level> &levels,
                       const PriceBitset &bits, const Map &ref,
                       Price maxPrice) {
  for (Price p = 0; p <= maxPrice; p++) {
    auto it = ref.find(p);
    const Level &level = levels[p];
    ASSERT_EQ(bits.get(p), !level.empty()) << "bitset out of sync at " << p;
    if (it == ref.end()) {
      ASSERT_TRUE(level.empty()) << "price " << p;
      continue;
    }
    ASSERT_EQ(level.size(), it->second.size()) << "price " << p;
    auto refIt = it->second.begin();
    for (const Order &o : level) {
      ASSERT_EQ(o.getId(), refIt->id) << "price " << p;
      ASSERT_EQ(o.getQuantity(), refIt->quantity) << "id " << o.getId();
      ASSERT_EQ(ob.findOrder(o.getId()), &o);
      ++refIt;
    }
  }
}

} // namespace

TEST(OrderbookFuzzTest, MatchesReferenceBook) {
  // Small price range so orders cross often. 1..200 spans several bitset
  // words. Small pool/id map so the heap fallback and id-map growth are hit.
  constexpr Price PRICE_RANGE = 200;
  constexpr int NUM_OPS = 100'000;
  constexpr int FULL_CHECK_EVERY = 50;

  Orderbook ob{256, 16};
  ReferenceBook ref;
  std::mt19937_64 gen(12345);
  auto rand = [&](uint64_t lo, uint64_t hi) {
    return std::uniform_int_distribution<uint64_t>(lo, hi)(gen);
  };

  for (int i = 0; i < NUM_OPS; i++) {
    uint64_t op = rand(0, 99);
    if (op < 50) {
      // Mostly valid adds, plus a few invalid prices/quantities/sides.
      Side side = static_cast<Side>(rand(0, 1));
      Price price = static_cast<Price>(rand(1, PRICE_RANGE));
      Quantity quantity = rand(1, 50);
      uint64_t corrupt = rand(0, 99);
      if (corrupt == 0) {
        price = 0;
      } else if (corrupt == 1) {
        price = Orderbook::MAX_PRICE + 1;
      } else if (corrupt == 2) {
        quantity = 0;
      } else if (corrupt == 3) {
        side = static_cast<Side>(2);
      }
      NewOrderResult got = ob.addOrder(side, price, quantity);
      NewOrderResult want = ref.add(side, price, quantity);
      ASSERT_EQ(got.status, want.status) << "op " << i;
      ASSERT_EQ(got.orderId, want.orderId) << "op " << i;
    } else if (op < 75) {
      // Ids from 0 to a bit past the last one, so some are invalid.
      OrderId id = rand(0, ref.nextId() + 2);
      ASSERT_EQ(ob.cancelOrder(id), ref.cancel(id)) << "op " << i;
    } else {
      OrderId id = rand(0, ref.nextId() + 2);
      Quantity newQuantity = rand(0, 55);
      ASSERT_EQ(ob.modifyOrder(id, newQuantity), ref.modify(id, newQuantity))
          << "op " << i;
    }

    std::optional<Price> wantBid =
        ref.bids.empty() ? std::nullopt
                         : std::optional<Price>(ref.bids.begin()->first);
    std::optional<Price> wantAsk =
        ref.asks.empty() ? std::nullopt
                         : std::optional<Price>(ref.asks.begin()->first);
    ASSERT_EQ(ob.bestBid(), wantBid) << "op " << i;
    ASSERT_EQ(ob.bestAsk(), wantAsk) << "op " << i;
    if (wantBid && wantAsk) {
      ASSERT_LT(*wantBid, *wantAsk) << "book crossed at op " << i;
    }

    if (i % FULL_CHECK_EVERY == 0 || i == NUM_OPS - 1) {
      expectSideMatches(ob, ob.getBids(), ob.getBidsBitset(), ref.bids,
                        PRICE_RANGE);
      expectSideMatches(ob, ob.getAsks(), ob.getAsksBitset(), ref.asks,
                        PRICE_RANGE);
      ASSERT_FALSE(HasFatalFailure()) << "op " << i;
    }
  }
}
