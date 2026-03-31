# P2P Protocol

## Overview

The P2P protocol module implements Monero's peer-to-peer networking layer, responsible for peer discovery, connection management, block synchronization, transaction relay, and ban/blocklist enforcement. It is split into two cooperating subsystems: the lower-level **node server** (`nodetool::node_server`) that manages TCP connections, peerlists, handshakes, pings, and banning; and the higher-level **cryptonote protocol handler** (`cryptonote::t_cryptonote_protocol_handler`) that implements the blockchain-specific sync logic including block/transaction propagation, fluffy block relay, Dandelion++ transaction routing, and chain request/response flows. Communication between nodes uses the Levin binary protocol over TCP, with support for multiple network zones (public clearnet, Tor, I2P) each with independent peerlists and connection settings.

## Key Files

| File | Lines | Description |
|------|------:|-------------|
| `src/p2p/net_node.h` | 563 | `node_server` class template declaration: connection management, peer discovery, ban logic, UPnP, command-line options |
| `src/p2p/net_node.inl` | 3257 | `node_server` implementation: handshake/timed-sync handlers, connections_maker, peerlist housekeeping, DNS seed resolution, block/subnet banning, UPnP port mapping |
| `src/p2p/p2p_protocol_defs.h` | 299 | P2P-layer protocol message definitions: `COMMAND_HANDSHAKE`, `COMMAND_TIMED_SYNC`, `COMMAND_PING`, `COMMAND_REQUEST_SUPPORT_FLAGS`; data structures `peerlist_entry`, `anchor_peerlist_entry`, `basic_node_data`, `network_config` |
| `src/p2p/net_peerlist.h` | 553 | `peerlist_manager` and `peerlist_storage` classes: white/gray/anchor peerlist management with Boost.MultiIndex containers, trimming, merging, filtering, anonymized peerlist head retrieval |
| `src/p2p/net_peerlist_boost_serialization.h` | 252 | Boost.Serialization support for `peerlist_entry`, `anchor_peerlist_entry`, and network address types (IPv4, IPv6, Tor, I2P) for persisting peerlists to disk |
| `src/p2p/net_node_common.h` | 139 | `i_p2p_endpoint` interface and `p2p_endpoint_stub`: abstract API between protocol handler and node server |
| `src/cryptonote_protocol/cryptonote_protocol_handler.h` | 240 | `t_cryptonote_protocol_handler` class template declaration: block/tx notification handlers, sync state, block queue, pruning stripe management |
| `src/cryptonote_protocol/cryptonote_protocol_handler.inl` | 2908 | Protocol handler implementation: block sync logic, fluffy block handling, tx relay with Dandelion++, chain request/response, idle peer kicking, sync time estimation |
| `src/cryptonote_protocol/cryptonote_protocol_defs.h` | 379 | Cryptonote-layer protocol message definitions: `NOTIFY_NEW_BLOCK`, `NOTIFY_NEW_TRANSACTIONS`, `NOTIFY_REQUEST_GET_OBJECTS`, `NOTIFY_RESPONSE_GET_OBJECTS`, `NOTIFY_REQUEST_CHAIN`, `NOTIFY_RESPONSE_CHAIN_ENTRY`, `NOTIFY_NEW_FLUFFY_BLOCK`, `NOTIFY_REQUEST_FLUFFY_MISSING_TX`, `NOTIFY_GET_TXPOOL_COMPLEMENT`; also `CORE_SYNC_DATA`, `block_complete_entry`, `connection_info` |
| `src/cryptonote_protocol/cryptonote_protocol_handler_common.h` | 68 | `i_cryptonote_protocol` interface: `relay_block`, `relay_transactions`, `is_synchronized` |
| `src/cryptonote_protocol/block_queue.h` | 114 | `block_queue` class: queuing downloaded block spans for ordered addition to the blockchain |
| `src/cryptonote_basic/connection_context.h` | 156 | `cryptonote_connection_context`: per-connection state including sync state machine, requested objects, pruning seed, scoring |

## Data Structures

### P2P Layer Structures (in `nodetool` namespace)

#### `peerlist_entry_base<AddressType>` / `peerlist_entry`
```cpp
struct peerlist_entry {
    network_address adr;         // peer network address
    peerid_type id;              // uint64_t unique peer identifier
    int64_t last_seen;           // timestamp of last successful contact
    uint32_t pruning_seed;       // peer's pruning configuration
    uint16_t rpc_port;           // peer's RPC port (0 if unknown)
    uint32_t rpc_credits_per_hash; // peer's RPC credit rate
};
```

#### `anchor_peerlist_entry_base<AddressType>` / `anchor_peerlist_entry`
```cpp
struct anchor_peerlist_entry {
    network_address adr;         // peer network address
    peerid_type id;              // unique peer identifier
    int64_t first_seen;          // timestamp of first successful outgoing connection
};
```

