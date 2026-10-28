#include <Engine.h>
#include <Orderbook.h>
#include <Server.h>
#include <ServerEngineContext.h>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <thread>

namespace {
constexpr uint64_t POOL_BLOCKS = 1'000'000;      // preallocated order nodes
constexpr uint32_t INITIAL_ORDER_IDS = 1'000'000; // initial id-map size
constexpr uint16_t DEFAULT_PORT = 8080;
} // namespace

int main(int argc, char **argv) {
  uint16_t port =
      argc > 1 ? static_cast<uint16_t>(std::atoi(argv[1])) : DEFAULT_PORT;

  // The queues are several MB, too big for the stack.
  auto ctx = std::make_unique<ServerEngineContext>();

  // The engine gets its own thread and busy-polls the request queue. If the
  // server fails, the jthread destructor stops and joins it.
  std::jthread engine([&ctx](std::stop_token stop) {
    Orderbook orderbook{POOL_BLOCKS, INITIAL_ORDER_IDS};
    runEngine(orderbook, *ctx, stop);
  });

  try {
    Server server{*ctx, port};
    server.run();
  } catch (const std::exception &e) {
    std::cerr << "server error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
