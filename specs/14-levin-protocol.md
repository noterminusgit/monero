# Levin Protocol

## Overview

The Levin protocol is the binary peer-to-peer messaging layer used for all Monero network communication. It provides framing, request/response correlation, notification delivery, and traffic obfuscation (noise/fragmentation) for privacy on anonymity networks. The wire format -- header layout, flag bits, signature, message flow semantics (notifications, requests, responses, fragments, dummy messages) -- is documented in `docs/LEVIN_PROTOCOL.md`. This specification covers the **implementation-level** details: data structures, the async protocol handler state machine, connection lifecycle management, serialization integration, and the higher-level notification privacy system.

## Key Files

| File | Description |
|------|-------------|
| `contrib/epee/include/net/levin_base.h` | Core constants, `bucket_head2` header struct, flag defines, error codes, `levin_commands_handler` interface, `message_writer` class, and free functions for header/noise/fragment construction. |
| `contrib/epee/src/levin_base.cpp` | Implementation of `message_writer::finalize`, `make_header`, `make_noise_notify`, and `make_fragmented_notify`. |
| `contrib/epee/include/net/levin_protocol_handler_async.h` | The main async protocol handler (`async_protocol_handler`) and its shared configuration (`async_protocol_handler_config`). Contains the receive state machine, invoke/response correlation, and connection map management. |
| `contrib/epee/include/storages/levin_abstract_invoke2.h` | Higher-level invoke/notify helpers that serialize portable-storage structs into levin messages, plus the `CHAIN_LEVIN_INVOKE_MAP2` / `HANDLE_INVOKE_T2` / `HANDLE_NOTIFY_T2` dispatch macros. |
| `contrib/epee/include/net/abstract_tcp_server2.h` | TCP server and `connection<T>` class that owns one `async_protocol_handler` per connection, provides the `i_service_endpoint` interface, and manages socket I/O with Boost.Asio. |
| `contrib/epee/include/net/abstract_tcp_server2.inl` | Implementation of the TCP `connection<T>` template: read loop, write queue, SSL handshake, timer management, connection lifecycle states. |
| `contrib/epee/include/net/net_utils_base.h` | `connection_context_base` (UUID, remote address, traffic counters) and `i_service_endpoint` interface. |
| `contrib/epee/include/net/buffer.h` | `epee::net_utils::buffer` -- the append/carve buffer used as `m_cache_in_buffer` for incremental receive parsing. |
| `src/cryptonote_basic/connection_context.h` | `cryptonote_connection_context` -- extends `connection_context_base` with handshake state, per-command size limits, blockchain sync state. |
| `src/cryptonote_basic/connection_context.cpp` | `get_max_bytes()` -- per-command maximum payload size table. |
| `src/cryptonote_protocol/levin_notify.h` | `cryptonote::levin::notify` -- Dandelion++ and noise-channel transaction notification privacy layer. |
| `src/cryptonote_protocol/levin_notify.cpp` | Full Dandelion++ stem/fluff/epoch logic and noise-channel management. |
| `docs/LEVIN_PROTOCOL.md` | Wire protocol specification (header format, message types, command table). |

## Data Structures

### `bucket_head2` (levin_base.h)

The on-wire header, packed to 33 bytes (`#pragma pack(1)`):

```
struct bucket_head2
{
  uint64_t m_signature;            // 8 bytes: LEVIN_SIGNATURE (0x0101010101012101)
  uint64_t m_cb;                   // 8 bytes: payload length (excludes header)
  uint8_t  m_have_to_return_data;  // 1 byte:  non-zero if response expected
  uint32_t m_command;              // 4 bytes: command ID
  int32_t  m_return_code;          // 4 bytes: response status (0 for requests)
  uint32_t m_flags;                // 4 bytes: LEVIN_PACKET_REQUEST/RESPONSE/BEGIN/END
  uint32_t m_protocol_version;     // 4 bytes: LEVIN_PROTOCOL_VER_1 (1)
};
// Total: 33 bytes
```