#### `basic_node_data`
Exchanged during handshake to identify a peer.
```cpp
struct basic_node_data {
    uuid network_id;             // identifies mainnet/testnet/stagenet
    uint32_t my_port;            // listening port (0 if hidden)
    uint16_t rpc_port;           // RPC port
    uint32_t rpc_credits_per_hash;
    peerid_type peer_id;         // random uint64 identifying this node
    uint32_t support_flags;      // feature flags (e.g., fluffy blocks)
};
```

#### `network_config`
Per-zone network configuration.
```cpp
struct network_config {
    milliseconds ping_connection_timeout;  // default 2000ms
    uint32_t max_out_connection_count;     // default 12
    uint32_t max_in_connection_count;
    uint32_t connection_timeout;           // default 5000ms
    uint32_t handshake_interval;           // default 60s
    uint32_t packet_max_size;              // default 50MB
    uint32_t config_id;
    uint32_t send_peerlist_sz;             // default 250
};
```

#### `p2p_connection_context_t<base_type>`
Extends the base connection context with P2P-specific fields.
```cpp
struct p2p_connection_context_t : base_type {
    peerid_type peer_id;
    uint32_t support_flags;
    bool m_in_timedsync;
    bool is_ping;
    std::set<network_address> sent_addresses;  // tracks addresses already sent to this peer
};
```

#### `network_zone` (inner struct of `node_server`)
Each network zone (public, Tor, I2P) has its own:
- `net_server` (TCP server with Levin protocol)
- `peerlist_manager` (white/gray/anchor peerlists)
- `config` (peer_id, support_flags, network_config)
- `m_notifier` (Dandelion++ transaction notification engine)
- Bind address/port, proxy address, seed nodes
- Atomic counters for current in/out peer counts

#### `peerlist_manager`
Manages three Boost.MultiIndex containers indexed by address and last_seen time:
- `m_peers_white` -- verified, reachable peers (limit: 1000)
- `m_peers_gray` -- unverified peers received from others (limit: 5000)
- `m_peers_anchor` -- peers successfully connected to as outgoing, used for Eclipse attack resistance

### Cryptonote Protocol Layer Structures

#### `CORE_SYNC_DATA`
Exchanged as payload data in handshakes and timed syncs.
```cpp
struct CORE_SYNC_DATA {
    uint64_t current_height;
    uint64_t cumulative_difficulty;
    uint64_t cumulative_difficulty_top64;  // for 128-bit difficulty
    crypto::hash top_id;                   // hash of top block
    uint8_t top_version;                   // hard fork version of top block
    uint32_t pruning_seed;                 // node's pruning seed
};
```

#### `cryptonote_connection_context`
Per-connection state for the sync protocol:
```cpp
struct cryptonote_connection_context : connection_context_base {
    enum state { state_before_handshake, state_synchronizing, state_standby, state_normal };
    state m_state;
    vector<pair<hash, uint64_t>> m_needed_objects;   // block hashes to download
    unordered_set<hash> m_requested_objects;          // blocks currently requested
    uint64_t m_remote_blockchain_height;
    ptime m_last_request_time;
    hash m_last_known_hash;
    uint32_t m_pruning_seed;
    int32_t m_score;                                  // peer reputation score
    int m_expect_response;                            // expected next response command ID
    uint64_t m_expect_height;
};
```

### P2P Protocol Messages (Command IDs)

| ID | Name | Type | Description |
|----|------|------|-------------|
| 1001 | `COMMAND_HANDSHAKE` | invoke | Initial connection setup; exchanges `basic_node_data` + `CORE_SYNC_DATA` + peerlist |
| 1002 | `COMMAND_TIMED_SYNC` | invoke | Periodic sync; exchanges `CORE_SYNC_DATA` + peerlist updates |
| 1003 | `COMMAND_PING` | invoke | Pingback verification that a peer's advertised port is reachable |
| 1007 | `COMMAND_REQUEST_SUPPORT_FLAGS` | invoke | Request peer's feature support flags |

### Cryptonote Protocol Messages (Command IDs)

| ID | Name | Type | Description |
|----|------|------|-------------|
| 2001 | `NOTIFY_NEW_BLOCK` | notify | Legacy new block notification (redirected to fluffy handler) |
| 2002 | `NOTIFY_NEW_TRANSACTIONS` | notify | New transaction relay; includes Dandelion++ fluff flag |
| 2003 | `NOTIFY_REQUEST_GET_OBJECTS` | notify | Request blocks by hash (up to 100 blocks) |
| 2004 | `NOTIFY_RESPONSE_GET_OBJECTS` | notify | Response with requested block data |
| 2006 | `NOTIFY_REQUEST_CHAIN` | notify | Request chain history (block hashes with exponential spacing) |
| 2007 | `NOTIFY_RESPONSE_CHAIN_ENTRY` | notify | Response with chain entry: start height, block IDs, weights |
| 2008 | `NOTIFY_NEW_FLUFFY_BLOCK` | notify | New block with only transaction hashes (fluffy blocks) |
| 2009 | `NOTIFY_REQUEST_FLUFFY_MISSING_TX` | notify | Request missing transactions by index within a block |
| 2010 | `NOTIFY_GET_TXPOOL_COMPLEMENT` | notify | Request txpool complement (txes not in provided hash set) |

## Public API

### Peer Discovery and Management

