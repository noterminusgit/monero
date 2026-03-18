# Network Anonymity

## Overview

Monero integrates Tor and I2P anonymity networks primarily to protect the source of transaction broadcasts. The design separates network traffic into "zones" (public, Tor, I2P), routes transactions through SOCKS proxies to reach hidden service peers, and uses Dandelion++ to obscure transaction origin on the public network. Noise channels provide additional traffic analysis resistance on anonymity networks by sending fixed-size packets at regular intervals. For network-level usage documentation, configuration examples, and privacy limitation analysis, see `docs/ANONYMITY_NETWORKS.md`.

## Key Files

| File | Description |
|------|-------------|
| `src/net/tor_address.h` / `.cpp` | Tor v3 onion address parsing, validation, and serialization |
| `src/net/i2p_address.h` / `.cpp` | I2P b32 address parsing, validation, and serialization |
| `src/net/socks.h` / `.cpp` | SOCKS4/4a/5 proxy client implementation (async, boost::asio) |
| `src/net/socks_connect.h` / `.cpp` | High-level SOCKS connector for `epee::net_utils::http_client` |
| `src/net/dandelionpp.h` / `.cpp` | Dandelion++ stem connection mapping (source-to-stem assignment) |
| `src/net/error.h` / `.cpp` | Network error codes including `invalid_tor_address`, `invalid_i2p_address` |
| `src/net/fwd.h` | Forward declarations for `tor_address`, `i2p_address`, `socks::*` |
| `src/net/parse.h` | Defines `net::socks::endpoint`, `net::user_and_pass` |
| `src/cryptonote_protocol/levin_notify.h` / `.cpp` | Dandelion++ epoch management, noise channels, tx relay orchestration |
| `src/cryptonote_protocol/enums.h` | `relay_method` enum: `none`, `local`, `forward`, `stem`, `fluff`, `block` |
| `src/cryptonote_core/tx_pool.cpp` | Dandelion++ embargo timer logic, stem/fluff state transitions in mempool |
| `src/p2p/net_node.h` | `proxy` and `anonymous_inbound` config structs; command-line arg declarations |
| `src/p2p/net_node.cpp` | Parsing of `--tx-proxy` and `--anonymous-inbound` options; `is_filtered_command()` |
| `src/p2p/net_node.inl` | P2P connection handling with zone-aware peer management |
| `contrib/epee/include/net/enums.h` | `epee::net_utils::zone` enum (`public_`, `i2p`, `tor`); `address_type` enum |
| `src/cryptonote_config.h` | Dandelion++ and noise constants (epoch timing, stem count, fluff probability) |
| `src/blockchain_db/blockchain_db.h` | `txpool_tx_meta_t` with `dandelionpp_stem` bitfield |
| `docs/ANONYMITY_NETWORKS.md` | User-facing documentation for Tor/I2P setup and privacy considerations |

## Data Structures

### Zone Classification (`contrib/epee/include/net/enums.h`)

Every peer connection belongs to a zone:

```
epee::net_utils::zone: invalid(0), public_(1), i2p(2), tor(3)
```

Address types map to zones: `ipv4(1)`, `ipv6(2)`, `i2p(3)`, `tor(4)`.

The zone value also determines priority for origin transaction relay -- I2P and Tor are preferred over public when an anonymity network is configured.

### `net::tor_address` (`src/net/tor_address.h`)

Represents a Tor v3 onion address (56 base32 characters + `.onion` suffix).

- **Internal buffer**: `char host_[63]` (null-terminated) -- fits exactly `v3_length(56) + sizeof(".onion")(7) = 63`.
- **Port**: `std::uint16_t port_` -- stored alongside the host; `0` means unspecified.
- **Factory**: `static expect<tor_address> make(boost::string_ref address, std::uint16_t default_port = 0)` -- parses `"<base32>.onion[:port]"` format, validates base32 alphabet and length.
- **Unknown sentinel**: Default-constructed addresses store `"<unknown tor host>"` and are detected by checking `host_[0] == '<'`.
- **Serialization**: Uses epee KV serialization with `host` (string) and `port` (uint16) fields.
- **Comparison**: `equal()` checks both host and port; `is_same_host()` checks host only.
- **Zone**: `get_zone()` returns `epee::net_utils::zone::tor`; `get_type_id()` returns `address_type::tor`.
- **Properties**: `is_loopback()` and `is_local()` always return `false`. `is_blockable()` returns `true` for non-unknown addresses.

