# Orderbook

A limit order book and matching engine in C++20, served over a small binary TCP
protocol.

Every order is a limit order that rests until it is filled or cancelled
(good-til-cancel). Incoming orders match against the opposite side by price,
then by time (oldest first within a price level), and fill at the resting
order's price.

## Engine

The engine (`src/Orderbook.cpp`) is single-threaded and keeps the hot path free
of hashing and tree lookups:

- **Flat price levels.** Each side is a `std::vector` with one `std::list` of
  orders per price, indexed directly by price (`1..MAX_PRICE`, currently
  100000).
- **Price bitsets.** Each side has a bitset with one bit per price, set while
  that level has orders. When the best level empties, the next best price is
  found by scanning 64 prices per word with `ctz`/`clz`.
- **Pool allocator.** List nodes come from a preallocated, cache-line-aligned
  block pool (`FixedPool` / `BlockAllocator`). It falls back to the heap once
  the pool is used up.
- **Flat id map.** Order ids are handed out in sequence, and only to orders that
  rest. A `std::vector` indexed by id holds each resting order's list iterator,
  so cancel and modify are O(1).

The engine runs on its own thread (`src/Engine.cpp`). It busy-polls a lock-free
single-producer single-consumer queue for requests and pushes each response to
a second SPSC queue. It then wakes the server through an `eventfd`.

## Server

The server (`src/Server.cpp`) runs on its own thread with an `epoll` event
loop. Each client gets a `Session` that owns the socket and a receive buffer,
so messages split across TCP reads, or several messages in one read, are
handled. Requests are forwarded to the engine. When the engine signals the
`eventfd`, the server sends the queued responses back to their clients.
Responses to a client always come back in the order of its requests.

If a client stops reading and its socket buffer fills, or the connection
breaks, the server disconnects it.

## Client

`client` is a small interactive command-line client, mostly for manual testing.
`include/OrderbookClient.h` can also be used as a blocking client library.

## Build, run, test, benchmark

Requires CMake 3.16+, a C++20 compiler (GCC 13 tested), and GoogleTest.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This builds four binaries in `build/`:

| Binary | What it is |
| :--- | :--- |
| `orderbook [port]` | the server and engine (default port 8080) |
| `client [port]` | interactive client |
| `tests` | unit tests, plus a randomized test against a reference book |
| `bench` | engine microbenchmark (add, modify, cancel) |

```sh
./build/orderbook        # in one terminal
./build/client           # in another
./build/tests
./build/bench            # use a Release build
```

To run the tests with AddressSanitizer and UBSan:

```sh
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DORDERBOOK_SANITIZE=ON
cmake --build build-asan -j && ./build-asan/tests
```

# Communication Protocol

A binary protocol on top of TCP. All multi-byte integers are **big-endian**,
with no padding. Each request gets exactly one response, and responses arrive
in request order.

## Requests (client → server)

The first byte of every request is its `MessageType`, which fixes the
message's length.

#### New Order — 14 bytes

| Field | Size (bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | `0` |
| `Side` | 1 | `0` = BUY, `1` = SELL |
| `Price` | 4 | unsigned, `1..100000` |
| `Quantity` | 8 | unsigned, greater than 0 |

#### Cancel Order — 9 bytes

| Field | Size (bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | `1` |
| `OrderId` | 8 | id of the resting order to cancel |

#### Modify Order — 17 bytes

| Field | Size (bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | `2` |
| `OrderId` | 8 | id of the resting order to modify |
| `NewQuantity` | 8 | new remaining quantity |

Modify can only lower an order's quantity (or leave it unchanged), and the
order keeps its place in the queue. A new quantity of 0, or one above the
order's remaining quantity, is rejected. Use cancel to remove an order.

## Responses (server → client)

#### New Order Response — 9 bytes

| Field | Size (bytes) | Description |
| :--- | :--- | :--- |
| `Status` | 1 | see below |
| `OrderId` | 8 | id of the resting order, or `0` if the order filled in full on arrival or was rejected |

The response is always 9 bytes, including when the request is rejected.

#### Cancel Order Response — 1 byte

| Field | Size (bytes) | Description |
| :--- | :--- | :--- |
| `Status` | 1 | see below |

#### Modify Order Response — 1 byte

| Field | Size (bytes) | Description |
| :--- | :--- | :--- |
| `Status` | 1 | see below |

### Statuses

* `0` = SUCCESS
* `1` = BAD_REQUEST: invalid side, price or quantity, an unknown or no longer
  resting order id, or an invalid modify
* `2` = SERVER_ERROR

### Unknown message types

If a request starts with an unknown `MessageType`, the server can't tell where
the next message begins. It sends the responses for any earlier requests, then
a single `BAD_REQUEST` byte, then closes the connection.