```cpp
// Register all P2P command-line options
static void node_server::init_options(boost::program_options::options_description& desc);

// Initialize node from command-line options; optional SOCKS proxy
bool node_server::init(const variables_map& vm, const string& proxy = {}, bool proxy_dns_leaks_allowed = {});

// Deinitialize: kill threads, close connections, delete UPnP mapping, store config
bool node_server::deinit();

// Main event loop: starts peer monitor thread, idle handlers, runs TCP server with 10 threads
bool node_server::run();

// Get counts for the public zone only
uint64_t get_public_connections_count();
size_t get_public_outgoing_connections_count();
size_t get_public_white_peers_count();
size_t get_public_gray_peers_count();

// Retrieve peerlist entries
void get_public_peerlist(vector<peerlist_entry>& gray, vector<peerlist_entry>& white);
void get_peerlist(vector<peerlist_entry>& gray, vector<peerlist_entry>& white);

// Adjust connection limits at runtime
void change_max_out_public_peers(size_t count);
uint32_t get_max_out_public_peers() const;
void change_max_in_public_peers(size_t count);
uint32_t get_max_in_public_peers() const;
```

### Handshake Protocol

```cpp
// Outgoing: send COMMAND_HANDSHAKE to a connected peer; optionally just take peerlist
bool do_handshake_with_peer(peerid_type& pi, p2p_connection_context& context, bool just_take_peerlist = false);

// Incoming: handle COMMAND_HANDSHAKE from a remote peer
int handle_handshake(int command, COMMAND_HANDSHAKE::request& arg, COMMAND_HANDSHAKE::response& rsp, p2p_connection_context& context);

// Periodic: send COMMAND_TIMED_SYNC to all handshaked peers
bool do_peer_timed_sync(const connection_context_base& context, peerid_type peer_id);

// Handle incoming timed sync
int handle_timed_sync(int command, COMMAND_TIMED_SYNC::request& arg, COMMAND_TIMED_SYNC::response& rsp, p2p_connection_context& context);

// Handle ping request (for pingback verification)
int handle_ping(int command, COMMAND_PING::request& arg, COMMAND_PING::response& rsp, p2p_connection_context& context);
```

### Block/TX Sync (Cryptonote Protocol Handler)

```cpp
// Process CORE_SYNC_DATA from a peer; triggers sync if peer is ahead
bool process_payload_sync_data(const CORE_SYNC_DATA& hshd, cryptonote_connection_context& context, bool is_initial);

// Get local sync data to send to peers
bool get_payload_sync_data(CORE_SYNC_DATA& hshd);

// Handle incoming notifications
int handle_notify_new_block(int command, NOTIFY_NEW_BLOCK::request& arg, ...);
int handle_notify_new_fluffy_block(int command, NOTIFY_NEW_FLUFFY_BLOCK::request& arg, ...);
int handle_request_fluffy_missing_tx(int command, NOTIFY_REQUEST_FLUFFY_MISSING_TX::request& arg, ...);
int handle_notify_new_transactions(int command, NOTIFY_NEW_TRANSACTIONS::request& arg, ...);
int handle_request_get_objects(int command, NOTIFY_REQUEST_GET_OBJECTS::request& arg, ...);
int handle_response_get_objects(int command, NOTIFY_RESPONSE_GET_OBJECTS::request& arg, ...);
int handle_request_chain(int command, NOTIFY_REQUEST_CHAIN::request& arg, ...);
int handle_response_chain_entry(int command, NOTIFY_RESPONSE_CHAIN_ENTRY::request& arg, ...);
int handle_notify_get_txpool_complement(int command, NOTIFY_GET_TXPOOL_COMPLEMENT::request& arg, ...);

// Relay methods
bool relay_block(NOTIFY_NEW_FLUFFY_BLOCK::request& arg, cryptonote_connection_context& exclude_context);
bool relay_transactions(NOTIFY_NEW_TRANSACTIONS::request& arg, const uuid& source, zone zone, relay_method tx_relay);

// Sync state queries
bool is_synchronized() const;
bool is_busy_syncing();
bool needs_new_sync_connections(zone zone) const;
pair<uint32_t, uint32_t> get_next_needed_pruning_stripe() const;
```

### Ban Logic

```cpp
// Block a host for a given duration (default 24 hours); drops existing connections
bool block_host(network_address address, time_t seconds = P2P_IP_BLOCKTIME, bool add_only = false);
bool unblock_host(const network_address& address);

// Block/unblock an IPv4 subnet
bool block_subnet(const ipv4_network_subnet& subnet, time_t seconds = P2P_IP_BLOCKTIME);
bool unblock_subnet(const ipv4_network_subnet& subnet);

// Query block status
bool is_host_blocked(const network_address& address, time_t* seconds);
map<string, time_t> get_blocked_hosts();
map<ipv4_network_subnet, time_t> get_blocked_subnets();

// Increment fail score; auto-blocks at threshold (P2P_IP_FAILS_BEFORE_BLOCK = 10)
bool add_host_fail(const network_address& address, unsigned int score = 1);

// Connection filter callback: checks blocked hosts, subnets, and connection limits
bool is_remote_host_allowed(const network_address& address, time_t* t = NULL);
bool is_host_limit(const network_address& address);
```