### `net::i2p_address` (`src/net/i2p_address.h`)

Represents an I2P b32 address (52 base32 characters + `.b32.i2p` suffix).

- **Internal buffer**: `char host_[61]` (null-terminated) -- fits exactly `b32_length(52) + sizeof(".b32.i2p")(9) = 61`.
- **No port field**: Unlike `tor_address`, there is no `port_` member. The `port()` method always returns `1` to satisfy I2P SOCKS requirements (which treat `0` as an error). The serialized form includes `port: 1` for backwards compatibility with older clients.
- **Factory**: `static expect<i2p_address> make(boost::string_ref address)` -- no `default_port` parameter since port is fixed.
- **Unknown sentinel**: Same pattern as `tor_address` using `"<unknown i2p host>"`.
- **Equality**: `equal()` delegates to `is_same_host()` (string comparison only, since port is constant).
- **Zone**: `get_zone()` returns `epee::net_utils::zone::i2p`; `get_type_id()` returns `address_type::i2p`.

### `relay_method` (`src/cryptonote_protocol/enums.h`)

Tracks how a transaction was received and how it should be relayed:

| Value | Meaning |
|-------|---------|
| `none` | Received via RPC with `do_not_relay` set |
| `local` | Received via RPC; attempting to send over i2p/tor |
| `forward` | Received over i2p/tor; timer-delayed before public broadcast |
| `stem` | Received/sent using Dandelion++ stem phase |
| `fluff` | Received/sent using Dandelion++ fluff phase |
| `block` | Received in a block; takes precedence over all others |

These methods have a precedence order enforced by `txpool_tx_meta_t::upgrade_relay_method()` -- once a tx reaches `fluff` or `block`, it cannot be downgraded.

### `txpool_tx_meta_t` (`src/blockchain_db/blockchain_db.h`)

The 192-byte metadata structure stored per pooled transaction includes Dandelion++ state via bitfields:

- `dandelionpp_stem` (1 bit): Transaction is in Dandelion++ stem phase.
- `is_local` (1 bit): Transaction originated from local RPC.
- `is_forwarding` (1 bit): Transaction received from anonymity network, pending public relay.
- `do_not_relay` (1 bit): Transaction should not be relayed.
- `last_relayed_time`: For stem transactions, this stores the embargo expiry time (future timestamp). For fluff transactions, this stores the actual last relay time.

### Proxy Configuration (`src/p2p/net_node.h`)

```cpp
struct proxy {
    std::int64_t max_connections;  // -1 = default
    net::socks::endpoint address;  // SOCKS proxy IP:port + auth + version
    epee::net_utils::zone zone;    // tor or i2p
    bool noise;                    // true = enable noise channels (default)
};

struct anonymous_inbound {
    std::int64_t max_connections;
    std::string local_ip;                        // local bind IP
    std::string local_port;                      // local bind port
    epee::net_utils::network_address our_address; // published hidden service address
    epee::net_utils::network_address default_remote; // default remote for zone
};
```

### SOCKS Endpoint (`src/net/parse.h`)

```cpp
struct endpoint {
    boost::asio::ip::tcp::endpoint address; // proxy IP:port
    user_and_pass userinfo;                 // SOCKS5 authentication credentials
    version ver;                            // SOCKS protocol version
};
```

## Tor Support

### Address Handling

Tor v3 addresses are validated in `tor_address::make()`:
1. The host must end with `.onion`.
2. After stripping the TLD, the remaining string must be exactly 56 characters of base32 (`A-Za-z2-7`).
3. An optional `:<port>` suffix is parsed separately.

Note: v3 address checksum validation (base32 decoding to verify the embedded checksum) is not yet implemented (see Known Issues).

