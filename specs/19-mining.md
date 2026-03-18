# Mining

## Overview

The mining subsystem provides solo mining capabilities integrated directly into the Monero daemon. It is built around the `cryptonote::miner` class, which manages one or more worker threads that iterate over nonce values searching for a block hash that meets the current network difficulty target. The miner obtains block templates from the blockchain module, supports foreground and background (idle-aware) mining modes, and can auto-detect an optimal thread count. External mining software (pools and standalone miners) interacts with the daemon through a set of JSON-RPC endpoints for retrieving block templates, submitting solved blocks, and querying mining status. The proof-of-work function dispatches to RandomX (for hard fork version 12+) or legacy CryptoNight variants (for earlier versions).

## Key Files

| File | Description |
|------|-------------|
| `src/cryptonote_basic/miner.h` | Declaration of the `miner` class, `i_miner_handler` interface, and background mining constants. |
| `src/cryptonote_basic/miner.cpp` | Implementation of the miner lifecycle, worker thread loop, background mining controller, nonce search, hashrate tracking, and platform-specific idle/battery detection. |
| `src/cryptonote_core/cryptonote_core.cpp` | `core::get_block_template()` (delegates to blockchain), `core::handle_block_found()` (adds mined block to chain and relays it), `core::update_miner_block_template()`, `core::pause_mine()`, `core::resume_mine()`. |
| `src/cryptonote_core/blockchain.cpp` | `Blockchain::create_block_template()` -- constructs a full block template with coinbase transaction, selected mempool transactions, difficulty, and RandomX seed information. Caches templates when possible. |
| `src/cryptonote_core/cryptonote_tx_utils.cpp` | `construct_miner_tx()` -- builds the coinbase (miner) transaction. `get_block_longhash()` -- wrapper that dispatches to the appropriate PoW hash function. |
| `src/rpc/core_rpc_server.cpp` | RPC handlers: `on_start_mining`, `on_stop_mining`, `on_mining_status`, `on_getblocktemplate`, `on_submitblock`, `on_getminerdata`, `on_calcpow`, `on_add_aux_pow`, `on_generateblocks`. |
| `src/rpc/core_rpc_server_commands_defs.h` | Request/response structures for all mining-related RPC commands. |
| `src/cryptonote_basic/cryptonote_format_utils.cpp` | `get_block_longhash()` -- core PoW dispatch: RandomX via `crypto::rx_slow_hash()` or CryptoNight via `crypto::cn_slow_hash()`. |
| `src/crypto/rx-slow-hash.c` | RandomX implementation: `rx_slow_hash()`, `rx_set_miner_thread()`, dataset management. |
| `src/cryptonote_basic/difficulty.cpp` | `check_hash()` -- compares a PoW hash against a difficulty target. |
| `src/cryptonote_config.h` | `MINER_CONFIG_FILE_NAME` ("miner_conf.json"), `THREAD_STACK_SIZE` (5 MB). |
| `src/crypto/hash-ops.h` | `RX_BLOCK_VERSION` (12) -- the hard fork major version at which RandomX activates. |

## Data Structures

### `i_miner_handler` (interface)

Defined in `miner.h`. An abstract interface that the miner uses to communicate with the core:

```
virtual bool handle_block_found(block& b, block_verification_context &bvc) = 0;
virtual bool get_block_template(block& b, const account_public_address& adr,
    difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward,
    uint64_t &cumulative_weight, const blobdata& ex_nonce,
    uint64_t &seed_height, crypto::hash &seed_hash) = 0;
```

`cryptonote::core` implements this interface.

### `get_block_hash_t` (function type)

```cpp
typedef std::function<bool(const cryptonote::block&, uint64_t, const crypto::hash*,
    unsigned int, crypto::hash&)> get_block_hash_t;
```

A callable that computes the PoW hash for a given block at a given height. Injected into the miner at construction. The core instantiates it as a lambda that calls `cryptonote::get_block_longhash()`.

### `miner` class

The central mining class. Key member fields:

| Field | Type | Description |
|-------|------|-------------|
| `m_stop` | `atomic<bool>` | Stop signal; when true, worker threads exit their loops. |
| `m_template` | `block` | Current block template being mined. Protected by `m_template_lock`. |
| `m_template_no` | `atomic<uint32_t>` | Monotonically incremented counter; worker threads detect new templates by comparing their local copy against this value. |
| `m_starter_nonce` | `atomic<uint32_t>` | Random starting nonce assigned when a new template is set. |
| `m_diffic` | `difficulty_type` | Difficulty target for the current template. |
| `m_height` | `uint64_t` | Block height of the current template. |
| `m_threads_total` | `volatile uint32_t` | Total number of mining threads. |
| `m_threads_active` | `atomic<uint32_t>` | Number of threads currently in the mining loop. |
| `m_pausers_count` | `atomic<int32_t>` | Pause reference count; mining threads sleep while this is positive. |
| `m_threads` | `list<boost::thread>` | Collection of active worker threads. |
| `m_phandler` | `i_miner_handler*` | Pointer to the core (for block template requests and block submission). |
| `m_gbh` | `get_block_hash_t` | PoW hash computation callback. |
| `m_mine_address` | `account_public_address` | Address receiving coinbase rewards. |
| `m_extra_messages` | `vector<blobdata>` | Optional extra messages loaded from file, embedded in coinbase transactions. |
| `m_config` | `miner_config` | Persisted configuration tracking `current_extra_message_index`. |
| `m_hashes` | `atomic<uint64_t>` | Hash count since last merge (for rate calculation). |
| `m_total_hashes` | `atomic<uint64_t>` | Cumulative hash count across the miner's lifetime. |
| `m_current_hash_rate` | `atomic<uint64_t>` | Most recently computed hash rate (hashes/sec). |
| `m_last_hash_rates` | `list<uint64_t>` | Rolling window of recent hash rates (up to 19 entries) for averaging. |
| `m_threads_autodetect` | `vector<pair<uint64_t,uint64_t>>` | Per-thread-count timing data used during auto-detection. |
| `m_block_reward` | `atomic<uint64_t>` | Expected reward for the current block template. |
| `m_is_background_mining_enabled` | `atomic<bool>` | Whether background (smart) mining mode is active. |
| `m_is_background_mining_started` | `atomic<bool>` | Whether the background controller has signaled workers to begin hashing. |
| `m_miner_extra_sleep` | `atomic<uint64_t>` | Adaptive sleep duration (ms) per hash in background mode. |
| `m_min_idle_seconds` | `uint64_t` | Lookback interval for idle detection. |
| `m_idle_threshold` | `uint8_t` | Minimum CPU idle percentage to trigger background mining. |
| `m_mining_target` | `uint8_t` | Maximum CPU usage target for miner threads in background mode. |

### `miner_config` (nested struct)

A simple serializable struct containing a single field `current_extra_message_index` (`uint64_t`). Persisted to `miner_conf.json` on disk to track position within the extra messages file across restarts.

### Block template cache fields (in `Blockchain`)

The blockchain caches the most recent block template to avoid redundant construction. The cache is invalidated when the mining address, extra nonce, mempool cookie, or chain tip changes. Cache fields include `m_btc` (the cached block), `m_btc_valid`, `m_btc_address`, `m_btc_nonce`, `m_btc_pool_cookie`, `m_btc_difficulty`, `m_btc_height`, `m_btc_expected_reward`, `m_btc_seed_height`, `m_btc_seed_hash`, and `m_btc_cumulative_weight`.

## Public API

### `miner` class methods

#### Construction and initialization

- **`miner(i_miner_handler* phandler, const get_block_hash_t& gbh)`** -- Constructor. Stores the handler and hash callback. Sets all atomic counters to initial states. Sets thread stack size to `THREAD_STACK_SIZE` (5 MB).

- **`~miner()`** -- Destructor. Calls `stop()` in a try/catch block.