### P2P Endpoint Interface (`i_p2p_endpoint`)

```cpp
// Send notification to a list of connections (sorted by zone)
bool relay_notify_to_list(int command, message_writer message, vector<pair<zone, uuid>> connections);

// Send transactions via the appropriate network zone's notifier (Dandelion++)
zone send_txs(vector<blobdata> txs, zone origin, const uuid& source, relay_method tx_relay);

// Send notification to a specific peer
bool invoke_notify_to_peer(int command, message_writer message, const connection_context_base& context);

// Drop a connection
bool drop_connection(const connection_context_base& context);

// Iterate over all connections
void for_each_connection(function<bool(connection_context&, peerid_type, uint32_t)> f);
bool for_connection(const uuid&, function<bool(connection_context&, peerid_type, uint32_t)> f);
```

## Internal Logic

### Peer Discovery and Selection

**Seed Nodes:** On first startup (or when no white peers exist), the node connects to seed nodes. DNS-based seed resolution queries four `moneroseeds.*` domains in parallel threads with a timeout (`CRYPTONOTE_DNS_TIMEOUT_MS`). If DNS yields fewer than 12 results (`MIN_WANTED_SEED_NODES`), hardcoded IP fallback seeds are added. Tor and I2P zones have their own hardcoded `.onion` and `.b32.i2p` seed addresses.

**Peerlist Types:**
- **White list** (limit 1000): Peers verified as reachable. Updated when a successful handshake completes or timed sync succeeds on an outgoing connection.
- **Gray list** (limit 5000): Peers received from other nodes' peerlists but not yet verified. Added via `merge_peerlist` from handshake/timed-sync responses.
- **Anchor list**: Peers the node has successfully connected to as outgoing. Used as first priority when making new connections to resist Eclipse attacks.

**Connection Making (`connections_maker`):** Called every 1 second via idle handler. For each zone:
1. If no white peers exist, connect to seeds to bootstrap.
2. Connect to priority peers.
3. Fill outgoing connections in priority order:
   - **Anchor peers** first (up to `P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT` = 2)
   - **White list** peers (up to `P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT` = 70% of max outgoing)
   - **Gray list** peers (remaining slots)
4. If still below max and no progress, retry seeds.

**Peer Selection Algorithm (`make_new_connection_from_peerlist`):**
1. Deduplicate peers by host (ignore multiple ports on the same IP).
2. Deduplicate by /24 subnet to prevent a single operator from dominating peer selection.
3. Filter by needed pruning stripe (prefer matching stripe, accept unknown).
4. For white list: use `get_random_index_with_fixed_probability` which applies a cubic distribution favoring more recently seen peers. Also attempts to reuse previously successful stripe peers.
5. For gray list: uniform random selection.
6. Up to 3 outer retry attempts per call.

**Peerlist Anonymization:** When generating a peerlist head to send in handshake/timed-sync responses, if anonymization is enabled (always true for responses), the entire white peerlist is shuffled and truncated to `depth` (250), with all `last_seen` timestamps zeroed. This prevents timing attacks described in "Exploring the Monero Peer-to-Peer Network" (Cao, Tong et al., 2019).

**Gray Peerlist Housekeeping:** Every 30 minutes, a random gray peer is selected, a test handshake is attempted, and on success the peer is promoted to white; on failure it is evicted from gray.

### Handshake Flow

**Outgoing Connection (initiator side):**
1. `try_to_connect_and_handshake_with_new_peer` checks outgoing peer limits, establishes TCP connection via `zone.m_connect` (direct or SOCKS proxy).
2. `do_handshake_with_peer` sends `COMMAND_HANDSHAKE` with local `basic_node_data` and `CORE_SYNC_DATA`.
3. Async response handler validates: network_id match, processes remote peerlist, processes payload sync data.
4. If peer_id matches own (self-connection), drops.
5. On success: updates white peerlist (`set_peer_just_seen`), adds to anchor peerlist, notifies the Dandelion++ notifier.
6. If `support_flags == 0`, asynchronously requests them via `COMMAND_REQUEST_SUPPORT_FLAGS`.

**Incoming Connection (responder side):**
1. `handle_handshake` validates: correct network_id, connection is incoming, no prior handshake on this connection, not self-connection.
2. Processes payload sync data (may trigger sync).
3. Associates peer_id with connection context.
4. If peer advertises a port and zone supports pingback: performs async ping to verify reachability. Only on successful ping is the peer added to white peerlist.
5. Returns response with local `basic_node_data`, `CORE_SYNC_DATA`, and anonymized peerlist head (up to 250 entries).

**Timed Sync:** Every `P2P_DEFAULT_HANDSHAKE_INTERVAL` (60s), `peer_sync_idle_maker` sends `COMMAND_TIMED_SYNC` to all handshaked peers. The response includes updated `CORE_SYNC_DATA` and new peerlist entries (only entries not previously sent to that peer, tracked via `sent_addresses`).

