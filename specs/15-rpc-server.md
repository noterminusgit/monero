# Core RPC Server

## Overview

The Core RPC Server (`core_rpc_server`) is the primary HTTP-based interface through which external clients (wallets, mining software, monitoring tools, other daemons) interact with a running Monero daemon. It exposes blockchain queries, transaction submission, mining operations, network peer management, and administrative controls over both a direct REST-like URI API and a JSON-RPC 2.0 endpoint at `/json_rpc`. The server supports restricted mode (limiting dangerous or privacy-sensitive operations), a bootstrap daemon fallback for nodes still syncing, optional RPC payment via proof-of-work micropayments, TLS/SSL transport, HTTP digest authentication, and per-IP connection limiting. The RPC protocol version is 3.16 (`CORE_RPC_VERSION_MAJOR=3`, `CORE_RPC_VERSION_MINOR=16`).

## Key Files

| File | Lines | Description |
|------|-------|-------------|
| `src/rpc/core_rpc_server.h` | 311 | Class declaration, URI/JSON-RPC route maps, handler signatures, member variables |
| `src/rpc/core_rpc_server.cpp` | 3964 | All handler implementations, init/lifecycle, bootstrap daemon logic, arg descriptors |
| `src/rpc/core_rpc_server_commands_defs.h` | 2794 | All request/response struct definitions for every RPC command |
| `src/rpc/core_rpc_server_error_codes.h` | 84 | Numeric error codes (-1 through -21) with human-readable messages |
| `src/rpc/rpc_handler.h` | 67 | Abstract `RpcHandler` base class; `output_distribution_data` struct |
| `src/rpc/rpc_args.h` | 95 | Common RPC argument processing (bind IP, SSL, login, CORS) |
| `src/rpc/rpc_payment.h` | 191 | RPC payment system: per-client credits, nonce tracking, persistence |
| `src/rpc/rpc_payment_costs.h` | 50 | Cost constants for each RPC endpoint when payment is enabled |
| `src/rpc/bootstrap_daemon.h` | 90 | Bootstrap daemon proxy: HTTP JSON/BIN/JSON-RPC forwarding, auto-switching |
| `src/rpc/rpc_payment_signature.h` | (support) | Payment signature verification utilities |

## Data Structures

### Base Types

All request/response pairs inherit from a hierarchy of base structs defined in `core_rpc_server_commands_defs.h`:

**`rpc_request_base`** -- Empty base for requests (provides the serialization map scaffold).

**`rpc_response_base`** -- Base for responses:
- `status` (string): `"OK"`, `"BUSY"`, `"NOT MINING"`, `"PAYMENT REQUIRED"`, or `"<error>"`
- `untrusted` (bool): true when response came from a bootstrap daemon rather than the local node

**`rpc_access_request_base`** (extends `rpc_request_base`):
- `client` (string): Client public key signature for RPC payment system

**`rpc_access_response_base`** (extends `rpc_response_base`):
- `credits` (uint64_t): Remaining payment credits
- `top_hash` (string): Current top block hash at time of payment check

### Key Shared Structs

**`block_header_response`** -- Standardized block header used across multiple endpoints:
- `major_version`, `minor_version`, `timestamp`, `prev_hash`, `nonce`
- `orphan_status`, `height`, `depth`, `hash`
- `difficulty`, `wide_difficulty`, `difficulty_top64` (128-bit difficulty support)
- `cumulative_difficulty`, `wide_cumulative_difficulty`, `cumulative_difficulty_top64`
- `reward`, `block_size`, `block_weight`, `num_txes`, `pow_hash`, `long_term_weight`, `miner_tx_hash`

**`tx_info`** -- Transaction pool entry:
- `id_hash`, `tx_json`, `blob_size`, `weight`, `fee`
- `max_used_block_id_hash`, `max_used_block_height`
- `kept_by_block`, `last_failed_height`, `last_failed_id_hash`
- `receive_time`, `relayed`, `last_relayed_time`, `do_not_relay`, `double_spend_seen`, `tx_blob`

