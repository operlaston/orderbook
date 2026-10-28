#include "Orderbook.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace {
void report(const char *name, size_t ops, std::chrono::steady_clock::time_point start,
            std::chrono::steady_clock::time_point end) {
  auto elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  double nsPerOp = elapsed / static_cast<double>(ops);
  double opsPerSec = (static_cast<double>(ops) / elapsed) * 1e9;
  std::printf("%s\n", name);
  std::printf("  ops:        %zu\n", ops);
  std::printf("  total time: %.6f s\n", static_cast<double>(elapsed) / 1e9);
  std::printf("  per op:     %.2f ns\n", nsPerOp);
  std::printf("  throughput: %.0f ops/s\n", opsPerSec);
}
} // namespace

int main() {
  Orderbook ob(10'000'000, 10'000'000);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> priceDist(1, 10000);
  std::uniform_int_distribution<uint64_t> quantityDist(1, 10000);
  std::uniform_int_distribution<int> sideDist(0, 1);

  const size_t NUM_OPS = 10'000'000;

  std::vector<Price> prices(NUM_OPS);
  std::vector<Quantity> quantities(NUM_OPS);
  std::vector<Side> sides(NUM_OPS);

  for (size_t i = 0; i < NUM_OPS; i++) {
    prices[i] = priceDist(gen);
    quantities[i] = quantityDist(gen);
    sides[i] = static_cast<Side>(sideDist(gen));
  }

  // add orders
  uint64_t rested = 0;
  auto start = std::chrono::steady_clock::now();
  for (size_t i = 0; i < NUM_OPS; i++) {
    rested += ob.addOrder(sides[i], prices[i], quantities[i]).orderId != 0;
  }
  auto end = std::chrono::steady_clock::now();
  report("new_limit_orders_randomized", NUM_OPS, start, end);
  std::printf("  rested:     %lu\n", static_cast<unsigned long>(rested));

  std::vector<OrderId> restingIds;
  std::vector<Quantity> newQuantities;
  for (size_t i = 1; i < ob.getIdMap().size(); i++) {
    if (const Order *o = ob.findOrder(i)) {
      restingIds.push_back(i);
      newQuantities.push_back(o->getQuantity() > 1 ? o->getQuantity() - 1 : 1);
    }
  }
  // Visit orders in random order so the benchmark isn't just a linear walk.
  std::vector<size_t> perm(restingIds.size());
  for (size_t i = 0; i < perm.size(); i++) {
    perm[i] = i;
  }
  std::shuffle(perm.begin(), perm.end(), gen);

  // modify orders (decrease quantity by 1)
  start = std::chrono::steady_clock::now();
  for (size_t i : perm) {
    ob.modifyOrder(restingIds[i], newQuantities[i]);
  }
  end = std::chrono::steady_clock::now();
  report("modify_orders", perm.size(), start, end);

  // cancel orders (in id order, like the previous benchmark)
  start = std::chrono::steady_clock::now();
  for (OrderId id : restingIds) {
    ob.cancelOrder(id);
  }
  end = std::chrono::steady_clock::now();
  report("cancel_orders", restingIds.size(), start, end);

  return 0;
}