- **`bool init(const boost::program_options::variables_map& vm, network_type nettype)`** -- Reads command-line arguments: `--start-mining` (wallet address), `--mining-threads`, `--extra-messages-file`, and all background mining parameters. If `--start-mining` is specified, sets `m_do_mining = true` so that mining will begin after synchronization.

- **`static void init_options(boost::program_options::options_description& desc)`** -- Registers all mining-related command-line options with the program options framework.

#### Lifecycle

- **`bool start(const account_public_address& adr, size_t threads_count, bool do_background = false, bool ignore_battery = false)`** -- Starts mining to the given address. Requests an initial block template, spawns `threads_count` worker threads (or 1 thread in auto-detect mode if `threads_count == 0`), and optionally starts the background mining controller thread. Returns false if already mining.

- **`bool stop()`** -- Signals all worker threads to stop, waits for them to finish, interrupts and joins the background mining thread, and clears the thread list.

- **`void send_stop_signal()`** -- Sets `m_stop = true` without waiting for threads to exit.

- **`void on_synchronized()`** -- Called when the daemon finishes initial blockchain synchronization. If `m_do_mining` was set (via `--start-mining`), starts mining automatically.

- **`void pause()`** -- Increments `m_pausers_count`. Worker threads enter a sleep loop while this counter is positive. Used by `core::handle_block_found()` to safely add a block.

- **`void resume()`** -- Decrements `m_pausers_count`. Logs an error if called more times than `pause()`.

#### Block template management

- **`bool set_block_template(const block& bl, const difficulty_type& diffic, uint64_t height, uint64_t block_reward)`** -- Under lock, replaces the current template, difficulty, height, and reward. Increments `m_template_no` and assigns a new random `m_starter_nonce`.

- **`bool on_block_chain_update()`** -- If mining is active, calls `request_block_template()` to refresh the template after a chain update.

- **`bool on_idle()`** -- Periodic callback. Refreshes block template every 5 seconds, merges hashrate data every 2 seconds, and runs autodetection every 1 second.

#### Status queries

- **`bool is_mining() const`** -- Returns `!m_stop`.
- **`uint64_t get_speed() const`** -- Returns current hash rate if mining, 0 otherwise.
- **`uint32_t get_threads_count() const`** -- Returns `m_threads_total`.
- **`const account_public_address& get_mining_address() const`** -- Returns the address receiving mining rewards.
- **`uint64_t get_block_reward() const`** -- Returns expected reward for current template.
- **`void do_print_hashrate(bool do_hr)`** -- Enables/disables periodic console hashrate printing.
- **`bool get_is_background_mining_enabled() const`** -- Returns background mining enabled state.
- **`bool get_ignore_battery() const`** -- Returns battery ignore setting.
- **`uint64_t get_min_idle_seconds() const`** -- Returns idle lookback interval.
- **`uint8_t get_idle_threshold() const`** -- Returns idle percentage threshold.
- **`uint8_t get_mining_target() const`** -- Returns target CPU usage percentage.

#### Background mining setters

- **`bool set_min_idle_seconds(uint64_t min_idle_seconds)`** -- Sets idle lookback interval. Range: 10--3600 seconds.
- **`bool set_idle_threshold(uint8_t idle_threshold)`** -- Sets idle threshold. Range: 0--99%.
- **`bool set_mining_target(uint8_t mining_target)`** -- Sets CPU usage target. Range: 1--100%.

#### Static utility

- **`static bool find_nonce_for_given_block(const get_block_hash_t &gbh, block& bl, const difficulty_type& diffic, uint64_t height, const crypto::hash *seed_hash = NULL)`** -- Synchronous nonce search. Iterates `bl.nonce` from its current value to `UINT32_MAX`, computing and checking the PoW hash for each. Used by the `generateblocks` RPC for regtest block generation. For difficulties <= 100 (test scenarios), hashes single-threaded; otherwise uses `get_max_concurrency()` threads for dataset initialization.

### `core` mining-related methods