**`txpool_stats`** -- Aggregated transaction pool statistics:
- `bytes_total`, `bytes_min`, `bytes_max`, `bytes_med`
- `fee_total`, `oldest`, `txs_total`
- `num_failing`, `num_10m`, `num_not_relayed`, `num_double_spends`
- `histo` (vector of `txpool_histo` with `txs` and `bytes` per bucket)

**`peer`** -- Peer list entry: `id`, `host`, `ip`, `port`, `rpc_port`, `rpc_credits_per_hash`, `last_seen`, `pruning_seed`

**`public_node`** -- Subset of peer info for public node discovery: `host`, `last_seen`, `rpc_port`, `rpc_credits_per_hash`

**`connection_info`** -- Full connection details (defined in `cryptonote_protocol_defs.h`, used by `get_connections` and `sync_info`)

**`get_outputs_out`** -- Output query specifier: `amount`, `index`

## RPC Endpoints

Endpoints are served on two transports:
1. **Direct URI endpoints** -- HTTP POST to paths like `/get_height`, returning JSON or binary
2. **JSON-RPC endpoints** -- HTTP POST to `/json_rpc` with standard JSON-RPC 2.0 envelope

Many endpoints have both underscore and camelCase aliases (e.g., `/get_height` and `/getheight`).

Endpoints gated with `!m_restricted` are only available in unrestricted mode.

### Blockchain Queries

| Method | Transport | Restricted? | Description |
|--------|-----------|-------------|-------------|
| `get_height` / `getheight` | URI (JSON) | No | Returns current blockchain height and top block hash |
| `get_blocks.bin` / `getblocks.bin` | URI (binary) | No | Fetches blocks (with optional pool info) starting from known block IDs; supports pruned mode |
| `get_blocks_by_height.bin` / `getblocks_by_height.bin` | URI (binary) | No | Fetches complete blocks by specific heights |
| `get_hashes.bin` / `gethashes.bin` | URI (binary) | No | Fetches block hashes starting from known block IDs |
| `get_block_count` / `getblockcount` | JSON-RPC | No | Returns current blockchain height as count |
| `on_get_block_hash` / `on_getblockhash` | JSON-RPC | No | Returns block hash for a given height |
| `get_block` / `getblock` | JSON-RPC | No | Returns full block data (header, miner tx hash, tx hashes, blob, JSON) by height or hash |
| `get_last_block_header` / `getlastblockheader` | JSON-RPC | No | Returns header of the most recent block |
| `get_block_header_by_hash` / `getblockheaderbyhash` | JSON-RPC | No | Returns block header(s) by hash (single or batch) |
| `get_block_header_by_height` / `getblockheaderbyheight` | JSON-RPC | No | Returns block header by height |
| `get_block_headers_range` / `getblockheadersrange` | JSON-RPC | No | Returns block headers for a height range (restricted to 1000 blocks in restricted mode) |
| `get_alt_blocks_hashes` | URI (JSON) | No | Returns hashes of all alternative (orphan) blocks |
| `get_coinbase_tx_sum` | JSON-RPC | **Yes** | Returns sum of coinbase emissions and fees over a block range |
| `get_txids_loose` | JSON-RPC | No | Finds transaction IDs matching a template with specified number of matching bits |

### Transaction Operations

| Method | Transport | Restricted? | Description |
|--------|-----------|-------------|-------------|
| `get_transactions` / `gettransactions` | URI (JSON) | No | Fetches transactions by hash with optional JSON decode and pruning; returns pool/chain status, confirmations (restricted to 100 tx) |
| `send_raw_transaction` / `sendrawtransaction` | URI (JSON) | No | Submits a raw transaction; performs sanity checks; returns detailed failure reasons (double spend, low fee, overspend, etc.) |
| `is_key_image_spent` | URI (JSON) | No | Checks spent status of key images (unspent/in-blockchain/in-pool); restricted to 5000 key images |
| `get_o_indexes.bin` | URI (binary) | No | Returns global output indexes for a transaction |
| `get_outs.bin` | URI (binary) | No | Returns output keys, masks, unlock status, heights, and txids (binary format) |
| `get_outs` | URI (JSON) | No | Same as above but JSON format; restricted to 40 fake outs / 5000 global fake outs |
| `relay_tx` | JSON-RPC | **Yes** | Relays transactions from the pool by txid |
| `flush_txpool` | JSON-RPC | **Yes** | Removes specific or all transactions from the pool |

