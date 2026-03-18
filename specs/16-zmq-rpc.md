# ZMQ RPC

## Overview

The ZMQ RPC subsystem provides a high-performance, asynchronous interface to the Monero daemon using ZeroMQ (libzmq). It exposes two independent communication patterns: a JSON-RPC 2.0 request/reply service over a `ZMQ_REP` socket, and a publish/subscribe notification service over a `ZMQ_XPUB` socket. The REP socket handles the same set of daemon RPC methods available through the HTTP JSON-RPC interface (via `DaemonHandler`), while the PUB socket pushes real-time chain and mempool events to subscribed clients. See `docs/ZMQ.md` for the user-facing protocol specification covering topic naming, subscription semantics, and message ordering guarantees.

## Key Files

| File | Description |
|---|---|
| `src/rpc/zmq_server.h` | `ZmqServer` class declaration -- owns the ZMQ context, REP/XPUB/PAIR sockets, and the serve loop thread |
| `src/rpc/zmq_server.cpp` | Socket initialization, event loop (`serve()`), and lifecycle management |
| `src/rpc/zmq_pub.h` | `listener::zmq_pub` class declaration -- subscription tracking, event serialization, relay to the server thread |
| `src/rpc/zmq_pub.cpp` | JSON serialization for all pub topics, subscription management, message relay logic |
| `src/rpc/message.h` | `Message` and `FullMessage` classes for JSON-RPC 2.0 request/response framing |
| `src/rpc/message.cpp` | JSON-RPC 2.0 parsing (insitu), serialization, and error-response helpers |
| `src/rpc/daemon_handler.h` | `DaemonHandler` -- `RpcHandler` subclass that dispatches ZMQ RPC methods to `cryptonote::core` |
| `src/rpc/daemon_rpc_version.h` | ZMQ RPC version constants (currently major=2, minor=0) |
| `src/rpc/rpc_handler.h` | Abstract `RpcHandler` base class with `handle(std::string&&)` virtual method |
| `src/rpc/message_data_structs.h` | Shared data structures used by daemon RPC messages |
| `src/net/zmq.h` | Low-level ZMQ RAII wrappers (`context`, `socket`), error handling macros, `send()`/`receive()` helpers |
| `src/net/zmq.cpp` | Implementation of ZMQ error category, `terminate`, `receive()`, and zero-copy `send()` |
| `src/rpc/fwd.h` | Forward declaration of `cryptonote::listener::zmq_pub` |
| `src/daemon/daemon.cpp` | Daemon startup code that wires `ZmqServer` and `zmq_pub` into the blockchain notification system |
| `src/daemon/command_line_args.h` | ZMQ-related command-line argument definitions |
| `src/cryptonote_config.h` | Default port constants per network |
| `src/cryptonote_basic/events.h` | `txpool_event` struct definition |
| `src/rpc/CMakeLists.txt` | Build targets: `rpc_pub`, `daemon_rpc_server` |
| `tests/unit_tests/zmq_rpc.cpp` | Unit tests covering `FullMessage` parsing and all `zmq_pub` topic combinations |
| `tests/fuzz/fuzz_rpc/fuzz_zmq.cpp` | Fuzz harness for `zmq_pub` methods |
| `tests/fuzz/fuzz_rpc/zmq_endpoints.cpp` | Fuzz target implementations for each `zmq_pub` entry point |
| `utils/python-rpc/framework/zmq.py` | Python test framework ZMQ subscriber client |

## Data Structures

### ZMQ RAII Types (`src/net/zmq.h`)

```
net::zmq::context  = std::unique_ptr<void, terminate>  // calls zmq_term on destruction
net::zmq::socket   = std::unique_ptr<void, close>      // calls zmq_close on destruction
```

The `terminate` destructor retries `zmq_term()` on `EINTR`, ensuring the context is always cleaned up. The `close` destructor is a straightforward `zmq_close()` wrapper.

### ZmqServer (`src/rpc/zmq_server.h`)

