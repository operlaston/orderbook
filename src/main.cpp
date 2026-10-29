#include <Orderbook.h>
#include <Server.h>
#include <ServerEngineContext.h>
#include <thread>
#include <Engine.h>

int main() {
  ServerEngineContext ctx;
  Engine engine{10'000'000, 10'000'000, ctx};
  std::jthread matchingEngine([&engine]() {
    engine.run();
  });
  Server server{ctx};
  server.run();

  matchingEngine.join();
  return 0;
}