- **`bool core::get_block_template(block& b, const account_public_address& adr, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, uint64_t& cumulative_weight, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)`** -- Delegates to `Blockchain::create_block_template()`. Implements `i_miner_handler`.

- **`bool core::get_block_template(block& b, const crypto::hash *prev_block, ...)`** -- Overload that supports building a template on top of a specified previous block (for alternative chain mining).

- **`bool core::handle_block_found(block& b, block_verification_context &bvc)`** -- Called when the miner finds a valid nonce. Pauses the miner, gathers the block's transactions from the mempool, calls `prepare_handle_incoming_blocks()`, adds the block via `m_blockchain_storage.add_new_block()`, updates the miner template, resumes the miner, and if the block was added to the main chain, relays it to peers as a fluffy block (header only, no transaction bodies).

- **`bool core::get_miner_data(...)`** -- Provides raw data for external block template construction: major version, height, previous block ID, seed hash, difficulty, median weight, already generated coins, and transaction backlog.

- **`void core::pause_mine()` / `void core::resume_mine()`** -- Forward to `m_miner.pause()` / `m_miner.resume()`.

- **`bool core::update_miner_block_template()`** -- Calls `m_miner.on_block_chain_update()`.

### Mining RPC endpoints

All defined in `core_rpc_server.cpp`.

#### `start_mining` (non-restricted)

- **Request**: `miner_address` (string), `threads_count` (uint64), `do_background_mining` (bool), `ignore_battery` (bool).
- **Behavior**: Parses the address (rejects subaddresses), validates thread count against `hardware_concurrency * 4`, calls `miner::start()`.
- **Response**: Status string.

#### `stop_mining` (non-restricted)

- **Request**: (empty).
- **Behavior**: Calls `miner::stop()`.
- **Response**: Status string.

#### `mining_status` (non-restricted)

- **Request**: (empty).
- **Response**: `active` (bool), `speed` (uint64, hashes/sec), `threads_count`, `address`, `pow_algorithm` (string, e.g., "RandomX"), `is_background_mining_enabled`, `bg_idle_threshold`, `bg_min_idle_seconds`, `bg_ignore_battery`, `bg_target`, `block_target` (seconds per block), `block_reward`, `difficulty`.
- **Behavior**: The `pow_algorithm` field is derived from the current hard fork version: version 13+ maps to "RandomX", versions 10-11 to "CNv4", versions 8-9 to "CNv2", version 7 to "CNv1", and earlier to "Cryptonight".

#### `getblocktemplate` (JSON-RPC)

- **Request**: `reserve_size` (max 255), `wallet_address`, `prev_block` (optional, for alt-chain mining), `extra_nonce` (optional, hex, max 510 chars). `reserve_size` and `extra_nonce` are mutually exclusive.
- **Behavior**: Calls `core::get_block_template()`, serializes the block to blob, extracts the `reserved_offset` (position after the tx pubkey + 2 bytes for the nonce tag).
- **Response**: `difficulty`, `height`, `reserved_offset`, `expected_reward`, `cumulative_weight`, `prev_hash`, `seed_height`, `seed_hash`, `next_seed_hash`, `blocktemplate_blob` (hex), `blockhashing_blob` (hex).

#### `submitblock` (JSON-RPC)

- **Request**: A vector with a single element: the hex-encoded block blob.
- **Behavior**: Parses and validates the blob, checks block size, calls `core::handle_block_found()`.
- **Response**: `block_id` (hex hash of accepted block).
- Not supported on bootstrap daemons.

#### `getminerdata` (JSON-RPC)

- **Request**: (empty).
- **Response**: `major_version`, `height`, `prev_id`, `seed_hash`, `difficulty` (hex), `median_weight`, `already_generated_coins`, `tx_backlog` (array of `{id, weight, fee}`).
- Provides all data needed for external software to construct its own block template.

#### `calc_pow` (JSON-RPC)

- **Request**: `major_version`, `height`, `block_blob` (hex), `seed_hash` (hex).
- **Behavior**: Computes and returns the PoW hash for the given block blob without submitting it.
- **Response**: The PoW hash as a hex string.

