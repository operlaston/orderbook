#include "Orderbook.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

int main() {
  Orderbook ob;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint64_t> priceDist(1, 10000);
  std::uniform_int_distribution<uint64_t> quantityDist(1, 10000);
  std::uniform_int_distribution<uint8_t> sideDist(0, 1);

  const size_t NUM_OPS = 10000000;

  std::vector<Price> prices(NUM_OPS);
  std::vector<Quantity> quantities(NUM_OPS);
  std::vector<Side> sides(NUM_OPS);

  for (size_t i = 0; i < NUM_OPS; i++) {
    prices[i] = priceDist(gen);
    quantities[i] = quantityDist(gen);
    sides[i] = static_cast<Side>(sideDist(gen));
  }

  auto start = std::chrono::steady_clock::now();
  for (size_t i = 0; i < NUM_OPS; i++) {
    ob.addOrder(sides[i], prices[i], quantities[i]);
  }
  auto end = std::chrono::steady_clock::now();

  auto elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  double nsPerOp = elapsed / static_cast<double>(NUM_OPS);
  double opsPerSec = (static_cast<double>(NUM_OPS) / elapsed) * 1e9;

  std::printf("new_limit_orders_randomized\n");
  std::printf("  ops:        %zu\n", NUM_OPS);
  std::printf("  total time: %.6f s\n", static_cast<double>(elapsed) / 1e9);
  std::printf("  per op:     %.2f ns\n", nsPerOp);
  std::printf("  throughput: %.0f ops/s\n", opsPerSec);

  // const size_t numRestingOrders = ob.getActiveOrders().size();
  // std::vector<OrderId> restingOrders(numRestingOrders);
  // int idx = 0;
  // for (auto restingOrder : ob.getActiveOrders()) {
  //   restingOrders[idx++] = restingOrder.first;
  // }
  //
  // start = std::chrono::steady_clock::now();
  // for (size_t i = 0; i < numRestingOrders; i++) {
  //   ob.cancelOrder(cancelRes, restingOrders[i]);
  // }
  // end = std::chrono::steady_clock::now();
  //
  // elapsed =
  //     std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  // nsPerOp = elapsed / static_cast<double>(numRestingOrders);
  // opsPerSec = (static_cast<double>(numRestingOrders) / elapsed) * 1e9;
  //
  // std::printf("cancel_orders\n");
  // std::printf("  ops:        %zu\n", numRestingOrders);
  // std::printf("  total time: %.6f s\n", static_cast<double>(elapsed) / 1e9);
  // std::printf("  per op:     %.2f ns\n", nsPerOp);
  // std::printf("  throughput: %.0f ops/s\n", opsPerSec);
}