### Block Synchronization Protocol

**State Machine:** Each connection has a state: `before_handshake` -> `synchronizing` / `standby` -> `normal`.

**Chain Sync Initiation:** When `process_payload_sync_data` determines a peer is ahead (has blocks we do not have), the connection enters `state_synchronizing` and a callback is requested.

**Callback Flow:**
1. On callback, if synchronizing with no pending request: send `NOTIFY_REQUEST_CHAIN` with a short chain history (block hashes with exponential spacing, always ending at genesis). Includes `prune` flag if pruned sync is enabled.
2. Peer responds with `NOTIFY_RESPONSE_CHAIN_ENTRY`: start_height, total_height, cumulative_difficulty, block IDs, block weights.
3. Handler validates the chain entry, populates `m_needed_objects`.
4. `request_missing_objects` sends `NOTIFY_REQUEST_GET_OBJECTS` for batches of block hashes (up to 100 per request). Considers pruning stripes to request from appropriate peers.
5. Peer responds with `NOTIFY_RESPONSE_GET_OBJECTS`: block blobs + transaction blobs.
6. Handler validates blocks (parses, checks hashes match requested, verifies chain continuity), adds spans to `block_queue`.
7. `try_add_next_blocks` takes spans from the queue in order and submits them to core for processing.

**Dynamic Span Sizing:** The `calculate_dynamic_span` method adjusts `m_span_limit` based on current block processing rate (`blocks_per_seconds`) and average block size, with a minimum of `BLOCK_QUEUE_NSPANS_MINIMUM` (10) spans.

**Idle Peer Management:** Every 8 seconds, `kick_idle_peers` checks synchronizing peers. If a peer has been idle for `IDLE_PEER_KICK_TIME` (240s) or non-responsive for `NON_RESPONSIVE_PEER_KICK_TIME` (20s), it is moved to standby. Peers with negative scores are dropped entirely.

**Fluffy Block Relay:** New blocks are relayed as "fluffy blocks" (`NOTIFY_NEW_FLUFFY_BLOCK`) containing the block header but only transaction hashes. If the receiver is missing transactions, it requests them by index via `NOTIFY_REQUEST_FLUFFY_MISSING_TX`. The sender looks up the requested transactions and responds with a complete fluffy block. Blocks that provide all txs needed (supplemented from mempool and blockchain) bypass the mempool entirely for faster propagation via `pool_supplement`.

### Transaction Relay

**Dandelion++ Protocol:** Transactions are relayed using Dandelion++ for privacy:
- Transactions received over public network enter **stem** phase (`relay_method::stem`).
- Transactions received over anonymity networks (Tor/I2P) enter **forward** phase with randomized delay, or **fluff** if the sender set the `dandelionpp_fluff` flag.
- Stem transactions are relayed to a single selected peer; fluff transactions are broadcast to all peers.
- The `m_notifier` (per-zone `cryptonote::levin::notify`) handles the Dandelion++ state machine, noise generation for anonymity networks, and epoch-based relay changes.

**Transaction Verification:** Each incoming transaction is verified via `core::handle_incoming_tx`. On verification failure (unless it is a "no-drop offense"), the connection is dropped. Successfully verified transactions are categorized by their relay method and forwarded accordingly.

**TX Pool Complement:** `NOTIFY_GET_TXPOOL_COMPLEMENT` allows a newly synchronized peer to request all transactions in the remote node's mempool that the local node does not have, identified by sending a list of already-known transaction hashes.

### Ban Scoring System

**Host Fail Scoring:** `add_host_fail(address, score)` increments a per-host fail counter. When the counter exceeds `P2P_IP_FAILS_BEFORE_BLOCK` (10), the host is blocked for `P2P_IP_BLOCKTIME` (24 hours) and the counter is halved.

**Connection-Level Scoring:** The protocol handler maintains a per-connection `m_score` (in `cryptonote_connection_context`). Various misbehaviors decrease the score (`hit_score`). When score reaches `DROP_PEERS_ON_SCORE` (-2), the connection is dropped. Bad PoW on a block triggers an immediate block with the full `P2P_IP_FAILS_BEFORE_BLOCK` score.

**Blocking Mechanics:**
- `block_host`: Records host string with expiry timestamp, immediately closes all connections to that host across all zones, and evicts the host from all peerlists (white, gray, anchor).
- `block_subnet`: Records /24 subnet with expiry, closes matching connections, filters matching peers from all peerlists.
- Blocks expire automatically: `is_remote_host_allowed` checks timestamps and removes expired entries.

**DNS Blocklist:** Every 7000 seconds on mainnet, `update_dns_blocklist` queries multiple `blocklist.moneropulse.*` DNS domains for TXT records containing IPs/subnets to block. Entries are blocked for `DNS_BLOCKLIST_LIFETIME` (8 days).

**Per-IP Connection Limit:** `has_too_many_connections` enforces a maximum number of incoming connections from a single IP address (`max_connections`, configurable via `--max-connections-per-ip`, default 1). Checked in `is_host_limit` before accepting new incoming connections.