```cpp
class ZmqServer final {
    RpcHandler& handler;           // DaemonHandler (dispatches RPC methods to core)
    net::zmq::context context;     // Single ZMQ context (1 I/O thread)
    boost::thread run_thread;      // Thread running serve()
    net::zmq::socket rep_socket;   // ZMQ_REP -- JSON-RPC request/reply
    net::zmq::socket pub_socket;   // ZMQ_XPUB -- pub/sub notifications (optional)
    net::zmq::socket relay_socket; // ZMQ_PAIR -- inproc relay from zmq_pub to serve loop (optional)
    std::shared_ptr<listener::zmq_pub> shared_state; // Pub/sub state (optional)
};
```

The pub/sub sockets (`pub_socket`, `relay_socket`, `shared_state`) are only initialized when `init_pub()` is called. The server can operate in REP-only mode.

### zmq_pub (`src/rpc/zmq_pub.h`)

```cpp
class zmq_pub {
    net::zmq::socket relay_;                   // ZMQ_PAIR connected to inproc://pub_relay
    std::deque<std::vector<txpool_event>> txes_; // Queued txpool events awaiting relay
    std::array<std::size_t, 2> chain_subs_;    // Subscriber counts: [json-full-chain_main, json-minimal-chain_main]
    std::array<std::size_t, 1> miner_subs_;    // Subscriber counts: [json-full-miner_data]
    std::array<std::size_t, 2> txpool_subs_;   // Subscriber counts: [json-full-txpool_add, json-minimal-txpool_add]
    boost::mutex sync_;                        // Protects *_subs_ arrays and txes_ queue
};
```

### txpool_event (`src/cryptonote_basic/events.h`)

```cpp
struct txpool_event {
    cryptonote::transaction tx;
    crypto::hash hash;
    uint64_t blob_size;
    uint64_t weight;
    bool res;   // When false, listeners must ignore the tx (failed validation)
};
```

### Message / FullMessage (`src/rpc/message.h`)

`Message` is the base class for all ZMQ RPC request/response payloads. It carries `status`, `error_details`, and `rpc_version`. Each concrete message type (defined in `daemon_messages.h`) overrides `doToJson()` and `fromJson()`.

`FullMessage` wraps a complete JSON-RPC 2.0 envelope. It uses **insitu parsing** (`doc.ParseInsitu`) to avoid copying string data from the raw JSON into the DOM, which accelerates string-heavy content. It validates the presence of `jsonrpc`, `method`+`params` (for requests), or `result`/`error` (for responses).

### Pub/Sub Serialization Structs (anonymous namespace in `zmq_pub.cpp`)

| Struct | Purpose |
|---|---|
| `minimal_chain` | Holds `height` + `blocks` span for the compact chain_main format (`first_height`, `first_prev_id`, `ids`) |
| `miner_data` | Aggregates all fields for the miner_data notification |
| `minimal_txpool` | Holds `tx`, `hash`, `blob_size`, `weight`, `fee` for the compact txpool_add format |
| `context<F>` | Associates a topic name string with a serializer function pointer; stored in sorted arrays for binary search |

### Pub Topic Context Arrays

```cpp
chain_contexts[2]  = { "json-full-chain_main",    "json-minimal-chain_main"   }
miner_contexts[1]  = { "json-full-miner_data"                                 }
txpool_contexts[2] = { "json-full-txpool_add",     "json-minimal-txpool_add"   }
```

These arrays must be sorted lexicographically (verified at construction time by `verify_sorted()`). Prefix matching via `get_range()` allows a single subscription like `json-full` to match across multiple context arrays.

## Public API

### ZmqServer

| Method | Signature | Description |
|---|---|---|
| `init_rpc` | `void* init_rpc(boost::string_ref address, boost::string_ref port)` | Creates the ZMQ context (1 I/O thread) and binds a `ZMQ_REP` socket to `tcp://{address}:{port}`. Returns the ZMQ context pointer on success, `nullptr` on failure. Empty address defaults to `*` (all interfaces), empty port defaults to `*` (OS-assigned). |
| `init_pub` | `std::shared_ptr<listener::zmq_pub> init_pub(epee::span<const std::string> addresses)` | Initializes the pub/sub subsystem: creates a `ZMQ_XPUB` socket bound to each address, a `ZMQ_PAIR` relay socket bound to `inproc://pub_relay`, and the shared `zmq_pub` state. Returns the shared `zmq_pub` pointer (for the blockchain to hold), or `nullptr` on failure. Must be called after `init_rpc`. |
| `run` | `void run()` | Spawns the `serve()` loop in a new `boost::thread`. |
| `stop` | `void stop()` | Resets (destroys) the ZMQ context, which causes all blocking ZMQ operations to return `ETERM`. Then joins the serve thread. |
| `serve` | `void serve()` | Main event loop (private, runs on `run_thread`). |