### SOCKS Proxy Integration

Tor connections use the SOCKS proxy client (`src/net/socks.h`):

1. **Versions supported**: `v4`, `v4a`, `v4a_tor` (Tor extensions), and `v5`. Tor addresses use `v4a` or `v5` since they require domain-based resolution by the proxy.
2. **Connection flow**:
   - `socks::make_connect_client()` creates a `connect_client<Handler>` wrapping a TCP socket.
   - `set_connect_command(const net::tor_address&, ...)` delegates to `set_connect_command(host_str, port, ...)`, sending the `.onion` domain through the SOCKS protocol for the proxy to resolve.
   - `client::connect_and_send()` asynchronously connects to the proxy endpoint, then writes the SOCKS command, reads the response, and invokes the `done` callback.
3. **SOCKS5 authentication**: When `user_and_pass` credentials are provided, the client advertises both `noauth` and `userpass` methods. If the server selects `userpass`, the client sends a v1 username/password sub-negotiation.
4. **Resolve command**: `set_resolve_command()` uses the Tor-specific SOCKS4a extension command `0xF0` for DNS resolution through Tor.
5. **High-level connector**: `socks::connector` (`src/net/socks_connect.h`) provides a simpler interface returning a `boost::unique_future<tcp::socket>`. It auto-detects IPv4, IPv6, and domain-based addresses, applying the appropriate SOCKS connect method.

### Connection Flow

```
Application -> socks::connector::operator()
  -> Creates socks::connect_client with TCP socket
  -> Detects address type (IPv4/IPv6/domain)
  -> Sets appropriate connect command
  -> client::connect_and_send(proxy_endpoint)
    -> TCP connect to proxy
    -> Write SOCKS handshake (v4a/v5)
    -> [v5: auth negotiation if needed]
    -> Write connect request with .onion domain
    -> Read proxy response
    -> Invoke done callback with connected socket
  -> timeout.async_wait triggers async_close on expiry
```

## I2P Support

### Address Handling

I2P b32 addresses are validated in `i2p_address::make()`:
1. The host must end with `.b32.i2p`.
2. After stripping the TLD, the remaining string must be exactly 52 characters of base32 (`A-Za-z2-7`).
3. Port is stripped from input but not stored -- `port()` always returns `1`.

Only b32 addresses are currently supported (see Known Issues).

### Connection Flow

I2P connections reuse the same SOCKS proxy infrastructure as Tor. The key difference is:
- `socks::client::set_connect_command(const net::i2p_address&, ...)` sends the `.b32.i2p` domain and port `1` through the SOCKS protocol.
- The I2P SOCKS proxy (e.g., from i2pd) handles the actual SAM/I2P tunnel establishment transparently.
- Port is always `1` because I2P SOCKS considers port `0` an error.

## Dandelion++

Dandelion++ is a transaction relay protocol designed to make it difficult for spy nodes to determine which node originated a transaction. The implementation spans three major components: stem routing (`src/net/dandelionpp.cpp`), relay orchestration (`src/cryptonote_protocol/levin_notify.cpp`), and embargo management (`src/cryptonote_core/tx_pool.cpp`).

### Stem/Fluff Phases

Each epoch, a node probabilistically enters either **stem mode** or **fluff mode**:
- **Fluff probability**: `CRYPTONOTE_DANDELIONPP_FLUFF_PROBABILITY = 20` (20% chance of fluff epoch).
- In **stem mode**, the node forwards transactions along a fixed stem path (a subset of outgoing connections).
- In **fluff mode**, the node immediately broadcasts transactions to all eligible peers with randomized delays.

When a transaction arrives:
1. If arriving with `dandelionpp_fluff = true` in the P2P message, it is treated as fluff regardless of local epoch state.
2. If arriving without the fluff flag over an anonymity network, it enters `forward` state (delayed before public broadcast).
3. If arriving without the fluff flag over the public network, it enters `stem` state.
4. **Loop detection**: If a transaction returns in `stem` state but already has `dandelionpp_stem` set in the pool, the relay method is upgraded to `fluff` to break the loop (see `tx_pool.cpp:295-296`).