**Failed Address Cache:** `record_addr_failed` timestamps connection failures per host. `is_addr_recently_failed` skips addresses that failed within the last `P2P_FAILED_ADDR_FORGET_SECONDS` (1 hour) to avoid wasting connection attempts.

## Dependencies

### What This Module Depends On

| Dependency | Purpose |
|-----------|---------|
| `net/abstract_tcp_server2.h` | Boost.Asio-based TCP server (`boosted_tcp_server`) |
| `net/levin_protocol_handler_async.h` | Levin binary protocol framing and async invoke |
| `storages/levin_abstract_invoke2.h` | Levin command invoke/notify macros |
| `cryptonote_core/cryptonote_core.h` | Core blockchain interface for block/tx handling |
| `common/dns_utils.h` | DNS resolution for seed nodes and blocklists |
| `common/pruning.h` | Pruning seed/stripe utilities |
| `crypto/crypto.h` | Random number generation for peer selection |
| `cryptonote_protocol/levin_notify.h` | Dandelion++ transaction notification engine |
| `cryptonote_protocol/block_queue.h` | Ordered block span queue for sync |
| `miniupnpc` | UPnP port mapping for NAT traversal |
| `net/socks.h` | SOCKS proxy support for Tor/I2P |
| `net/tor_address.h`, `net/i2p_address.h` | Anonymity network address types |

### What Depends On This Module

| Dependent | Usage |
|-----------|-------|
| `daemon/` | Creates and runs the `node_server` instance |
| `rpc/` | Queries connection info, peer lists, ban status via `node_server` public API |
| `cryptonote_core/` | Receives blocks/transactions from protocol handler; provides sync data |

## Configuration

### Command-Line Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `--p2p-bind-ip` | string | `"0.0.0.0"` | IPv4 address to bind P2P server |
| `--p2p-bind-ipv6-address` | string | `"::"` | IPv6 address to bind P2P server |
| `--p2p-bind-port` | string | `18080`/`28080`/`38080` | P2P listening port (network-dependent) |
| `--p2p-bind-port-ipv6` | string | (same as IPv4) | IPv6 P2P listening port |
| `--p2p-use-ipv6` | bool | `false` | Enable IPv6 support |
| `--p2p-ignore-ipv4` | bool | `false` | Do not require IPv4 |
| `--p2p-external-port` | uint32 | `0` | External port to advertise (if behind NAT) |
| `--p2p-allow-local-ip` | bool | `false` | Allow connecting to local/private IPs |
| `--add-peer` | string[] | -- | Manually add peer to peerlist |
| `--add-priority-node` | string[] | -- | Priority peers to always try connecting to |
| `--add-exclusive-node` | string[] | -- | Connect ONLY to these peers |
| `--seed-node` | string[] | -- | Additional seed nodes |
| `--tx-proxy` | string[] | -- | SOCKS proxy for anonymity zones (format: `zone,address[:port][,max_connections][,noise]`) |
| `--anonymous-inbound` | string[] | -- | Hidden service inbound config (format: `address,bind_ip:port[,max_connections]`) |
| `--ban-list` | string | -- | Path to file with IPs/subnets to ban |
| `--hide-my-port` | bool | `false` | Do not advertise listening port |
| `--no-sync` | bool | `false` | Disable blockchain sync |
| `--enable-dns-blocklist` | bool | `false` | Enable DNS-based peer blocklist |
| `--no-igd` | bool | `false` | Disable UPnP (deprecated, use `--igd`) |
| `--igd` | string | `"delayed"` | UPnP mode: `enabled`, `disabled`, or `delayed` |
| `--out-peers` | int64 | `-1` (use default 12) | Maximum outgoing connections |
| `--in-peers` | int64 | `-1` (use default) | Maximum incoming connections |
| `--tos-flag` | int | `-1` | Type of Service flag for P2P packets |
| `--limit-rate-up` | int64 | `8192` | Upload rate limit (kB/s) |
| `--limit-rate-down` | int64 | `32768` | Download rate limit (kB/s) |
| `--limit-rate` | int64 | `-1` | Combined rate limit (kB/s) |
| `--pad-transactions` | bool | `false` | Pad relayed transactions to reduce fingerprinting |
| `--max-connections-per-ip` | uint32 | `1` | Max incoming connections from a single IP |
| `--offline` | bool | `false` | Run without P2P networking |

### Compile-Time Constants (from `cryptonote_config.h`)

