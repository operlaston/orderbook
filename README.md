# Orderbook

## Background

An orderbook built in C++ to support market and limit orders. It also implements the FillOrKill, ImmediateOrCancel, and GoodTilCancel TIF order types.

## Engine
The engine runs on its own dedicated thread and is responsible for matching orders and keeping track of bids and asks. It spins in a busy loop (and should be pinned to a single core while doing so) waiting for requests to be made available to it through a single producer single consumer lock-free queue. Upon popping a request off the queue, it processes the request and sends a response back through a different spsc queue.

## Server
The server is also run on its own dedicated thread. It handles all the network I/O. It uses epoll to implement an async event loop. For each client, an instance of a Session is created (which manages the underlying file descriptor and closes it upon destruction). Upon receiving a request, it forwards it to the matching engine so it can move on to parse other requests while the engine works on fulfilling the request. Once the matching engine wants to send a response, it will make the response available via the outbound spsc queue and write to an event file descriptor (created using eventfd), which is in the server's epoll interest list. This allows the server to be made aware that a response is ready to be sent back to a client.

# Communication Protocol
This is a basic protocol I came up with on top of TCP.

## Requests (Incoming Messages)

Incoming messages communicate the fundamental attributes of a trade request, including `Side`, `OrderType`, `Price`, `Quantity`, and `TimeInForce`.

### Message Structures

#### New Order Message
**Total Size: 20 bytes**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | Identifies the message as a New Order |
| `Side` | 1 | Indicates whether it is a Buy or Sell |
| `OrderType` | 1 | Indicates whether it is a Limit or Market order |
| `TimeInForce` | 1 | Specifies how long the order remains active |
| `Price` | 8 | The requested order price |
| `Quantity` | 8 | The requested number of units |

#### Cancel Order Message
**Total Size: 9 bytes**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | Identifies the message as a Cancel Order |
| `OrderId` | 8 | The unique identifier of the order to be canceled |

#### Modify Order Message
**Total Size: 17 bytes**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `MessageType` | 1 | Identifies the message as a Modify Order |
| `OrderId` | 8 | The unique identifier of the order to be modified |
| `NewQuantity`| 8 | The updated quantity for the order |

### Field Enumerations

**Message Types**
* `0` = New Order
* `1` = Cancel Order
* `2` = Modify Order

**Sides**
* `0` = BUY
* `1` = SELL

**Order Types**
* `0` = LIMIT
* `1` = MARKET

**Time In Force**
* `0` = NONE (this option is meant to be used for market orders and defaults to GTC on limit orders)
* `1` = GOODTILCANCEL (GTC)
* `2` = IMMEDIATE OR CANCEL (IOC)
* `3` = FILL OR KILL (FOK)

---

## Responses (Outgoing Messages)

Outgoing messages represent the system's acknowledgement and result of the incoming requests.

### Message Structures

#### New Order Response
**Total Size: 9 bytes**

| Field | Size (Bytes) | Description |
| :--- | :--- | :--- |
| `Status` | 1 | The outcome of the new order request |
| `OrderId` | 8 | The generated unique identifier assigned to the new order |

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
* `1` = BAD_REQUEST
* `2` = INVALID_MESSAGE_TYPE
* `3` = SERVER_ERROR
* `4` = PARTIAL_FILL
* `5` = CANT_FILL