There is also a legacy `bucket_head` struct (same file) with `m_reservedA`/`m_reservedB` instead of `m_flags`/`m_protocol_version`. It is not used in the active protocol (version 1).

All multi-byte fields are little-endian on the wire. The implementation uses `SWAP64LE`/`SWAP32LE` macros for byte-order conversion. On little-endian platforms, the header is cast directly from the receive buffer without swapping.

### Flag Constants (levin_base.h)

| Constant | Value | Meaning |
|----------|-------|---------|
| `LEVIN_PACKET_REQUEST` | `0x00000001` | Q bit -- message is a request or notification |
| `LEVIN_PACKET_RESPONSE` | `0x00000002` | S bit -- message is a response |
| `LEVIN_PACKET_BEGIN` | `0x00000004` | B bit -- first fragment of a fragmented message |
| `LEVIN_PACKET_END` | `0x00000008` | E bit -- last fragment of a fragmented message |

### Size Limits

| Constant | Value | Context |
|----------|-------|---------|
| `LEVIN_INITIAL_MAX_PACKET_SIZE` | 256 KiB (262,144) | Maximum payload before handshake completes |
| `LEVIN_DEFAULT_MAX_PACKET_SIZE` | 100,000,000 (~100 MB) | Maximum payload after handshake |

After the handshake command (command 1001) completes, `m_max_packet_size` is upgraded from `m_initial_max_packet_size` to `m_config.m_max_packet_size`. This transition happens in two places: (1) when sending a handshake invoke (`async_invoke`), and (2) when receiving a handshake response where `handshake_complete()` returns true.

Additionally, `cryptonote_connection_context::get_max_bytes()` provides per-command limits that are checked alongside `m_max_packet_size`, with the stricter of the two applied:

| Command | Max Bytes |
|---------|-----------|
| Handshake (1001) | 64 KiB |
| Timed Sync (1002) | 64 KiB |
| Ping (1003) | 4 KiB |
| Support Flags (1007) | 4 KiB |
| New Block (2001) | 128 MiB |
| New Transactions (2002) | 128 MiB |
| Request Get Objects (2003) | 2 MiB |
| Response Get Objects (2004) | 128 MiB |
| Request Chain (2006) | 512 KiB |
| Response Chain Entry (2007) | 4 MiB |
| New Fluffy Block (2008) | 4 MiB |
| Request Fluffy Missing TX (2009) | 1 MiB |
| Get Txpool Complement | 4 MiB |
| Unknown commands | `size_t` max (no per-command limit) |

### Error Codes (levin_base.h)

| Code | Value | Meaning |
|------|-------|---------|
| `LEVIN_OK` | 0 | Success |
| `LEVIN_ERROR_CONNECTION` | -1 | Generic connection error |
| `LEVIN_ERROR_CONNECTION_NOT_FOUND` | -2 | UUID lookup failed in connection map |
| `LEVIN_ERROR_CONNECTION_DESTROYED` | -3 | Connection was released/destroyed |
| `LEVIN_ERROR_CONNECTION_TIMEDOUT` | -4 | Async invoke response timed out |
| `LEVIN_ERROR_CONNECTION_NO_DUPLEX_PROTOCOL` | -5 | Not used in current code |
| `LEVIN_ERROR_CONNECTION_HANDLER_NOT_DEFINED` | -6 | No handler registered for command |
| `LEVIN_ERROR_FORMAT` | -7 | Deserialization/format error |

### `levin_commands_handler<t_connection_context>` (levin_base.h)

Abstract interface that application code implements to handle incoming messages:

```cpp
virtual int invoke(int command, const epee::span<const uint8_t> in_buff,
                   byte_stream& buff_out, t_connection_context& context) = 0;
virtual int notify(int command, const epee::span<const uint8_t> in_buff,
                   t_connection_context& context) = 0;
virtual void callback(t_connection_context& context) {}
virtual void on_connection_new(t_connection_context& context) {}
virtual void on_connection_close(t_connection_context& context) {}
```