### zmq_pub

| Method | Signature | Thread Safety | Description |
|---|---|---|---|
| `sub_request` | `bool sub_request(boost::string_ref message)` | Thread-safe (mutex) | Processes XPUB subscription/unsubscription messages. First byte: `0x01` = subscribe, `0x00` = unsubscribe. Remainder is topic prefix. Updates subscriber counts in `chain_subs_`, `miner_subs_`, `txpool_subs_`. |
| `relay_to_pub` | `bool relay_to_pub(void* relay, void* pub)` | Called from serve loop only | Reads one message from the `relay` socket. If it is a block message, forwards directly to `pub`. If it is the `tx_signal` sentinel, dequeues txpool events from `txes_`, serializes them, and sends to `pub`. |
| `send_chain_main` | `std::size_t send_chain_main(uint64_t height, span<const block> blocks)` | Thread-safe (mutex) | Called from the P2P thread when new blocks arrive. Serializes block data (on the calling thread) into the relay socket. Returns the number of messages sent. Skips serialization entirely when no subscribers exist (checked via `chain_subs_`). |
| `send_miner_data` | `std::size_t send_miner_data(...)` | Thread-safe (mutex) | Called from the P2P thread when new miner data is available. Serializes miner data into the relay socket. Returns the number of messages sent. |
| `send_txpool_add` | `std::size_t send_txpool_add(vector<txpool_event> txes)` | Thread-safe (mutex) | Called from core when transactions enter the pool. Queues events in `txes_` and sends a `tx_signal` sentinel string to the relay socket. The actual serialization happens later in `relay_to_pub` on the server thread. |

### Weak-Pointer Callables

`zmq_pub` provides three nested callable structs for safe integration with the blockchain notification system:

- `zmq_pub::chain_main` -- holds `weak_ptr<zmq_pub>`, calls `send_chain_main` if the `zmq_pub` is still alive.
- `zmq_pub::miner_data` -- holds `weak_ptr<zmq_pub>`, calls `send_miner_data` if the `zmq_pub` is still alive.
- `zmq_pub::txpool_add` -- holds `weak_ptr<zmq_pub>`, calls `send_txpool_add` if the `zmq_pub` is still alive.

These are registered via `Blockchain::add_block_notify()`, `Blockchain::add_miner_notify()`, and `Blockchain::set_txpool_notify()` respectively. The weak pointer ensures the callbacks become no-ops if the ZMQ server is destroyed before the blockchain object.

### ZMQ RPC Methods (via DaemonHandler)

The ZMQ REP socket accepts JSON-RPC 2.0 requests. The `DaemonHandler` dispatches to these methods:

| Method | Description |
|---|---|
| `GetHeight` | Current blockchain height |
| `GetBlocksFast` | Bulk block retrieval |
| `GetHashesFast` | Bulk block hash retrieval |
| `GetTransactions` | Retrieve transactions by hash |
| `KeyImagesSpent` | Check if key images are spent |
| `GetTxGlobalOutputIndices` | Output indices for a transaction |
| `SendRawTx` / `SendRawTxHex` | Submit raw transaction |
| `StartMining` / `StopMining` / `MiningStatus` | Mining control |
| `GetInfo` | Daemon status information |
| `SaveBC` | Save blockchain to disk |
| `GetBlockHash` | Block hash by height |
| `GetBlockTemplate` / `SubmitBlock` | Mining template management |
| `GetLastBlockHeader` / `GetBlockHeaderByHash` / `GetBlockHeaderByHeight` / `GetBlockHeadersByHeight` / `GetBlockHeadersRange` | Block header queries |
| `GetBlock` | Full block retrieval |
| `GetPeerList` / `GetConnections` | P2P network info |
| `SetLogHashRate` / `SetLogLevel` | Logging configuration |
| `GetTransactionPool` | Mempool contents |
| `StopDaemon` | Shutdown daemon |
| `StartSaveGraph` / `StopSaveGraph` | Difficulty graph saving |
| `HardForkInfo` | Hard fork status |
| `GetBans` / `SetBans` | IP ban management |
| `FlushTransactionPool` | Clear mempool |
| `GetOutputHistogram` / `GetOutputKeys` / `GetOutputDistribution` | Output-related queries |
| `GetRPCVersion` | Returns `DAEMON_RPC_VERSION_ZMQ` (currently 2.0, encoded as `0x00020000`) |
| `GetFeeEstimate` | Fee estimation |