#### `add_aux_pow` (JSON-RPC)

- **Request**: `blocktemplate_blob` (hex), `aux_pow` (array of `{id, hash}`).
- **Behavior**: For merge-mining. Computes a Merkle root from auxiliary PoW hashes, finds a nonce that avoids slot collisions (up to 65535 attempts), and patches the block template's extra field with the Merkle tree root and path.
- **Response**: Updated `blocktemplate_blob`, `blockhashing_blob`, `merkle_root`, `merkle_tree_depth`, `aux_pow`.

#### `generateblocks` (JSON-RPC, regtest only)

- **Request**: `amount_of_blocks`, `wallet_address`, `prev_block` (optional), `starting_nonce`.
- **Behavior**: Calls `getblocktemplate` + `find_nonce_for_given_block` + `submitblock` in a loop. Only available when `nettype == FAKECHAIN`.
- **Response**: `height`, `blocks` (array of block hashes).

## Internal Logic

### Mining loop (`worker_thread`)

Each worker thread follows this loop:

1. **Thread initialization**: Assigns itself a unique `th_local_index` via atomic increment of `m_thread_index`. Calls `slow_hash_allocate_state()` to allocate per-thread CryptoNight/RandomX state. Increments `m_threads_active`. Sets initial nonce to `m_starter_nonce + th_local_index`.

2. **Pause check**: If `m_pausers_count > 0`, sleeps for 100ms and continues. This is the "anti split workaround" that prevents mining during critical operations like adding a found block.

3. **Background mining wait**: If background mining is enabled, sleeps for `m_miner_extra_sleep` milliseconds. If background mining has not been started by the controller (CPU not idle enough), waits on `m_is_background_mining_started_cond`.

4. **Template update**: If `local_template_ver != m_template_no`, copies the current template, difficulty, and height under lock. Resets nonce to `m_starter_nonce + th_local_index`.

5. **RandomX setup**: On first hash when `b.major_version >= RX_BLOCK_VERSION` (12), calls `crypto::rx_set_miner_thread(th_local_index, max_concurrency)` to register the thread for RandomX dataset access.

6. **Hash and check**: Sets `b.nonce = nonce`, calls `m_gbh(b, height, NULL, max_concurrency, h)` to compute the PoW hash, then calls `check_hash(h, local_diff)`.

7. **Block found**: If the hash meets difficulty, calls `m_phandler->handle_block_found(b, bvc)`. On success, increments `current_extra_message_index` and persists the miner config to `miner_conf.json`.

8. **Nonce advance**: Increments nonce by `m_threads_total` (stride pattern ensures no overlap between threads). Increments `m_hashes` and `m_total_hashes`.

9. **Cleanup**: On exit, calls `slow_hash_free_state()` and decrements `m_threads_active`.

### Nonce distribution

Each worker thread `i` hashes nonces: `starter_nonce + i`, `starter_nonce + i + N`, `starter_nonce + i + 2N`, etc., where `N = m_threads_total`. The `starter_nonce` is randomized (`crypto::rand<uint32_t>()`) each time a new block template is set, avoiding overlap with other miners.

### Block template construction (`Blockchain::create_block_template`)

1. **Locking**: Acquires the transaction pool lock and `m_blockchain_lock`.

2. **Cache check**: If no `from_block` is specified, checks whether the cached template (`m_btc_valid`) matches the current address, extra nonce, mempool state (via pool cookie), and chain tip. Returns the cached template if valid.

3. **Height and chain state**: For main-chain templates: uses `m_db->height()`, the current hard fork version, and the chain tail ID. For alternative chain templates (when `from_block` is provided): walks the alt chain to determine height, difficulty, and seed hash.

4. **Seed hash**: For RandomX blocks (major version >= 12), calculates `seed_height` via `crypto::rx_seedheights(height, &seed_height, &next_height)` and retrieves the corresponding block hash.

5. **Transaction selection**: Calls `m_tx_pool.fill_block_template()` which selects transactions from the mempool up to the median weight limit, maximizing fee revenue.