- `invoke` is called for requests (`m_have_to_return_data` is set). The handler writes its serialized response into `buff_out` and returns a status code that becomes the response's `m_return_code`.
- `notify` is called for notifications and Dandelion++/cryptonote protocol messages.
- `callback` is triggered when a peer has requested an asynchronous callback via `request_callback`.
- `on_connection_new` / `on_connection_close` are lifecycle hooks called when a connection is added to or removed from the connection map.

### `message_writer` (levin_base.h / levin_base.cpp)

A zero-copy message builder that reserves space for the `bucket_head2` header at construction and fills it in at finalization:

```cpp
explicit message_writer(std::size_t reserve = 8192);
byte_slice finalize_invoke(uint32_t command);   // flags = REQUEST, expect_response = true
byte_slice finalize_notify(uint32_t command);   // flags = REQUEST, expect_response = false
byte_slice finalize_response(uint32_t command, uint32_t return_code); // flags = RESPONSE
byte_stream buffer;  // callers write payload into this after construction
```

The constructor writes `sizeof(bucket_head2)` zero bytes, then the caller appends the serialized payload. `finalize` computes the payload size, constructs the header with `make_header`, copies it over the reserved space, and returns the result as a `byte_slice`. Calling `finalize` more than once throws `std::runtime_error`.

### `async_protocol_handler<t_connection_context>` (levin_protocol_handler_async.h)

One instance per connection. Owns the receive-side state machine and pending response handler queue. Key members:

| Member | Type | Purpose |
|--------|------|---------|
| `m_state` | `stream_state` enum | `stream_state_head` or `stream_state_body` |
| `m_current_head` | `bucket_head2` | Header of the message currently being received |
| `m_cache_in_buffer` | `net_utils::buffer` | Accumulation buffer for incoming TCP data (initial 4 KiB) |
| `m_fragment_buffer` | `std::string` | Reassembly buffer for fragmented messages |
| `m_invoke_response_handlers` | `std::list<shared_ptr<invoke_response_handler_base>>` | FIFO queue of pending response callbacks |
| `m_max_packet_size` | `atomic<uint64_t>` | Current max packet size (starts at initial, upgrades after handshake) |
| `m_protocol_released` | `atomic<bool>` | Set during teardown to prevent new handlers |
| `m_wait_count` | `atomic<uint32_t>` | Reference count for outstanding outer calls |
| `m_close_called` | `atomic<uint32_t>` | Incremented on close to reject further receives |
| `m_oponent_protocol_ver` | `int32_t` | Peer's protocol version extracted from first received header |
| `m_connection_initialized` | `bool` | Whether `after_init_connection` has been called |

### `async_protocol_handler_config<t_connection_context>` (levin_protocol_handler_async.h)

Shared configuration across all connections. Owns the connection map and command handler pointer:

| Member | Type | Purpose |
|--------|------|---------|
| `m_connects` | `unordered_map<uuid, async_protocol_handler*>` | All active connections, keyed by UUID |
| `m_connects_lock` | `critical_section` | Mutex protecting the connection map |
| `m_pcommands_handler` | `levin_commands_handler<T>*` | Application-level command handler (set via `set_handler`) |
| `m_pcommands_handler_destroy` | function pointer | Optional destructor for the handler |
| `m_initial_max_packet_size` | `uint64_t` | Pre-handshake limit (default 256 KiB) |
| `m_max_packet_size` | `uint64_t` | Post-handshake limit (default ~100 MB) |
| `m_invoke_timeout` | `chrono::milliseconds` | Default timeout for async invocations |

### `connection_context_base` (net_utils_base.h)

Base context carried per connection:

| Field | Type | Description |
|-------|------|-------------|
| `m_connection_id` | `boost::uuids::uuid` | Unique connection identifier |
| `m_remote_address` | `network_address` | Peer's address |
| `m_is_income` | `bool` | Whether the connection was incoming |
| `m_started` | `time_t` | Connection start time |
| `m_ssl` | `bool` | Whether SSL is active |
| `m_last_recv` / `m_last_send` | `time_t` | Timestamps of last I/O |
| `m_recv_cnt` / `m_send_cnt` | `uint64_t` | Byte counters |
| `m_current_speed_down` / `m_current_speed_up` | `double` | Current throughput |
| `m_max_speed_down` / `m_max_speed_up` | `double` | Peak throughput |