### Transaction Pool

| Method | Transport | Restricted? | Description |
|--------|-----------|-------------|-------------|
| `get_transaction_pool` | URI (JSON) | No | Returns full transaction pool contents and spent key images |
| `get_transaction_pool_hashes.bin` | URI (JSON) | No | Returns pool transaction hashes as binary blobs |
| `get_transaction_pool_hashes` | URI (JSON) | No | Returns pool transaction hashes as hex strings |
| `get_transaction_pool_stats` | URI (JSON) | No | Returns aggregated pool statistics (sizes, fees, histogram) |
| `get_txpool_backlog` | JSON-RPC | No | Returns pool backlog entries (weight, fee, time_in_pool) |

### Mining

| Method | Transport | Restricted? | Description |
|--------|-----------|-------------|-------------|
| `start_mining` | URI (JSON) | **Yes** | Starts the miner with specified address, thread count, background mining options |
| `stop_mining` | URI (JSON) | **Yes** | Stops the miner |
| `mining_status` | URI (JSON) | **Yes** | Returns mining status (active, speed, threads, address, PoW algorithm, difficulty, reward) |
| `get_block_template` / `getblocktemplate` | JSON-RPC | No | Returns a block template for mining (with reserve space, difficulty, seed hash for RandomX) |
| `get_miner_data` | JSON-RPC | No | Returns data needed for mining (major version, height, prev_id, seed hash, difficulty, median weight, generated coins, tx backlog) |
| `calc_pow` | JSON-RPC | **Yes** | Calculates proof-of-work hash for a given block blob |
| `add_aux_pow` | JSON-RPC | No | Adds auxiliary proof-of-work for merge mining |
| `submit_block` / `submitblock` | JSON-RPC | No | Submits a mined block |
| `generateblocks` | JSON-RPC | **Yes** | Generates blocks on regtest/fakechain (testing only) |

### Network Info

| Method | Transport | Restricted? | Description |
|--------|-----------|-------------|-------------|
| `get_info` / `getinfo` | URI (JSON) + JSON-RPC | No | Comprehensive node status (height, difficulty, connections, pool size, version, sync state); privacy-sensitive fields zeroed in restricted mode |
| `get_connections` | JSON-RPC | **Yes** | Returns list of all P2P connections with detailed info |
| `get_peer_list` | URI (JSON) | **Yes** | Returns white and gray peer lists |
| `get_public_nodes` | URI (JSON) | No | Returns known public nodes with RPC ports |
| `get_net_stats` | URI (JSON) | **Yes** | Returns network I/O statistics (packets/bytes in/out) |
| `get_limit` | URI (JSON) | No | Returns current upload/download bandwidth limits |
| `set_limit` | URI (JSON) | **Yes** | Sets bandwidth limits (kB/s) |
| `out_peers` | URI (JSON) | **Yes** | Gets/sets max outgoing peer count |
| `in_peers` | URI (JSON) | **Yes** | Gets/sets max incoming peer count |
| `sync_info` | JSON-RPC | **Yes** | Returns sync progress, peer list, active download spans |
| `hard_fork_info` | JSON-RPC | No | Returns hard fork voting status (version, enabled, window, votes, threshold, state) |
| `get_version` | JSON-RPC | No | Returns RPC version, release status, current/target heights, hard fork schedule |

### Output Distribution and Histogram

| Method | Transport | Restricted? | Description |
|--------|-----------|-------------|-------------|
| `get_output_histogram` | JSON-RPC | No | Returns output amount histogram (total/unlocked/recent instances); restricted mode limits `recent_cutoff` to 3 days |
| `get_output_distribution` | JSON-RPC + URI (binary) | No | Returns output distribution data for specified amounts; supports binary, compressed, and cumulative modes |

### Admin / Daemon Management