6. **Coinbase construction (two-phase)**:
   - First, constructs a miner transaction with estimated parameters to approximate the final weight.
   - Then iterates (up to 10 times) to converge on a coinbase transaction whose weight, combined with the selected transactions, produces a consistent total block weight and correct reward. If the coinbase is smaller than expected, zero-byte padding is appended to `miner_tx.extra` to match, with special handling for varint encoding edge cases.

7. **Caching**: If building on the main chain, caches the resulting template for future reuse.

### RandomX integration point

The PoW dispatch chain works as follows:

1. **`miner::worker_thread()`** calls `m_gbh(b, height, NULL, threads, h)`.
2. `m_gbh` is a lambda captured in the core constructor: `cryptonote::get_block_longhash(&m_blockchain_storage, b, hash, height, seed_hash, threads)`.
3. `get_block_longhash()` in `cryptonote_tx_utils.cpp` serializes the block to a hashing blob and delegates to the overload in `cryptonote_format_utils.cpp`.
4. `get_block_longhash()` in `cryptonote_format_utils.cpp` dispatches based on `major_version`:
   - **`>= RX_BLOCK_VERSION` (12)**: Calls `crypto::rx_slow_hash(seed_hash.data, blob.data(), blob.size(), res.data)`.
   - **Earlier versions**: Calls `crypto::cn_slow_hash()` with a variant derived from the major version.
   - **Height 202612**: Returns a hardcoded hash (bug workaround).

Before hashing, each miner thread calls `crypto::rx_set_miner_thread(th_local_index, max_concurrency)` to register itself for RandomX dataset access. The RandomX dataset is initialized from the seed hash (which is the block hash at `seed_height = (height - 1) & ~2047` for heights >= 2048, i.e., the hash changes every 2048 blocks).

### Extra nonce handling

The block template supports two mechanisms for embedding extra data:

1. **`reserve_size` (RPC)**: The caller requests N bytes of zeroed space in the coinbase extra field. The RPC returns a `reserved_offset` indicating where in the serialized block blob the reserved space begins (calculated as: position of the tx pubkey + 33 bytes for the pubkey + 2 bytes for the TX_EXTRA_NONCE tag and length). External miners write their own nonce data at this offset.

2. **`extra_nonce` (RPC)**: An alternative where the caller provides the exact hex-encoded extra nonce data at template creation time.

3. **`m_extra_messages` (internal miner)**: The built-in miner can load a file of base64-encoded messages. The `current_extra_message_index` selects which message to embed in the coinbase transaction's extra field. This index is persisted across daemon restarts.

### Hashrate calculation

The `merge_hr()` method, called every 2 seconds via `on_idle()`:
- Computes current rate as `m_hashes * 1000 / elapsed_ms`.
- Appends to `m_last_hash_rates` (rolling window of up to 19 samples).
- If `m_do_print_hashrate` is set, prints the average of the window to stdout.
- Resets `m_hashes` to 0 for the next interval.

### Thread auto-detection

When `start()` is called with `threads_count == 0`:
1. Starts with 1 thread and records `(timestamp, total_hashes)`.
2. Every `AUTODETECT_WINDOW` (10) seconds, the `update_autodetection()` method computes hash rate for the current thread count.
3. If the hash rate improvement over the previous count is less than `AUTODETECT_GAIN_THRESHOLD` (2%), stops and uses `N-1` threads as optimal.
4. Otherwise, adds one more thread and repeats.
5. Each adjustment stops all worker threads and restarts with the new count.

### Background mining controller (`background_worker_thread`)

The background mining controller runs as a separate thread when background mining is enabled:

1. **When not mining**: Sleeps for `min_idle_seconds`, then samples system CPU idle time. If idle percentage >= `idle_threshold` and on AC power, sets `m_is_background_mining_started = true` and notifies worker threads.