## Internal Logic

### Server Event Loop (`ZmqServer::serve`)

The serve loop uses `zmq_poll` with three poll items when pub/sub is active:

```
sockets[0] = relay_socket  (ZMQ_PAIR)  -- inproc relay from zmq_pub
sockets[1] = pub_socket    (ZMQ_XPUB)  -- subscription management messages from clients
sockets[2] = rep_socket    (ZMQ_REP)   -- JSON-RPC request/reply
```

On each iteration:
1. `zmq_poll` blocks until at least one socket has activity (timeout = -1, indefinite).
2. If `relay_socket` has data (`sockets[0].revents`): calls `shared_state->relay_to_pub()` to forward pub messages from the relay to the XPUB socket.
3. If `pub_socket` has data (`sockets[1].revents`): reads subscription request from XPUB and calls `shared_state->sub_request()` to update subscriber counts.
4. If `rep_socket` has data (`sockets[2].revents`), or if pub/sub is not active: reads the JSON-RPC request, passes it to `handler.handle()`, and sends the response.

When pub/sub is **not** active, the REP socket is read in blocking mode (no `ZMQ_DONTWAIT`), and `zmq_poll` is not used -- the loop simply blocks on `zmq_recv`.

The loop exits when `zmq_term` is called (via `ZmqServer::stop()`), which causes all blocking ZMQ calls to return `ETERM`.

### Pub/Sub Message Flow

The pub/sub architecture uses an internal `ZMQ_PAIR` relay to bridge multi-threaded event producers with the single-threaded XPUB socket:

```
  P2P Thread(s)                      Server Thread
  +------------------+               +------------------+
  | Blockchain events|               | serve() loop     |
  |                  |               |                  |
  | send_chain_main()|--[serialize]->| relay_socket     |
  |   via relay_     |  (ZMQ_PAIR)   | (ZMQ_PAIR)       |
  |                  |               |   |               |
  | send_txpool_add()|--[tx_signal]->|   +--relay_to_pub |
  |   queues txes_   |               |      |            |
  |   sends signal   |               |      v            |
  +------------------+               | pub_socket       |
                                     | (ZMQ_XPUB)       |
                                     |   |               |
                                     |   v               |
                                     | Subscribers       |
                                     +------------------+
```

Key design decisions:

1. **XPUB over PUB**: XPUB is used instead of PUB so the server can detect subscriptions/unsubscriptions. This allows the server to skip serialization entirely when there are no subscribers for a given topic, which is important because block serialization is expensive.

2. **Inproc relay**: ZMQ sockets are not thread-safe, so the P2P thread cannot write directly to the XPUB socket. A `ZMQ_PAIR` inproc socket pair is used for the relay. All data stays in userspace (no kernel copies for inproc).

3. **Chain events serialized on P2P thread**: Block messages are serialized into the relay socket immediately by `send_chain_main()` because the blockchain cannot "give" ownership of block objects to the pub system (unlike txpool events). This means serialization of block data occurs on the P2P thread.

4. **Txpool events deferred**: `send_txpool_add()` only queues the events in `txes_` and sends a small `tx_signal` sentinel string through the relay. The actual JSON serialization happens in `relay_to_pub()` on the server thread. This avoids holding the txpool lock during serialization.

5. **Single relay socket for ordering**: A single ZMQ_PAIR socket is used for the relay rather than separate sockets, to guarantee that chain and txpool messages maintain their relative ordering (txpool_add events always arrive before the chain_main event that contains those transactions).