| Method | Transport | Restricted? | Description |
|--------|-----------|-------------|-------------|
| `save_bc` | URI (JSON) | **Yes** | Saves blockchain to disk |
| `stop_daemon` | URI (JSON) | **Yes** | Sends stop signal to the daemon |
| `set_bootstrap_daemon` | URI (JSON) | **Yes** | Configures or changes the bootstrap daemon address/credentials/proxy |
| `set_log_hash_rate` | URI (JSON) | **Yes** | Toggles hash rate logging visibility |
| `set_log_level` | URI (JSON) | **Yes** | Sets the log verbosity level (0-4) |
| `set_log_categories` | URI (JSON) | **Yes** | Sets log categories string |
| `update` | URI (JSON) | **Yes** | Checks for or downloads daemon updates |
| `pop_blocks` | URI (JSON) | **Yes** | Pops N blocks from the blockchain top |
| `prune_blockchain` | JSON-RPC | **Yes** | Prunes or checks pruning status of the blockchain |
| `flush_cache` | JSON-RPC | **Yes** | Flushes internal caches (optionally bad blocks cache) |
| `set_bans` | JSON-RPC | **Yes** | Bans/unbans peers by IP or host |
| `get_bans` | JSON-RPC | **Yes** | Returns list of currently banned peers |
| `banned` | JSON-RPC | **Yes** | Checks if a specific address is banned |
| `get_alternate_chains` | JSON-RPC | **Yes** | Returns info about known alternative chains |
| `get_fee_estimate` | JSON-RPC | No | Returns base fee estimate with quantization mask and per-priority fees |

### RPC Payment System

| Method | Transport | Restricted? | Description |
|--------|-----------|-------------|-------------|
| `rpc_access_info` | JSON-RPC | No | Returns mining blob for earning RPC credits; includes difficulty, credits per hash, seed info |
| `rpc_access_submit_nonce` | JSON-RPC | No | Submits a mined nonce to earn RPC credits |
| `rpc_access_pay` | JSON-RPC | No | Pays credits for a specific RPC call |
| `rpc_access_tracking` | JSON-RPC | **Yes** | Returns/clears per-RPC call tracking statistics (count, time, credits) |
| `rpc_access_data` | JSON-RPC | **Yes** | Returns per-client payment account data (balance, nonce stats) |
| `rpc_access_account` | JSON-RPC | **Yes** | Queries/adjusts a client's credit balance |

## Internal Logic

### Server Lifecycle

1. **Static initialization** (`init_options`): Registers all command-line argument descriptors (bind ports, SSL, bootstrap, payment, connection limits).

2. **Construction**: Takes references to `core` and `node_server<t_cryptonote_protocol_handler<core>>` (the P2P layer). Sets initial state: `m_was_bootstrap_ever_used = false`, `disable_rpc_ban = false`.

3. **Initialization** (`init`):
   - Processes `rpc_args` (bind IPs, SSL, CORS, login credentials).
   - If restricted mode is active and the restricted port is being configured, uses the restricted bind IP addresses.
   - Configures the RPC payment system if a payment address is provided (requires restricted mode for security; cannot use subaddresses; sets up difficulty and credits-per-hash; loads persisted payment state from disk).
   - Configures the bootstrap daemon (direct address, `"auto"` for public node discovery, or none).
   - Sets up HTTP digest authentication if credentials are provided.
   - Configures SSL/TLS with certificate persistence (generates new certs on first run, loads existing on subsequent runs, writes fingerprint file).
   - Validates connection limit settings (per-public-IP, per-private-IP, total max).
   - Initializes the underlying epee HTTP server with random number generator, ports, bind addresses, SSL options, and connection limits.
   - Sets `m_max_content_length` to `MAX_RPC_CONTENT_LENGTH`.

4. **Destruction** (`~core_rpc_server`): Persists RPC payment state to disk.

### Request Routing

The `CHAIN_HTTP_TO_MAP2` macro forwards incoming HTTP requests to a URI map. Two mapping systems coexist:

- **`MAP_URI_AUTO_JON2` / `MAP_URI_AUTO_BIN2`**: Maps URI paths to handler functions with JSON or binary serialization. The `_IF` variants add a boolean condition (e.g., `!m_restricted`) that must be true or the endpoint returns an error.