### Portable Storage Deserialization Limits (levin_abstract_invoke2.h)

When deserializing levin payloads, the `default_levin_limits` constrain the portable storage parser:

| Limit | Value |
|-------|-------|
| Max objects | 8,192 |
| Max fields | 16,384 |
| Max strings | 16,384 |

## Public API

### `async_protocol_handler_config` -- Connection Manager Interface

This is the primary interface used by higher-level code (the P2P layer) to interact with levin connections:

```cpp
// Asynchronous invoke: serialize message, send to peer, register response callback
template<class callback_t>
int invoke_async(int command, message_writer in_msg, boost::uuids::uuid connection_id,
                 const callback_t &cb, std::chrono::milliseconds timeout);

// Send a pre-built message (noise, fragment, or finalized notification)
int send(epee::byte_slice message, const boost::uuids::uuid& connection_id);

// Close a connection by UUID
bool close(boost::uuids::uuid connection_id);

// Update a connection's context
bool update_connection_context(const t_connection_context& contxt);

// Request an asynchronous callback on the given connection
bool request_callback(boost::uuids::uuid connection_id);

// Iterate all connections
template<class callback_t>
bool foreach_connection(const callback_t &cb);

// Access a specific connection's context
template<class callback_t>
bool for_connection(const boost::uuids::uuid &connection_id, const callback_t &cb);

// Connection counts
size_t get_connections_count();
size_t get_out_connections_count();
size_t get_in_connections_count();

// Randomly close N connections
void del_out_connections(size_t count);
void del_in_connections(size_t count);

// Set the application-level command handler
void set_handler(levin_commands_handler<T>* handler,
                 void (*destroy)(levin_commands_handler<T>*) = NULL);
```

All operations that access a specific connection use `find_and_lock_connection`, which takes the `m_connects_lock`, looks up the UUID, and calls `start_outer_call()` on the handler to increment `m_wait_count` and add a reference on the underlying socket. This prevents the connection from being destroyed while the operation is in progress.

### `async_protocol_handler` -- Per-Connection Protocol Handler

```cpp
// Called by the TCP layer when data arrives
virtual bool handle_recv(const void* ptr, size_t cb);

// Called after the TCP connection is established
bool after_init_connection();

// Asynchronous invoke on this specific connection
template<class callback_t>
bool async_invoke(int command, message_writer in_msg, const callback_t &cb,
                  std::chrono::milliseconds timeout);

// Send a pre-built message
int send(byte_slice message);

// Close the connection
bool close();

// Release the protocol (cancel all pending response handlers)
bool release_protocol();

// Reference counting for outer calls
bool start_outer_call();
bool finish_outer_call();
```

### High-Level Serialization Helpers (levin_abstract_invoke2.h)

```cpp
// Serialize t_arg, send as invoke, deserialize response as t_result in callback
template<class t_result, class t_arg, class callback_t, class t_transport>
bool async_invoke_remote_command2(const connection_context_base &context, int command,
    const t_arg& out_struct, t_transport& transport, const callback_t &cb,
    std::chrono::milliseconds inv_timeout);

// Serialize t_arg, send as notification
template<class t_arg, class t_transport>
bool notify_remote_command2(const typename t_transport::connection_context &context,
    int command, const t_arg& out_struct, t_transport& transport);
```

### Command Dispatch Macros (levin_abstract_invoke2.h)

These macros generate the `invoke()`/`notify()` virtual method implementations by dispatching to typed handler functions:

```cpp
CHAIN_LEVIN_INVOKE_MAP2(context_type)     // implements invoke()
CHAIN_LEVIN_NOTIFY_MAP2(context_type)     // implements notify()
BEGIN_INVOKE_MAP2(owner_type)             // begins the dispatch table
HANDLE_INVOKE_T2(COMMAND, func)           // maps a command to a request handler
HANDLE_NOTIFY_T2(NOTIFY, func)            // maps a command to a notification handler
CHAIN_INVOKE_MAP_TO_OBJ_FORCE_CONTEXT(obj, context_type)  // chains to another handler
END_INVOKE_MAP2()                         // ends the dispatch table (logs unknown commands)
```