2. **When mining**: Sleeps for `BACKGROUND_MINING_MINER_MONITOR_INVERVAL_IN_SECONDS` (10 seconds), then:
   - Computes system idle percentage and miner process CPU percentage.
   - If `idle_percentage + process_percentage < idle_threshold` or not on AC power, stops background mining.
   - Otherwise, adjusts `m_miner_extra_sleep` to steer the miner's CPU usage toward `m_mining_target`. The adjustment is `-(target - actual)`, with a floor of 5ms.

3. **Battery detection**: Platform-specific (`on_battery_power()`):
   - **Windows**: `GetSystemPowerStatus()`.
   - **macOS**: `IOPSGetTimeRemainingEstimate()`.
   - **Linux**: Reads `/sys/class/power_supply/*/type` and `/sys/class/power_supply/*/status`.
   - **FreeBSD**: `sysctlbyname("hw.acpi.acline")` or `/dev/apm`.
   - Returns `tribool` (true/false/indeterminate). If indeterminate and `ignore_battery` is false, mining may not start.

4. **System time sampling**: Platform-specific (`get_system_times()`):
   - **Windows**: `GetSystemTimes()`.
   - **Linux**: Reads `/proc/stat` (user, nice, system, idle).
   - **macOS**: `host_statistics()` with `HOST_CPU_LOAD_INFO`.
   - **FreeBSD**: `sysctlbyname("kern.cp_time")`.

## Dependencies

### What this module depends on

| Dependency | Usage |
|------------|-------|
| `cryptonote_core/blockchain` | `create_block_template()`, difficulty queries, block addition. |
| `cryptonote_core/tx_memory_pool` | `fill_block_template()` for transaction selection; pool cookie for cache invalidation. |
| `cryptonote_core/cryptonote_tx_utils` | `construct_miner_tx()` for coinbase generation; `get_block_longhash()` for PoW computation. |
| `crypto/rx-slow-hash.c` | RandomX hashing (`rx_slow_hash`, `rx_set_miner_thread`). |
| `crypto/slow-hash.c` | CryptoNight hashing (`cn_slow_hash`, `slow_hash_allocate_state`, `slow_hash_free_state`). |
| `cryptonote_basic/difficulty` | `check_hash()` to verify PoW meets target. |
| `cryptonote_basic/cryptonote_format_utils` | Block serialization, hashing blob extraction, PoW dispatch. |
| `epee` (utility library) | Critical sections, timer helpers (`once_a_time_seconds`), file I/O, serialization, HTTP/RPC framework. |
| `boost` | Threads, filesystem, program_options, tribool, chrono. |

### What depends on this module

| Dependent | Usage |
|-----------|-------|
| `cryptonote_core::core` | Owns the `miner` instance. Calls `init()`, `on_synchronized()`, `on_idle()`, `on_block_chain_update()`, `pause()`, `resume()`. |
| `rpc::core_rpc_server` | Calls `miner::start()`, `stop()`, `is_mining()`, `get_speed()`, etc. through the core's `get_miner()` accessor. |
| `cryptonote_protocol` | Triggers `core::on_synchronized()` which starts the miner if configured. Chain updates trigger `on_block_chain_update()`. |

## Configuration

### Command-line options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `--start-mining` | string | (none) | Wallet address to mine to. Must be a standard (non-subaddress) address. Enables auto-start on synchronization. |
| `--mining-threads` | uint32 | 0 | Number of mining threads. 0 = auto-detect optimal count. |
| `--extra-messages-file` | string | (none) | Path to a file containing base64-encoded extra messages for coinbase transactions. |
| `--bg-mining-enable` | bool | true | Enable background (smart) mining that runs only when CPU is idle. |
| `--bg-mining-ignore-battery` | bool | false | If true, mine even when unable to determine AC power status. |
| `--bg-mining-min-idle-interval` | uint64 | 10 | Lookback interval in seconds for determining idle state (range: 10--3600). |
| `--bg-mining-idle-threshold` | uint16 | 90 | Minimum average idle percentage over the lookback interval to start mining (range: 0--99). |
| `--bg-mining-miner-target` | uint16 | 40 | Maximum CPU percentage the miner should consume in background mode (range: 1--100). |

