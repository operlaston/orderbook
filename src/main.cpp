#include <OrderType.h>
#include <Orderbook.h>
#include <Server.h>
#include <ServerEngineContext.h>
#include <TimeInForce.h>
#include <functional>
#include <thread>

void runEngine(ServerEngineContext &ctx) {
  Orderbook orderbook{ctx};
  orderbook.run();
}

void runServer(ServerEngineContext &ctx) {
  Server server{ctx};
  server.run();
}

int main() {
  ServerEngineContext ctx;
  std::thread matchingEngine(runEngine, std::ref(ctx));
  std::thread server(runServer, std::ref(ctx));

  matchingEngine.join();
  server.join();
  return 0;
}