The `buff_to_t_adapter` templates handle portable-storage deserialization of the input and serialization of the output, bridging between raw byte spans and typed request/response structs.

## Internal Logic

### Connection Lifecycle

1. **TCP Accept / Connect**: `boosted_tcp_server` accepts or initiates a TCP connection, creating a `connection<T>` object. The constructor instantiates `async_protocol_handler(this, *shared_state, m_conn_context)` as `m_handler`.

2. **Initialization**: `connection::start_internal` sets the connection status to `RUNNING`, starts a timer, and calls `m_handler.after_init_connection()`. This registers the handler in the config's `m_connects` map (keyed by UUID) and calls `m_pcommands_handler->on_connection_new()`.

3. **Read Loop**: The TCP layer starts asynchronous reads into a fixed 8 KiB buffer (`m_state.data.read.buffer`). When data arrives, `finish_read` posts `m_handler.handle_recv()` to a separate strand to avoid deadlocking the socket I/O strand. If `handle_recv` returns false, the connection is interrupted.

4. **Handshake Upgrade**: Before the handshake completes, `m_max_packet_size` is `LEVIN_INITIAL_MAX_PACKET_SIZE` (256 KiB). When the handshake command (1001) is processed -- either sent or received with a successful response -- `m_max_packet_size` is upgraded to `m_config.m_max_packet_size` (default ~100 MB).

5. **Teardown**: `connection::cancel_handler` calls `m_handler.release_protocol()`, which atomically swaps out all pending invoke response handlers and cancels their timers (invoking their callbacks with `LEVIN_ERROR_CONNECTION_DESTROYED`). The handler's destructor removes it from the config's connection map and calls `on_connection_close`. The destructor waits up to 60 seconds (in 100ms increments) for `m_wait_count` to reach zero.

### Receive State Machine (`handle_recv`)

The `handle_recv` method implements a two-phase state machine that processes data incrementally as TCP packets arrive:

**Phase 1: `stream_state_head`**
- Accumulates bytes in `m_cache_in_buffer` until at least `sizeof(bucket_head2)` (33) bytes are available.
- Early signature check: if at least 8 bytes are buffered, the first 8 bytes are checked against `LEVIN_SIGNATURE`. A mismatch closes the connection immediately.
- On little-endian platforms, the header is read directly via pointer cast. On big-endian platforms, each field is byte-swapped.
- Validates that `m_current_head.m_cb` does not exceed `min(m_max_packet_size, get_max_bytes(command))`.
- Stores the peer's protocol version in `m_oponent_protocol_ver`.
- Transitions to `stream_state_body`.

**Phase 2: `stream_state_body`**
- Waits until `m_cache_in_buffer.size() >= m_current_head.m_cb`.
- While waiting for a partial body, if at least `MIN_BYTES_WANTED` (512) bytes arrived and there are pending invoke response handlers, the front handler's timer is reset to avoid premature timeout during large transfers.
- Once the full body is available, it is carved from the buffer and processed based on the header flags:

  **Noise/Fragment handling** (neither REQUEST nor RESPONSE flag set):
  - If both BEGIN and END are set: dummy/noise message -- silently discarded.
  - If BEGIN is set: clear `m_fragment_buffer` and start accumulating.
  - If neither BEGIN nor END: append to `m_fragment_buffer` (middle fragment).
  - If END is set: complete the fragment. The reassembled buffer must contain a full `bucket_head2` header followed by the actual message payload. This inner header is parsed and validated (including size check against both max_packet_size and per-command max_bytes), and processing continues as a regular message.

  **Response handling** (RESPONSE flag set, protocol version 1):
  - Pops the front handler from `m_invoke_response_handlers` (FIFO order).
  - Cancels the handler's timeout timer.
  - Calls the handler's callback with the return code and payload.
  - If no response handler is available, returns false (closes connection).

  **Request/Notification handling**:
  - If `m_have_to_return_data` is set: calls `m_config.m_pcommands_handler->invoke()`, builds a response message, and sends it. After the handshake command, upgrades `m_max_packet_size`.
  - Otherwise: calls `m_config.m_pcommands_handler->notify()`.