| Constant | Value | Description |
|----------|-------|-------------|
| `P2P_DEFAULT_CONNECTIONS_COUNT` | 12 | Default max outgoing connections |
| `P2P_DEFAULT_HANDSHAKE_INTERVAL` | 60 | Seconds between timed sync rounds |
| `P2P_DEFAULT_PACKET_MAX_SIZE` | 50000000 | Maximum Levin packet size (50 MB) |
| `P2P_DEFAULT_PEERS_IN_HANDSHAKE` | 250 | Peerlist entries sent in handshake/sync |
| `P2P_MAX_PEERS_IN_HANDSHAKE` | 250 | Maximum accepted peerlist entries |
| `P2P_DEFAULT_CONNECTION_TIMEOUT` | 5000 | Connection timeout (ms) |
| `P2P_DEFAULT_PING_CONNECTION_TIMEOUT` | 2000 | Ping connection timeout (ms) |
| `P2P_DEFAULT_INVOKE_TIMEOUT` | 120000 | Levin invoke timeout (ms) |
| `P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT` | 5000 | Handshake invoke timeout (ms) |
| `P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT` | 70 | Target % of outgoing from white list |
| `P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT` | 2 | Target anchor connections |
| `P2P_DEFAULT_LIMIT_RATE_UP` | 8192 | Default upload limit (kB/s) |
| `P2P_DEFAULT_LIMIT_RATE_DOWN` | 32768 | Default download limit (kB/s) |
| `P2P_LOCAL_WHITE_PEERLIST_LIMIT` | 1000 | Maximum white peerlist entries |
| `P2P_LOCAL_GRAY_PEERLIST_LIMIT` | 5000 | Maximum gray peerlist entries |
| `P2P_FAILED_ADDR_FORGET_SECONDS` | 3600 | Time before retrying a failed address (1h) |
| `P2P_IP_BLOCKTIME` | 86400 | Default host block duration (24h) |
| `P2P_IP_FAILS_BEFORE_BLOCK` | 10 | Fail score threshold for auto-blocking |
| `P2P_SUPPORT_FLAG_FLUFFY_BLOCKS` | 0x01 | Fluffy block support flag |
| `DNS_BLOCKLIST_LIFETIME` | 691200 | DNS blocklist entry lifetime (8 days) |
| `CRYPTONOTE_NOISE_BYTES` | 3072 | Noise packet size for anonymity networks |
| `CURRENCY_PROTOCOL_MAX_OBJECT_REQUEST_COUNT` | 100 | Maximum blocks per GET_OBJECTS request |

### Periodic Timer Intervals

| Timer | Interval | Function |
|-------|----------|----------|
| `m_peer_handshake_idle_maker_interval` | 60s | `peer_sync_idle_maker` -- timed sync to all peers |
| `m_connections_maker_interval` | 1s | `connections_maker` -- maintain outgoing connections |
| `m_peerlist_store_interval` | 30 min | `store_config` -- persist peerlists to disk |
| `m_gray_peerlist_housekeeping_interval` | 60s | `gray_peerlist_housekeeping` -- verify/promote gray peers |
| `m_incoming_connections_interval` | 30 min | `check_incoming_connections` -- warn if no inbound |
| `m_dns_blocklist_interval` | ~7000s | `update_dns_blocklist` -- refresh DNS blocklist |
| `m_idle_peer_kicker` | 8s | `kick_idle_peers` -- remove stalled sync peers |
| `m_standby_checker` | 100ms | `check_standby_peers` -- activate standby peers |
| `m_sync_search_checker` | 101s | `update_sync_search` -- find new sync peers |
| `m_bad_peer_checker` | 43s | Bad peer detection (via sync protocol) |

## Sync State Machine Details

Source: `cryptonote_protocol_handler.inl`, `process_payload_sync_data()`, `cryptonote_connection_context` in connection_context.h.

### States

Each peer connection maintains a sync state (`cryptonote_connection_context::m_state`):

| State | Value | Description |
|-------|-------|-------------|
| `state_before_handshake` | 0 | Initial state; no sync data exchanged yet |
| `state_synchronizing` | 1 | Actively downloading blocks from this peer |
| `state_standby` | 2 | Peer has blocks we need but we're syncing from someone else |
| `state_normal` | 3 | Peer is at the same height or behind; normal operation |

### State Transitions

```
before_handshake → synchronizing   (peer is ahead, selected for sync)
before_handshake → standby         (peer is ahead, another peer already syncing)
before_handshake → normal          (peer is at same height or behind)
synchronizing    → standby         (idle timeout, IDLE_PEER_KICK_TIME = 240s)
synchronizing    → normal          (sync complete, peer caught up)
standby          → synchronizing   (activated when current sync peer stalls, checked every 100ms)
normal           → synchronizing   (peer announces new block we don't have)
```

### Stale Span Detection

In `kick_idle_peers()` (called every 8 seconds):
- If a synchronizing peer has been idle for `IDLE_PEER_KICK_TIME` (240 seconds), move to standby.
- If a peer has not responded within `NON_RESPONSIVE_PEER_KICK_TIME` (20 seconds) and its score is negative, drop the connection.
- Spans not completed within the timeout are re-requested from a different peer via the `block_queue`.

### Standby Activation

In `check_standby_peers()` (called every 100ms):
- Check if there are needed blocks that no synchronizing peer is providing.
- If so, activate a standby peer by transitioning it to `state_synchronizing`.

## Fluffy Block Protocol Details

Source: `handle_notify_new_fluffy_block()` in cryptonote_protocol_handler.inl.

### Protocol Flow

1. **Block announcement:** Sender creates a `NOTIFY_NEW_FLUFFY_BLOCK` message containing:
   - Full block header
   - Coinbase transaction (always included)
   - Transaction hashes only (no full tx blobs) for non-coinbase transactions