### Subscription Tracking

Subscription counts are stored in fixed-size arrays corresponding 1:1 with the topic context arrays:

- `chain_subs_[0]` = subscriber count for `json-full-chain_main`
- `chain_subs_[1]` = subscriber count for `json-minimal-chain_main`
- `miner_subs_[0]` = subscriber count for `json-full-miner_data`
- `txpool_subs_[0]` = subscriber count for `json-full-txpool_add`
- `txpool_subs_[1]` = subscriber count for `json-minimal-txpool_add`

When a client subscribes to a prefix like `json-full`, the `get_range()` function performs a binary search + upper-bound scan on the sorted context arrays to find all matching topics. The `add_subscriptions()` / `remove_subscriptions()` functions then increment/decrement the corresponding counts. The count is clamped to prevent overflow (`max - 1`).

When producing messages, `make_pubs()` iterates the subscription counts: for each topic with `subs[i] > 0`, it calls the associated serializer function to write the header and JSON payload into a shared `byte_stream`. The stream is then split into individual `byte_slice` objects (one per topic), which are sent as separate ZMQ messages.

### Message Wire Format

**REP (JSON-RPC 2.0)**:
```json
{"jsonrpc":"2.0","id":0,"method":"GetHeight","params":{...}}
```
Response:
```json
{"jsonrpc":"2.0","id":0,"result":{"rpc_version":131072,...}}
```

**PUB messages**:
```
<topic-name>:<json-payload>
```
The topic name (e.g., `json-full-chain_main`) is written as raw bytes (not JSON-encoded), followed by a colon delimiter, followed by the JSON payload. This allows ZMQ's built-in prefix-based subscription filtering to work directly on the topic portion.

### JSON Serialization Details

All JSON serialization uses RapidJSON `Writer<epee::byte_stream>`, which writes directly into a growable byte buffer without intermediate string allocations. The `toJsonValue` overloads from `serialization/json_object.h` handle the conversion of Monero types (blocks, transactions, hashes) to JSON.

**Full chain_main**: Serializes the complete block array as JSON.

**Minimal chain_main**: Serializes `{"first_height": N, "first_prev_id": "...", "ids": ["...", ...]}` -- only block IDs and the starting height/prev_id, so clients can detect chain position without the full block data.

**Full miner_data**: Serializes `{"major_version": N, "height": N, "prev_id": "...", "seed_hash": "...", "difficulty": "hex", "median_weight": N, "already_generated_coins": N, "tx_backlog": [...]}`.

**Full txpool_add**: Serializes the array of complete transaction objects, filtering out events where `res == false`.

**Minimal txpool_add**: Serializes `[{"id": "...", "blob_size": N, "weight": N, "fee": N}, ...]`, filtering out events where `res == false`.

### Socket Initialization Constants

| Constant | Value | Purpose |
|---|---|---|
| `num_zmq_threads` | 1 | Number of ZMQ I/O threads for the context |
| `max_message_size` | 10 MiB | `ZMQ_MAXMSGSIZE` -- maximum incoming message size |
| `linger_timeout` | 2 seconds | `ZMQ_LINGER` -- time to wait for pending outgoing messages on close |
| `ipv6_option` | 1 (enabled) | `ZMQ_IPV6` -- all sockets support IPv6 |
| `relay_endpoint` | `"inproc://pub_relay"` | Inproc address for the PAIR relay socket |
| `txpool_signal` | `"tx_signal"` | Sentinel string sent through relay to indicate queued txpool events |

### Zero-Copy Send

The `net::zmq::send(epee::byte_slice&&, ...)` overload implements zero-copy sending. It extracts the underlying buffer from the `byte_slice`, passes ownership to ZMQ via `zmq_msg_init_data` with a custom free callback (`epee::release_byte_slice::call`), and lets ZMQ decrement the reference count when the message is delivered. This avoids copying large serialized blocks/transactions.

### Error Handling

The `net::zmq` utilities define a custom `std::error_category` that maps ZMQ error codes to human-readable strings via `zmq_strerror()`. ZMQ-specific errors (`EFSM`, `ETERM`) are kept as-is, while standard errno values are mapped to `std::errc` equivalents. The `retry_op` template automatically retries any ZMQ operation that fails with `EINTR`.