### Epoch Management (`levin_notify.cpp`)

Epochs are managed per-zone via `start_epoch`:

- **Epoch duration**: `CRYPTONOTE_DANDELIONPP_MIN_EPOCH = 10` minutes + random `[0, CRYPTONOTE_DANDELIONPP_EPOCH_RANGE = 30]` seconds.
- At each epoch boundary:
  1. A coin flip determines stem vs. fluff mode (80%/20% split).
  2. Current outgoing connections are fetched, filtered by blockchain height.
  3. A new `dandelionpp::connection_map` is constructed with `CRYPTONOTE_DANDELIONPP_STEMS = 2` randomly selected outgoing connections.
  4. The map and fluff state are swapped in atomically via the zone strand.

For noise-enabled zones (I2P/Tor with noise), the epoch timing is different:
- **Noise epoch**: `CRYPTONOTE_NOISE_MIN_EPOCH = 5` minutes + random `[0, 30]` seconds.

### Relay Selection (`dandelionpp.cpp`)

The `connection_map` class manages source-to-stem assignment:

- **`out_mapping_`**: Vector of outgoing connection UUIDs selected as stems for this epoch. Randomly shuffled from available outgoing connections; capped at `stems` count.
- **`in_mapping_`**: Sorted vector of `(source_uuid, out_index)` pairs mapping incoming sources to their assigned stem.
- **`usage_count_`**: Tracks how many sources are assigned to each stem for load balancing.

`get_stem(source)` works as follows:
1. Binary search `in_mapping_` for the source UUID.
2. If not found, call `select_stem()` which picks the stem with the lowest usage count (ties broken randomly via `crypto::rand_idx`).
3. If found but the assigned stem connection is dead (`nil_uuid`), reassign to a new stem.
4. Return the outgoing connection UUID for the stem.

`update(current)` merges new outgoing connections into the existing map, replacing dead connections while preserving live ones.

### Fluff Relay with Randomized Delays (`levin_notify.cpp`)

When fluffing (either in a fluff epoch or after stem failure), transactions are distributed via `fluff_notify`:

1. Transactions are queued per-connection in `detail::zone::contexts`.
2. Each connection gets a randomized Poisson-distributed flush delay:
   - **Incoming connections**: Average `CRYPTONOTE_DANDELIONPP_FLUSH_AVERAGE = 5` seconds.
   - **Outgoing connections**: Average 2.5 seconds (half of incoming, following Bitcoin Core's rationale that the user controls outgoing connections).
   - The step size is 250ms (`fluff_stepsize = 1/4 second`) for sufficient granularity.
3. A single system timer per zone tracks the earliest flush time.
4. On flush, transactions are sorted (to avoid leaking receive order), deduplicated, and sent.
5. For anonymity zones (I2P/Tor), fluffing only targets outbound connections to prevent sybil attacks via incoming connections.

### Timer-Based Fluffing / Embargo (`tx_pool.cpp`)

The mempool uses an **embargo timer** to ensure stem transactions eventually get broadcast even if the stem path is broken:

- **Average embargo duration**: `CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE = 39` seconds, drawn from a Poisson distribution.
- The 39-second value is calculated from the formula `(-k*(k-1)*hop) / (2*log(1-ep))` with `k=5` (hops), `ep=0.10` (10% probability that a prior hop fluffs first), and `hop=175ms` (estimated per-hop latency).
- When `set_relayed()` is called for a stem transaction, `last_relayed_time` is set to `now + embargo_duration()` (a future timestamp).
- When `get_relayable_transactions()` runs (every ~2 minutes), stem/forward transactions with expired `last_relayed_time` are included in the relay batch, effectively fluffing them.

### Forward Delay (Anonymity-to-Public Bridge)

Transactions received over I2P/Tor that need public broadcast enter `forward` state with a delay:

- **Average forward delay**: `CRYPTONOTE_FORWARD_DELAY_AVERAGE = 22.5` seconds (computed as `FORWARD_DELAY_BASE + FORWARD_DELAY_BASE/2` where `FORWARD_DELAY_BASE = NOISE_MIN_DELAY + NOISE_DELAY_RANGE = 15` seconds).

### Noise Channels (`levin_notify.cpp`)

For anonymity zones with noise enabled (the default for `--tx-proxy`):

- **Channel count**: `CRYPTONOTE_NOISE_CHANNELS = 2` outgoing connections per zone.
- **Noise packet size**: `CRYPTONOTE_NOISE_BYTES = 3072` bytes (3 KiB).
- **Noise interval**: `CRYPTONOTE_NOISE_MIN_DELAY = 10` seconds + random `[0, CRYPTONOTE_NOISE_DELAY_RANGE = 5]` seconds.
- **Max fragments**: `CRYPTONOTE_MAX_FRAGMENTS = 20`, so max covert payload is ~60 KiB.

Each noise channel operates on its own strand:
1. At each interval, if the channel has a queued real message, it sends one noise-sized fragment of it; otherwise, it sends a dummy noise packet.
2. Real transactions are fragmented using `epee::levin::make_fragmented_notify()` into noise-sized chunks.
3. The `dandelionpp_fluff` flag is set to `false` (stem) for covert sends so the receiver forwards using Dandelion++.
4. Padding (to 1024-byte boundaries) is applied to non-noise fluff messages for traffic analysis resistance.

### P2P Command Filtering (`src/p2p/net_node.cpp`)

Only three P2P commands are permitted over anonymity networks:
1. `COMMAND_HANDSHAKE` -- required for connection establishment.
2. `COMMAND_TIMED_SYNC` -- lightweight peer synchronization.
3. `NOTIFY_NEW_TRANSACTIONS` -- transaction broadcast.

All other commands (block propagation, chain sync, etc.) are filtered by `is_filtered_command()` for non-public zones. This means blockchain synchronization cannot occur over hidden services.

## Dependencies

### What This Module Depends On

- **boost::asio**: Async I/O, timers, strands for thread-safe SOCKS and relay operations.
- **epee serialization**: KV serialization for address storage/loading in P2P messages.
- **epee::net_utils**: Base network address types (`ipv4_network_address`, `ipv6_network_address`), zone enums.
- **epee::levin**: Protocol handler for P2P messaging; `make_fragmented_notify()` for noise fragmentation.
- **crypto**: `crypto::rand_idx()`, `crypto::random_device`, `crypto::random_poisson_seconds` for randomized timing.
- **common/expect.h**: `expect<T>` error-or-value type used throughout address parsing.
- **blockchain_db**: `txpool_tx_meta_t` for Dandelion++ state tracking in the transaction pool.

### What Depends On This Module

- **p2p/net_node**: Uses `proxy` and `anonymous_inbound` structs; calls `socks_connect_internal()` for outbound anonymous connections.
- **cryptonote_protocol_handler**: Inspects `dandelionpp_fluff` flag on incoming `NOTIFY_NEW_TRANSACTIONS` messages; routes transactions to stem/fluff relay methods.
- **tx_memory_pool**: Manages Dandelion++ embargo timers; decides when to upgrade stem transactions to fluff.
- **simplewallet / wallet RPC**: Uses `socks::connector` via `--proxy` to connect to daemon through Tor/I2P.

## Configuration

### Command-Line Options

| Option | Format | Description |
|--------|--------|-------------|
| `--tx-proxy` | `<network-type>,[socks5://[user:pass@]]<ip:port>[,max_connections][,disable_noise]` | Configure SOCKS proxy for anonymity network tx relay. Network type is `tor` or `i2p`. Example: `tor,127.0.0.1:9050,10` |
| `--anonymous-inbound` | `<hidden-service-address>,<[bind-ip:]port>[,max_connections]` | Accept inbound anonymous connections. Example: `x.onion:18084,127.0.0.1:18084,25` |
| `--proxy` | `<ip:port>` | Route all clearnet P2P traffic through a SOCKS proxy (does NOT connect to hidden services) |

### Compile-Time Constants (`src/cryptonote_config.h`)

| Constant | Value | Description |
|----------|-------|-------------|
| `CRYPTONOTE_DANDELIONPP_STEMS` | 2 | Outgoing stem connections per epoch |
| `CRYPTONOTE_DANDELIONPP_FLUFF_PROBABILITY` | 20 | Percent chance of fluff epoch (out of 100) |
| `CRYPTONOTE_DANDELIONPP_MIN_EPOCH` | 10 min | Minimum epoch duration |
| `CRYPTONOTE_DANDELIONPP_EPOCH_RANGE` | 30 sec | Random range added to min epoch |
| `CRYPTONOTE_DANDELIONPP_FLUSH_AVERAGE` | 5 sec | Mean Poisson delay for fluff flush |
| `CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE` | 39 sec | Mean Poisson delay for stem embargo timeout |
| `CRYPTONOTE_NOISE_CHANNELS` | 2 | Outgoing noise connections per anonymity zone |
| `CRYPTONOTE_NOISE_BYTES` | 3072 | Noise packet size (3 KiB) |
| `CRYPTONOTE_NOISE_MIN_EPOCH` | 5 min | Minimum noise epoch duration |
| `CRYPTONOTE_NOISE_EPOCH_RANGE` | 30 sec | Random range added to noise epoch |
| `CRYPTONOTE_NOISE_MIN_DELAY` | 10 sec | Minimum delay between noise packets |
| `CRYPTONOTE_NOISE_DELAY_RANGE` | 5 sec | Random range added to noise delay |
| `CRYPTONOTE_MAX_FRAGMENTS` | 20 | Max noise-sized fragments per covert message (~60 KiB) |
| `CRYPTONOTE_FORWARD_DELAY_BASE` | 15 sec | Base delay for anonymity-to-public forwarding |
| `CRYPTONOTE_FORWARD_DELAY_AVERAGE` | 22.5 sec | Average forward delay (base + base/2) |

## Known Issues

Found `TODO`/`FIXME` comments in the anonymity-related code:

- `src/net/tor_address.cpp:63` -- `//! \TODO v3 has checksum, base32 decoding is required to verify it` -- Tor v3 address checksum is not validated; only length and alphabet are checked.
- `src/net/i2p_address.cpp:47` -- `// !TODO only b32 addresses right now` -- Only b32 I2P addresses are supported; full base64 I2P addresses are not handled.
- `src/cryptonote_core/tx_pool.cpp:87-91` -- `//TODO: constants such as these should at least be in the header` -- Relay timing constants (`MIN_RELAY_TIME`, `MAX_RELAY_TIME`, `ACCEPT_THRESHOLD`) are defined locally in the `.cpp` file rather than in a header, making them difficult to test or configure.
- `src/cryptonote_core/tx_pool.cpp:206` -- `// TODO: Investigate why not?` -- Unclear why a certain path is not taken.
- `src/cryptonote_core/tx_pool.cpp:504` -- `//FIXME: Can return early before removal of all of the key images.` -- Potential incomplete cleanup on error paths.
- `src/cryptonote_core/tx_pool.cpp:730` -- `//TODO: investigate whether boolean return is appropriate` -- Return type may not convey enough information.
- `src/cryptonote_core/tx_pool.cpp:785` -- `//TODO: investigate whether boolean return is appropriate` -- Same as above for `get_relayable_transactions`.
- `src/cryptonote_core/tx_pool.cpp:1190` -- `//TODO: investigate whether boolean return is appropriate` -- Same pattern.
- `src/cryptonote_core/tx_pool.cpp:1567` -- `//TODO: investigate whether boolean return is appropriate` -- Same pattern.
- `src/p2p/net_node.inl:779` -- `// TODO: a domain can be set through socks, so that the remote side does the lookup for the DNS seed nodes.` -- DNS seed lookups are not routed through SOCKS for remote resolution.
- `src/p2p/net_node.inl:788` -- `// TODO: at some point add IPv6 support` -- IPv6 seed node support is not yet implemented.
- `src/p2p/net_node.inl:808` -- `// TODO: care about dnssec avail/valid` -- DNSSEC validation for seed nodes is not fully handled.