2. **Receiver processing:**
   a. Parse the block header and coinbase transaction.
   b. For each transaction hash in the block:
      - Check if the transaction exists in the local mempool.
      - If found, use the local copy.
   c. If any transactions are missing:
      - Send `NOTIFY_REQUEST_FLUFFY_MISSING_TX` with the indices of missing transactions.
   d. If all transactions are available:
      - Add the block directly to the blockchain (via `pool_supplement` path, bypassing normal mempool).

3. **Missing TX response:**
   - The original sender receives `NOTIFY_REQUEST_FLUFFY_MISSING_TX`.
   - Looks up the requested transactions (from blockchain or mempool).
   - Sends a new `NOTIFY_NEW_FLUFFY_BLOCK` with the full transaction blobs included.

4. **Fallback:** If the missing TX response fails or the resulting block is invalid, fall back to requesting the full block via `NOTIFY_REQUEST_GET_OBJECTS`.

### Support Detection

Fluffy block support is indicated by the `P2P_SUPPORT_FLAG_FLUFFY_BLOCKS` (0x01) bit in `support_flags`, exchanged during handshake.

## Dandelion++ Exact Parameters

Source: `src/net/dandelionpp.cpp`, `src/cryptonote_protocol/levin_notify.cpp`, `cryptonote_protocol_handler.inl`.

### Protocol Parameters

| Parameter | Value | Source |
|-----------|-------|--------|
| Embargo timeout | Poisson-distributed, mean ≈ `CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE` (170 seconds) | `levin_notify.cpp` |
| Stem probability | 90% stem, 10% fluff on receipt | Dandelion++ paper specification |
| Epoch duration | Connection-based; new epoch when stem relay connections change | `dandelionpp.cpp` |
| Stem relay count | 2 outbound connections per epoch | `dandelionpp.cpp`, `STEMS` constant |
| Noise interval | Poisson-distributed, mean = `CRYPTONOTE_NOISE_MIN_DELAY` + `CRYPTONOTE_NOISE_DELAY_RANGE`/2 | For anonymity network zones only |

### Stem/Fluff Decision

1. **Local transactions:** Sent as stem to selected stem relay peers.
2. **Received stem transactions:** With 90% probability, forward to the next stem peer. With 10% probability, fluff (broadcast to all peers).
3. **Embargo mechanism:** When a transaction enters the stem phase, an embargo timer starts. If the timer expires before the transaction is seen again from the network (as fluff), the node fluffs it. This prevents transactions from being stuck in stem phase if the stem path fails.
4. **Fluff trigger:** A transaction is fluffed when:
   - The embargo timer expires.
   - The node receives the same transaction back from the network.
   - The node is selected for fluff (10% probability on stem receipt).

### Network Zone Behavior

- **Public network:** Full Dandelion++ with stem/fluff phases.
- **Anonymity networks (Tor/I2P):** Transactions are forwarded with randomized delay (noise), not stem. The `dandelionpp_fluff` flag in `NOTIFY_NEW_TRANSACTIONS` is set for fluff transactions received over anonymity networks.

## Known Issues

The following TODO/FIXME/HACK/XXX comments exist in the P2P and cryptonote protocol source files:

- `src/p2p/net_node.inl:779` -- TODO: SOCKS proxy DNS lookup for seed nodes ("a domain can be set through socks, so that the remote side does the lookup for the DNS seed nodes")
- `src/p2p/net_node.inl:788` -- TODO: IPv6 support for DNS seed resolution ("at some point add IPv6 support, but that won't be relevant for some time yet")
- `src/p2p/net_node.inl:808` -- TODO: DNSSEC validation ("care about dnssec avail/valid")
- `src/cryptonote_protocol/cryptonote_protocol_handler.inl:594` -- TODO: Drop support for legacy `NOTIFY_NEW_BLOCK` endpoint ("@TODO: Eventually drop support for this endpoint")
- `src/cryptonote_protocol/cryptonote_protocol_handler.inl:983` -- TODO: Add announce usage for stem transaction relay
- `src/cryptonote_protocol/cryptonote_protocol_handler.inl:990` -- TODO: Add announce usage for fluff transaction relay
- `src/cryptonote_protocol/cryptonote_protocol_handler.inl:1030` -- XXX: Commented out `handler_response_blocks_now` call in `handle_request_get_objects`
- `src/cryptonote_protocol/cryptonote_protocol_handler.inl:1966` -- TODO: Investigate tallying sync connections by zone
- `src/cryptonote_protocol/cryptonote_protocol_handler.inl:2782` -- TODO: Investigate tallying sync connections by zone (duplicate)
- `src/cryptonote_protocol/cryptonote_protocol_handler.h:233` -- XXX: Commented out `handler_response_blocks_now` call in `post_notify`
- `src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:82` -- XXX: Hardcoded minimum block size of 500
- `src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:108` -- XXX: Unmarked placeholder
- `src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:125` -- XXX: Commented out delay reset
- `src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:129` -- XXX: Debug sleep log message; TODO: randomize sleeps
- `src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:134` -- XXX LATER XXX: Placeholder for future work
