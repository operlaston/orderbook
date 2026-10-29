#include <Orderbook.h>
#include <Server.h>
#include <ServerEngineContext.h>
#include <stop_token>
#include <thread>
#include <Engine.h>

int main() {
  ServerEngineContext ctx;
  Engine engine{10'000'000, 10'000'000, ctx};
  std::jthread matchingEngine([&engine](std::stop_token st){ engine.run(st); });

  Server server{ctx};
  server.run();

  return 0;
}