## Dependencies

### This Module Depends On

| Dependency | Usage |
|---|---|
| **libzmq** (pkg-config) | Core ZMQ library -- sockets, contexts, message passing |
| **RapidJSON** | JSON parsing (insitu) and serialization |
| **Boost.Thread** | `boost::thread` for the serve loop, `boost::mutex`/`boost::lock_guard` for synchronization |
| **Boost.StringRef** | Lightweight string views for topic matching and address handling |
| **epee** | `byte_stream`, `byte_slice` (zero-copy buffer management), `span` |
| **cryptonote_basic** | `block`, `transaction`, `txpool_event`, `tx_block_template_backlog_entry` |
| **cryptonote_core** | `core` (via `DaemonHandler`), `Blockchain` notification callbacks |
| **serialization** | `json_object.h` -- `toJsonValue`/`fromJsonValue` overloads, `INSERT_INTO_JSON_OBJECT` macro |
| **net** | `net::zmq` RAII wrappers and helpers |
| **common** | `command_line` argument framework, `expect<T>` result type |

### What Depends on This Module

| Dependent | Usage |
|---|---|
| **`src/daemon/daemon.cpp`** | Creates `zmq_internals`, initializes `ZmqServer`, registers `zmq_pub` callbacks with `Blockchain` |
| **`cryptonote_core/blockchain.cpp`** | Invokes `zmq_pub::chain_main`, `zmq_pub::miner_data`, `zmq_pub::txpool_add` callables via stored notification callbacks |
| **`tests/unit_tests/zmq_rpc.cpp`** | Unit tests for `FullMessage` parsing and all pub/sub topic combinations |
| **`tests/fuzz/fuzz_rpc/`** | Fuzz harness targeting `zmq_pub` methods |
| **`utils/python-rpc/framework/zmq.py`** | Python test client for ZMQ SUB |

### Build Targets (from `src/rpc/CMakeLists.txt`)

| Target | Sources | Links |
|---|---|---|
| `rpc_pub` | `zmq_pub.cpp` | epee, net, cryptonote_basic, serialization, Boost.Thread |
| `daemon_rpc_server` | `daemon_handler.cpp`, `zmq_pub.cpp`, `zmq_server.cpp` | rpc, rpc_pub, cryptonote_core, cryptonote_protocol, version, daemon_messages, serialization, Boost.{Chrono,Regex,System,Thread}, libzmq |
| `daemon_messages` | `message.cpp`, `daemon_messages.cpp` | cryptonote_core, cryptonote_protocol, version, serialization |

## Configuration

### Command-Line Options

| Option | Type | Default | Description |
|---|---|---|---|
| `--zmq-rpc-bind-ip` | `string` | `127.0.0.1` | IP address for the ZMQ RPC server to listen on |
| `--zmq-rpc-bind-port` | `string` | `18082` (mainnet), `28082` (testnet), `38082` (stagenet) | Port for the ZMQ RPC server to listen on |
| `--zmq-pub` | `vector<string>` | (empty) | Address(es) for ZMQ PUB socket(s) -- format: `tcp://ip:port` or `ipc://path`. Multiple addresses can be specified. If omitted, pub/sub is disabled. |
| `--no-zmq` | `bool` | `false` | Disable the ZMQ RPC server entirely. When set, the daemon warns if any `--zmq-*` options are also specified. |

### Default Port Constants (from `src/cryptonote_config.h`)

| Network | Port |
|---|---|
| Mainnet | 18082 |
| Testnet | 28082 |
| Stagenet | 38082 |

### ZMQ RPC Version

Defined in `src/rpc/daemon_rpc_version.h`:
- Major: 2
- Minor: 0
- Combined: `0x00020000` (131072)

Returned by the `GetRPCVersion` method and included in every `Message` serialization as `rpc_version`.

## Known Issues

| File | Line | Comment |
|---|---|---|
| `src/rpc/message_data_structs.h` | 157 | `//TODO: data member?  not required, may want later.` -- the `error` struct's JSON-RPC `data` field is not implemented |