- After processing, if the fragment buffer temp string is small enough (64 KiB or less), it is recycled to avoid reallocation.
- Transitions back to `stream_state_head` and loops to check for more buffered data.

**Overflow protection**: Before appending new data, `handle_recv` checks that `cb` (incoming bytes) does not overflow `m_max_packet_size - m_cache_in_buffer.size() - m_fragment_buffer.size()`, using subtraction rather than addition to avoid integer overflow on the variable `m_max_packet_size`.

### Async Invoke and Response Correlation

The `async_invoke` method:
1. Acquires `m_invoke_response_handlers_lock`.
2. If the command is the handshake command, upgrades `m_max_packet_size` immediately (before the response arrives, so the response can be large).
3. Calls `send_message` with the finalized invoke message.
4. Creates an `anvoke_handler<callback_t>` (note: the typo "anvoke" is in the original code) and pushes it onto `m_invoke_response_handlers`.
5. The handler starts a Boost.Asio steady timer with the configured timeout. On timeout, the callback is invoked with `LEVIN_ERROR_CONNECTION_TIMEDOUT` and the connection is closed.

Response handlers are matched in FIFO order -- responses must arrive in the same order as the corresponding requests, as specified by the protocol.

### Fragment Handling

Fragment construction (`make_fragmented_notify` in levin_base.cpp):

1. If the message fits in a single noise-sized packet, it is padded with zeros and sent as a normal notification.
2. Otherwise, the message is first finalized as a complete notification (with its own levin header).
3. The resulting bytes are split into chunks of `noise_size - sizeof(bucket_head2)`.
4. The first chunk gets a fragment header with `LEVIN_PACKET_BEGIN`, command 0, payload_space as the length.
5. Middle chunks get headers with flags = 0.
6. The last chunk gets `LEVIN_PACKET_END` and is padded with zeros to exactly `noise_size`.
7. All chunks are concatenated into a single `byte_slice` for atomic send.

The key invariant is that every fragment message (including its header) is exactly `noise_size` bytes, making fragments indistinguishable from noise messages by size.

Fragment reassembly (in `handle_recv`):
1. On BEGIN: clear the fragment buffer.
2. On each fragment (including BEGIN): append the body to `m_fragment_buffer`.
3. On END: verify the reassembled buffer is at least `sizeof(bucket_head2)`, extract the inner header, validate the inner message's size, and dispatch normally.

### Noise / Dummy Messages

`make_noise_notify` creates a dummy message with both `LEVIN_PACKET_BEGIN` and `LEVIN_PACKET_END` set, command 0, and zero-filled payload. The receiver detects both B and E bits set without Q or S bits and discards the message contents.

Noise is integrated at the `cryptonote::levin::notify` layer, which manages dedicated noise channels (up to `CRYPTONOTE_NOISE_CHANNELS` = 2 outgoing connections per zone). Noise bytes are 3 KiB per message. Noise is sent at random intervals (min 10s + 0-5s range). Transaction data is piggybacked into noise slots or fragmented across multiple noise-sized messages.

### Traffic Logging

The `on_levin_traffic` template function logs all levin traffic with the category `net.p2p.traffic`, recording direction (sent/received), error status, byte count, and either the command number or a category string like "invalid-command".

### Thread Safety