### Background mining constants

| Constant | Value | Description |
|----------|-------|-------------|
| `BACKGROUND_MINING_DEFAULT_IDLE_THRESHOLD_PERCENTAGE` | 90 | Default idle threshold (%). |
| `BACKGROUND_MINING_MIN_IDLE_THRESHOLD_PERCENTAGE` | 0 | Minimum allowed idle threshold. |
| `BACKGROUND_MINING_MAX_IDLE_THRESHOLD_PERCENTAGE` | 99 | Maximum allowed idle threshold. |
| `BACKGROUND_MINING_DEFAULT_MIN_IDLE_INTERVAL_IN_SECONDS` | 10 | Default lookback interval. |
| `BACKGROUND_MINING_MIN_MIN_IDLE_INTERVAL_IN_SECONDS` | 10 | Minimum lookback interval. |
| `BACKGROUND_MINING_MAX_MIN_IDLE_INTERVAL_IN_SECONDS` | 3600 | Maximum lookback interval. |
| `BACKGROUND_MINING_DEFAULT_MINING_TARGET_PERCENTAGE` | 40 | Default CPU target (%). |
| `BACKGROUND_MINING_MIN_MINING_TARGET_PERCENTAGE` | 1 | Minimum CPU target. |
| `BACKGROUND_MINING_MAX_MINING_TARGET_PERCENTAGE` | 100 | Maximum CPU target. |
| `BACKGROUND_MINING_MINER_MONITOR_INVERVAL_IN_SECONDS` | 10 | How often the background controller checks system state when mining. |
| `BACKGROUND_MINING_DEFAULT_MINER_EXTRA_SLEEP_MILLIS` | 400 | Initial per-hash sleep for ramp-up. |

### Other constants

| Constant | Value | Description |
|----------|-------|-------------|
| `AUTODETECT_WINDOW` | 10 (seconds) | Duration to measure hash rate at each thread count during auto-detection. |
| `AUTODETECT_GAIN_THRESHOLD` | 1.02 (2%) | Minimum hash rate improvement to justify adding another thread. |
| `RX_BLOCK_VERSION` | 12 | Hard fork major version at which RandomX replaces CryptoNight. |
| `MINER_CONFIG_FILE_NAME` | "miner_conf.json" | Filename for persisted miner configuration. |
| `THREAD_STACK_SIZE` | 5 * 1024 * 1024 (5 MB) | Stack size for mining threads. |

## Known Issues

| Location | Type | Description |
|----------|------|-------------|
| `src/cryptonote_basic/miner.cpp:627` | Note | Comment says "Note: add documentation" on `set_is_background_mining_enabled()`. |
| `src/cryptonote_basic/miner.cpp:632-634` | Comment | Commented-out `notify_one()` call with note: "Extra logic will be required if we make this function public in the future and allow toggling smart mining without start/stop." |
| `src/cryptonote_basic/miner.cpp:166` | Comment | `expected_reward` variable comment: "only used for RPC calls - could possibly be useful here too?" |
| `src/cryptonote_core/blockchain.cpp:1523-1526` | TODO | "This function only needed minor modification to work with BlockchainDB, and *works*. As such, to reduce the number of things that might break in moving to BlockchainDB, this function will remain otherwise unchanged for the time being." |
| `src/cryptonote_core/blockchain.cpp:1530-1533` | FIXME | "this codebase references `#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)` in a lot of places. That flag is not referenced in any of the code nor any of the makefiles, however. Need to look into whether or not it's necessary at all." |
| `src/cryptonote_core/blockchain.cpp:1594` | TODO | Comment on `get_block_by_hash` call: "TODO" (no further detail). |
| `src/cryptonote_core/blockchain.cpp:1643` | FIXME | "consider moving away from block_extended_info at some point." |
| `src/cryptonote_core/blockchain.cpp:1782` | Comment | Profanity-laced comment about varint edge case: notes that -1 delta can cause varint counter size to shrink, requiring iteration to converge. |
