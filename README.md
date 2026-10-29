# Orderbook

## Background

An orderbook built in C++ that matches limit orders using price-time priority. Orders can be placed, canceled, and reduced in quantity.

[Update October 2026]
I have since tried to make improvements to the engine to increase new order and cancel order throughput. These optimizations can be viewed in the branch called `flat-vector`. Note that these optimizations are engine only and do not connect to the TCP server. As of now, the engine in `main` can handle ~5 million new orders/second and ~4 million order cancellations/second while the engine in `flat-vector` can handle ~40 million new orders/second and ~25 million order cancellations/second on a Lenovo Legion 5 in WSL2. I have additionally benchmarked the engine in `flat-vector` on a Macbook with an M5 Pro which yields ~65 million new orders/second and ~150 million order cancellations/second.

## Engine
The engine runs on its own dedicated thread and is responsible for matching orders and keeping track of bids and asks. It spins in a busy loop (and should be pinned to a single core while doing so) waiting for requests to be made available to it through a single producer single consumer lock-free queue. Upon popping a request off the queue, it processes the request and sends a response back through a different spsc queue.

## Server
The server is also run on its own dedicated thread. It handles all the network I/O. It uses epoll to implement an async event loop. For each client, an instance of a Session is created (which manages the underlying file descriptor and closes it upon destruction). Upon receiving a request, it forwards it to the matching engine so it can move on to parse other requests while the engine works on fulfilling the request. Once the matching engine wants to send a response, it will make the response available via the outbound spsc queue and write to an event file descriptor (created using eventfd), which is in the server's epoll interest list. This allows the server to be made aware that a response is ready to be sent back to a client.

## Client
This project also includes a very basic client command line application to interact with the server which I mostly used for testing.

## Dependencies
CMake (3.16+) and a compiler with C++20 support (e.g. gcc) are required to build this application. GoogleTest is required to build and run the tests.

## Build, Run, Test, and Benchmark
From the project's root directory:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
This produces the `orderbook` (server), `client`, `tests`, and `bench` binaries in `build/`.

* Run the server: `./build/orderbook`
* Run the client: `./build/client`
* Run tests: `./build/tests`
* Run benchmarks: `./build/bench`

To build the tests with AddressSanitizer and UndefinedBehaviorSanitizer, add `-DORDERBOOK_SANITIZE=ON` to the first `cmake` command.

# Communication Protocol
This is a basic binary protocol I came up with on top of TCP. The server listens on port `8080`.

All multi-byte integer fields are unsigned and sent in **big-endian** (network) byte order. Every message starts with a 1-byte `MessageType`, followed by the fields for that message type. There is no padding between fields.

## Requests (Incoming Messages)

### Message Structures

#### New Order Message
**Total Size: 14 bytes**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | `0` (New Order) |
| `Side` | 1 | Indicates whether it is a Buy or Sell |
| `Price` | 4 | The limit price. Must be in the range `[1, 100000]` |
| `Quantity` | 8 | The requested number of units. Must be nonzero |

Every new order is a limit order. It is matched against the opposite side of the book at the given price or better, and any quantity that is left over rests on the book until it is filled or canceled.

#### Cancel Order Message
**Total Size: 9 bytes**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | `1` (Cancel Order) |
| `OrderId` | 8 | The id of the resting order to cancel |

#### Modify Order Message
**Total Size: 17 bytes**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | `2` (Modify Order) |
| `OrderId` | 8 | The id of the resting order to modify |
| `NewQuantity`| 8 | The new quantity. Must be nonzero and no greater than the order's current quantity (orders can only be reduced) |

### Field Enumerations

**Message Types**
* `0` = New Order
* `1` = Cancel Order
* `2` = Modify Order

**Sides**
* `0` = BUY (bid)
* `1` = SELL (ask)

---

## Responses (Outgoing Messages)

Each request gets exactly one response, sent in the order the requests were received.

### Message Structures

#### New Order Response
**Total Size: 9 bytes**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `Status` | 1 | The outcome of the new order request |
| `OrderId` | 8 | The id assigned to the order if any of it is resting on the book. An id of `0` means the order was fully filled immediately (or was rejected) |

#### Cancel Order Response
**Total Size: 1 byte**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `Status` | 1 | The outcome of the cancel order request |

#### Modify Order Response
**Total Size: 1 byte**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `Status` | 1 | The outcome of the modify order request |

### Status Enumerations

* `0` = SUCCESS
* `1` = BAD_REQUEST - the request had invalid field values (e.g. price out of range, zero quantity, unknown or already-removed order id)
* `2` = SERVER_ERROR

### Malformed Messages
If the server receives an unknown `MessageType`, or a message whose body is shorter than expected, it replies with a single `BAD_REQUEST` status byte (no `OrderId`, even for a new order) and stops reading the rest of what that client has sent so far.