- The connection map (`m_connects`) is protected by `m_connects_lock` (a `critical_section`, which wraps `boost::recursive_mutex`).
- The response handler list (`m_invoke_response_handlers`) is protected by `m_invoke_response_handlers_lock`.
- The `start_outer_call` / `finish_outer_call` pair provides reference-counted liveness: `start_outer_call` increments `m_wait_count` and calls `add_ref()` on the service endpoint, preventing the TCP connection from being destroyed during an operation. The destructor of `async_protocol_handler` waits up to 60 seconds for `m_wait_count` to drain.
- A critical design rule is documented in comments: callbacks must never be invoked inside critical sections to avoid deadlocks. The `release_protocol` method swaps the handler list under the lock, then cancels handlers outside the lock.
- The TCP layer posts `handle_recv` to a separate strand (`connection_basic::strand_`) from the socket I/O strand (`m_strand`), allowing write processing to continue while `handle_recv` runs. This avoids deadlock where `handle_recv` queues writes that block waiting for the I/O strand which is blocked waiting for `handle_recv`.

## Dependencies

### This Module Depends On

| Dependency | Purpose |
|-----------|---------|
| `boost::asio` | Asynchronous I/O, steady timers, strands, SSL |
| `boost::uuid` | Connection identification |
| `epee::byte_slice` / `epee::byte_stream` | Zero-copy message construction and I/O buffers |
| `epee::span` | Lightweight buffer views for payload data |
| `epee::net_utils::buffer` | Incremental receive buffer with append/carve semantics |
| `epee::serialization::portable_storage` | Binary serialization of command request/response structs |
| `epee::net_utils::connection_context_base` | Per-connection metadata |
| `epee::net_utils::i_service_endpoint` | Abstract interface to the TCP connection for send/close/refcount |
| `epee::critical_section` / `syncobj.h` | Mutex wrappers (`boost::recursive_mutex`) |
| `int-util.h` | `SWAP32LE` / `SWAP64LE` byte-order macros |

### Depends On This Module

| Dependent | How |
|-----------|-----|
| `src/p2p/net_node.h` / `net_node.inl` | Instantiates `boosted_tcp_server` with the levin protocol handler, configures noise, manages peer connections. |
| `src/cryptonote_protocol/cryptonote_protocol_handler.h` | Implements `levin_commands_handler` via the `CHAIN_LEVIN_INVOKE_MAP2` macros to dispatch cryptonote commands. |
| `src/cryptonote_protocol/levin_notify.h` / `.cpp` | Uses `async_protocol_handler_config` as `connections` type for sending noise, fragments, and Dandelion++ transactions. |
| `src/rpc/core_rpc_server.h` | Uses the same TCP server infrastructure (different connection type) for RPC. |
| `src/net/dandelionpp.cpp` | Dandelion++ stem selection, depends on noise channel count configuration. |
| `tests/unit_tests/levin.cpp` | Unit tests for levin protocol handler. |
| `tests/unit_tests/levin_notify.cpp` | Unit tests for the Dandelion++/noise notification layer. |
| `tests/fuzz/levin.cpp` | Fuzz tests for the levin receive state machine. |

## Known Issues

| Location | Comment |
|----------|---------|
| `contrib/epee/include/net/levin_protocol_handler_async.h:700` | `// TODO or better just keep removing random elements (performance)` -- in `delete_connections`, connections are shuffled with `std::default_random_engine` seeded by wall-clock time, then the first N are closed. The TODO suggests a more efficient random-removal approach. |
| `contrib/epee/include/storages/levin_abstract_invoke2.h:64` | `//TODO: add true const support to searilzation` -- `const_cast<t_arg&>(out_struct).store(stg)` is used because the serialization `store()` method lacks a `const` overload. |
| `contrib/epee/include/net/abstract_tcp_server2.h:533` | `//TODO: change to enum server_type, now used` -- `m_thread_name_prefix` is a string but the comment suggests it should be an enum. |
| `contrib/epee/include/net/abstract_tcp_server2.inl:42-43` | `#include <boost/date_time/posix_time/posix_time.hpp> // TODO` and `#include <boost/thread/condition_variable.hpp> // TODO` -- apparently flagged for removal or replacement. |

Additionally, the `anvoke_handler` template class (levin_protocol_handler_async.h:191) contains a persistent typo ("anvoke" instead of "invoke") that appears throughout the handler registration code.