- **`BEGIN_JSON_RPC_MAP` / `MAP_JON_RPC` / `MAP_JON_RPC_WE`**: Maps JSON-RPC method names to handler functions. `_WE` variants support `epee::json_rpc::error` responses. `_WE_IF` variants add restriction checks.

### Bootstrap Daemon Fallback

When the local node is not fully synced, requests can be proxied to a trusted bootstrap daemon:

- The `use_bootstrap_daemon_if_necessary<COMMAND_TYPE>` template method checks (every 30 seconds) whether the bootstrap daemon should be used based on height comparison (local height + 10 < bootstrap height).
- Supports three invocation modes: `JON` (JSON), `BIN` (binary), `JON_RPC` (JSON-RPC).
- If the bootstrap daemon itself is out of sync or below the latest checkpoint, it is skipped.
- When auto-mode is configured (`"auto"`), the `bootstrap_daemon` class uses a `bootstrap_node::selector` to discover and switch between public nodes.
- Responses from the bootstrap daemon have `untrusted = true`.

### Restricted vs. Unrestricted Mode

The `m_restricted` flag controls access:

- 27 endpoints are gated behind `!m_restricted` (admin/mining/network management).
- Even unrestricted endpoints behave differently in restricted mode: `get_info` hides sensitive fields (start time, alt blocks, connection counts, version), `get_transactions` limits to 100 results, `is_key_image_spent` limits to 5000, `get_block_headers_range` limits to 1000 blocks, output requests are capped at 40 fake outs / 5000 global fake outs, histogram `recent_cutoff` is limited to 3 days.

### RPC Payment System

When `--rpc-payment-address` is configured:

- Clients earn credits by mining (solving RandomX PoW puzzles at the configured difficulty).
- Each RPC call has an associated cost (defined in `rpc_payment_costs.h`, e.g., `COST_PER_GET_INFO = 1`, `COST_PER_TX = 0.5`, `COST_PER_OUTPUT_HISTOGRAM = 25000`).
- The `CHECK_PAYMENT` / `CHECK_PAYMENT_MIN1` macros verify the client signature and deduct credits before processing.
- Loopback addresses can optionally bypass payment (`--rpc-payment-allow-free-loopback`).
- Payment state is periodically persisted to disk and loaded on startup.
- The `RPCTracker` class (anonymous namespace in the cpp) tracks per-RPC call counts, cumulative time, and credits consumed.

### Host Fail Scoring

The `add_host_fail` method tracks misbehaving clients by IP address. When a host's fail score exceeds `RPC_IP_FAILS_BEFORE_BLOCK`, the IP is blocked via the P2P layer. This is disabled when `disable_rpc_ban` is set (via `--disable-rpc-ban`).

### Performance Tracking

Every handler begins with `RPC_TRACKER(handler_name)` which creates an `RPCTracker` RAII object that:
- Starts a `LoggingPerformanceTimer`
- On destruction, records the call count and elapsed time in a static map
- Supports credit tracking via the `pay()` method
- Data is accessible via the `rpc_access_tracking` endpoint

## Dependencies

### What This Module Depends On

| Dependency | Purpose |
|------------|---------|
| `cryptonote_core/cryptonote_core.h` (`core`) | Blockchain queries, transaction pool, miner access, block templates |
| `p2p/net_node.h` (`node_server`) | Peer list, connection management, banning, relay |
| `cryptonote_protocol/cryptonote_protocol_handler.h` | Transaction relay, sync status, block queue |
| `net/http_server_impl_base.h` (epee) | HTTP server framework, connection handling, SSL |
| `rpc/bootstrap_daemon.h` | Bootstrap daemon proxy for syncing nodes |
| `rpc/rpc_payment.h` | RPC payment/credits system |
| `rpc/rpc_args.h` | Common RPC argument processing |
| `rpc/rpc_handler.h` | Output distribution data helper |
| `rpc/rpc_payment_costs.h` | Per-endpoint cost constants |
| `rpc/rpc_payment_signature.h` | Client payment signature verification |
| `common/updates.h` / `common/download.h` | Daemon update checking/downloading |
| `cryptonote_basic/merge_mining.h` | Merge mining (add_aux_pow) support |
| `cryptonote_core/tx_sanity_check.h` | Transaction sanity validation |

