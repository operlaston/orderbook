#include <OrderbookClient.h>
#include <cassert>
#include <cstdint>

int main(int argc, char *argv[]) {
  assert(argc == 2);
  uint16_t port = static_cast<uint16_t>(std::stoul(argv[1]));
  OrderbookClient client{port};
}