### What Depends On This Module

| Dependent | Purpose |
|-----------|---------|
| `daemon/` (daemon main) | Instantiates and runs the RPC server |
| Wallet software (external) | Queries blockchain, submits transactions via HTTP |
| Mining software (external) | Gets block templates, submits blocks via HTTP |
| Monitoring tools (external) | Gets node info, sync status, peer lists |

## Configuration

### Command-Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--rpc-bind-port` | 18081 (mainnet), 28081 (testnet), 38081 (stagenet) | Port for the RPC server |
| `--rpc-restricted-bind-port` | (empty) | Separate port for a restricted RPC server instance |
| `--restricted-rpc` | false | Restrict RPC to view-only commands, hide privacy-sensitive data |
| `--rpc-bind-ip` | (from rpc_args) | IPv4 bind address |
| `--rpc-bind-ipv6-address` | (from rpc_args) | IPv6 bind address |
| `--rpc-restricted-bind-ip` | (from rpc_args) | IPv4 bind address for restricted port |
| `--rpc-use-ipv6` | (from rpc_args) | Enable IPv6 |
| `--rpc-ignore-ipv4` | (from rpc_args) | Do not require IPv4 |
| `--rpc-login` | (none) | `username:password` for HTTP digest authentication |
| `--rpc-access-control-origins` | (none) | CORS allowed origins |
| `--rpc-ssl` | (from rpc_args) | SSL mode (`enabled`, `disabled`, `autodetect`) |
| `--rpc-ssl-private-key` | (auto-generated) | Path to SSL private key |
| `--rpc-ssl-certificate` | (auto-generated) | Path to SSL certificate |
| `--rpc-ssl-ca-certificates` | (none) | Path to CA certificates for client verification |
| `--rpc-ssl-allowed-fingerprints` | (none) | Allowed client certificate fingerprints |
| `--rpc-ssl-allow-any-cert` | false | Accept any client certificate |
| `--confirm-external-bind` | (from rpc_args) | Confirm binding to external (non-loopback) address |
| `--disable-rpc-ban` | false | Disable automatic IP banning for misbehaving RPC clients |
| `--bootstrap-daemon-address` | (empty) | Bootstrap daemon URL; `"auto"` for auto-discovery |
| `--bootstrap-daemon-login` | (empty) | `username:password` for bootstrap daemon authentication |
| `--bootstrap-daemon-proxy` | (empty) | SOCKS proxy (`ip:port`) for bootstrap daemon connections |
| `--rpc-payment-address` | (empty) | Monero address for RPC payment (enables payment system) |
| `--rpc-payment-difficulty` | 1000 | Mining difficulty for RPC payment credits |
| `--rpc-payment-credits` | 100 | Credits awarded per valid hash |
| `--rpc-payment-allow-free-loopback` | false | Allow free RPC access from localhost |
| `--rpc-max-connections-per-public-ip` | `DEFAULT_RPC_MAX_CONNECTIONS_PER_PUBLIC_IP` | Max concurrent RPC connections per public IP |
| `--rpc-max-connections-per-private-ip` | `DEFAULT_RPC_MAX_CONNECTIONS_PER_PRIVATE_IP` | Max concurrent RPC connections per private/localhost IP |
| `--rpc-max-connections` | `DEFAULT_RPC_MAX_CONNECTIONS` | Total max concurrent RPC connections |
| `--rpc-response-soft-limit` | `DEFAULT_RPC_SOFT_LIMIT_SIZE` | Max queued response bytes before enforcement |

### SSL Certificate Management

When `store_ssl_key` is true (unrestricted mode without explicit certificates):
- On first run: new SSL keys are generated and stored to `<data-dir>/rpc_ssl.key`, `rpc_ssl.crt`, and `rpc_ssl.fingerprint`.
- On subsequent runs: existing keys are loaded from disk.
- The `.crt` and `.key` files must both exist or both not exist; inconsistent state causes a fatal error.

### Restricted Mode Limits

| Constant | Value | Purpose |
|----------|-------|---------|
| `MAX_RESTRICTED_FAKE_OUTS_COUNT` | 40 | Max outputs per request in restricted mode |
| `MAX_RESTRICTED_GLOBAL_FAKE_OUTS_COUNT` | 5000 | Max total global fake outs in restricted mode |
| `OUTPUT_HISTOGRAM_RECENT_CUTOFF_RESTRICTION` | 259200 (3 days) | Max `recent_cutoff` in restricted mode |
| `RESTRICTED_BLOCK_HEADER_RANGE` | 1000 | Max block headers range in restricted mode |
| `RESTRICTED_TRANSACTIONS_COUNT` | 100 | Max transactions per request in restricted mode |
| `RESTRICTED_SPENT_KEY_IMAGES_COUNT` | 5000 | Max key images per request in restricted mode |
| `RESTRICTED_BLOCK_COUNT` | 1000 | Max blocks per request in restricted mode |

## Error Codes

Defined in `core_rpc_server_error_codes.h`:

| Code | Name | Description |
|------|------|-------------|
| -1 | `CORE_RPC_ERROR_CODE_WRONG_PARAM` | Invalid parameter |
| -2 | `CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT` | Height is too large |
| -3 | `CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE` | Reserve size is too large |
| -4 | `CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS` | Wrong wallet address |
| -5 | `CORE_RPC_ERROR_CODE_INTERNAL_ERROR` | Internal error |
| -6 | `CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB` | Wrong block blob |
| -7 | `CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED` | Block not accepted |
| -9 | `CORE_RPC_ERROR_CODE_CORE_BUSY` | Core is busy |
| -10 | `CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB_SIZE` | Wrong block blob size |
| -11 | `CORE_RPC_ERROR_CODE_UNSUPPORTED_RPC` | Unsupported RPC |
| -12 | `CORE_RPC_ERROR_CODE_MINING_TO_SUBADDRESS` | Mining to subaddress is not supported |
| -13 | `CORE_RPC_ERROR_CODE_REGTEST_REQUIRED` | Regtest mode required |
| -14 | `CORE_RPC_ERROR_CODE_PAYMENT_REQUIRED` | Payment required |
| -15 | `CORE_RPC_ERROR_CODE_INVALID_CLIENT` | Invalid client |
| -16 | `CORE_RPC_ERROR_CODE_PAYMENT_TOO_LOW` | Payment too low |
| -17 | `CORE_RPC_ERROR_CODE_DUPLICATE_PAYMENT` | Duplicate payment |
| -18 | `CORE_RPC_ERROR_CODE_STALE_PAYMENT` | Stale payment |
| -19 | `CORE_RPC_ERROR_CODE_RESTRICTED` | Parameters beyond restricted allowance |
| -20 | `CORE_RPC_ERROR_CODE_UNSUPPORTED_BOOTSTRAP` | Command is unsupported in bootstrap mode |
| -21 | `CORE_RPC_ERROR_CODE_PAYMENTS_NOT_ENABLED` | Payments not enabled |

Note: Error code -8 is not defined (gap in the sequence).

## Known Issues

| File | Line | Type | Description |
|------|------|------|-------------|
| `core_rpc_server_commands_defs.h` | 1512 | TODO | `tx_info::tx_json`: "TODO - expose this data directly" -- the `tx_json` field is a serialized string rather than structured data |
| `core_rpc_server.cpp` | 1445 | TODO | `on_send_raw_tx`: "TODO: make sure that tx has reached other nodes here, probably wait to receive reflections from other nodes" -- no confirmation that relayed transactions were actually propagated |
| `core_rpc_server.cpp` | 1820 | FIXME | `on_stop_daemon`: "FIXME: replace back to original m_p2p.send_stop_signal() after investigating why that isn't working quite right." -- the comment references the same call that is used, suggesting the original issue may have been resolved but the comment was left behind |
| `core_rpc_server.cpp` | 3342 | TODO | `on_relay_tx`: "TODO: The get_pool_transaction could have an optional meta parameter" -- would allow relay_tx to access transaction metadata more efficiently |
| `core_rpc_server.cpp` | 3352 | TODO | `on_relay_tx`: "TODO: make sure that tx has reached other nodes here, probably wait to receive reflections from other nodes" -- same propagation confirmation concern as in send_raw_tx |
