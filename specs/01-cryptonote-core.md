# Cryptonote Core

## Overview

The `cryptonote::core` class is the central coordinator of the Monero daemon. It ties together the Blockchain (persistent block storage and validation), the transaction memory pool (pending unconfirmed transactions), the miner, the cryptonote P2P protocol layer, and the RPC server. Nearly every daemon operation -- accepting blocks, accepting transactions, serving blockchain data to peers, creating block templates for miners, managing checkpoints, pruning, version-update checks, disk-space monitoring, and block-rate anomaly detection -- is routed through this class. It implements two interfaces: `i_miner_handler` (so the miner can submit found blocks and request templates) and `i_core_events` (so the protocol layer can query height, sync status, and notify about relayed transactions).

## Key Files

| File | Lines | Description |
|------|------:|-------------|
| `src/cryptonote_core/cryptonote_core.h` | 1127 | Class declaration for `core`, command-line arg descriptors, `test_options` struct, `HAVE_BLOCK_*` enum. |
| `src/cryptonote_core/cryptonote_core.cpp` | 1970 | Full implementation of `core`: init/deinit lifecycle, block/tx handling, idle tasks, checkpoints, update checks, block-rate analysis, pruning. |
| `src/cryptonote_core/i_core_events.h` | 46 | Pure-virtual interface `i_core_events` with three methods: `get_current_blockchain_height`, `is_synchronized`, `on_transactions_relayed`. |
| `src/cryptonote_core/blockchain_and_pool.h` | 62 | `BlockchainAndPool` helper struct that safely co-constructs `Blockchain` and `tx_memory_pool` (which have circular reference requirements). |

### Other files in `src/cryptonote_core/` (context, not primary focus)

| File | Description |
|------|-------------|
| `blockchain.h` / `blockchain.cpp` | `Blockchain` class -- block storage, validation, difficulty, hard forks. |
| `tx_pool.h` / `tx_pool.cpp` | `tx_memory_pool` -- pending transaction management. |
| `cryptonote_tx_utils.h` / `.cpp` | Transaction construction utilities (coinbase tx, block template filling). |
| `tx_verification_utils.h` / `.cpp` | `pool_supplement` struct, batch tx verification helpers. |
| `tx_sanity_check.h` / `.cpp` | Sanity checks on transaction output amounts. |
| `CMakeLists.txt` | Build definition; links against `version`, `common`, `cncrypto`, `blockchain_db`, `ringct`, `device`, `hardforks`, and several Boost libraries. |

## Data Structures

### `cryptonote::core` (class, declared `final`)

The primary class. Inherits from `i_miner_handler` and `i_core_events`.

#### Member variables

| Member | Type | Description |
|--------|------|-------------|
| `m_bap` | `BlockchainAndPool` | Owns the `Blockchain` and `tx_memory_pool` instances. |
| `m_mempool` | `tx_memory_pool&` | Reference to `m_bap.tx_pool`. |
| `m_blockchain_storage` | `Blockchain&` | Reference to `m_bap.blockchain`. |
| `m_pprotocol` | `i_cryptonote_protocol*` | Protocol handler for relaying blocks/txs to peers. Falls back to `m_protocol_stub` if null. |
| `m_protocol_stub` | `cryptonote_protocol_stub` | No-op protocol stub used when no real protocol is set. |
| `m_incoming_tx_lock` | `epee::critical_section` | Mutex serializing incoming transaction processing and `prepare/cleanup_handle_incoming_blocks`. |
| `m_miner` | `miner` | Built-in CPU miner instance. Constructed with a pointer to `this` (as `i_miner_handler`) and a block-hashing lambda. |
| `m_config_folder` | `std::string` | Data directory path (from `--data-dir`). |
| `m_nettype` | `network_type` | One of `MAINNET`, `TESTNET`, `STAGENET`, `FAKECHAIN`, `UNDEFINED`. |
| `m_offline` | `bool` | If true, daemon does not connect to peers (from `--offline`). |
| `m_target_blockchain_height` | `uint64_t` | Expected height of the network (set by sync protocol). |
| `m_test_drop_download` | `bool` | Testing flag: if false, all downloaded blocks are discarded. |
| `m_test_drop_download_height` | `uint64_t` | Testing flag: blocks below this height are discarded. |
| `m_checkpoints_path` | `std::string` | Path to JSON checkpoints file. |
| `m_last_dns_checkpoints_update` | `time_t` | Timestamp of last DNS checkpoint refresh. |
| `m_last_json_checkpoints_update` | `time_t` | Timestamp of last JSON checkpoint refresh. |
| `m_checkpoints_updating` | `std::atomic_flag` | Guards against concurrent checkpoint updates. |
| `m_disable_dns_checkpoints` | `bool` | If true, DNS checkpoints are never fetched. |
| `m_update_available` | `std::atomic<bool>` | Set to true when a newer Monero version is detected. |
| `m_update_download` | `tools::download_async_handle` | Handle to an in-progress update download (0 if idle). |
| `m_update_mutex` | `boost::mutex` | Guards `m_update_download`. |
| `m_last_update_length` | `size_t` | Tracks bytes downloaded for progress logging. |
| `m_block_rate_notify` | `std::shared_ptr<tools::Notify>` | External notification command for block-rate anomalies. |
| `block_sync_size` | `size_t` | Fixed block sync batch size (0 = adaptive, from `--block-sync-size`). |
| `batch_max_weight` | `uint64_t` | Maximum bytes per sync batch (from `--batch-max-weight`, converted to bytes from MB). |
| `start_time` | `time_t` | Daemon start timestamp. |
| `check_updates_level` | enum `{UPDATES_DISABLED, UPDATES_NOTIFY, UPDATES_DOWNLOAD, UPDATES_UPDATE}` | Controls auto-update behavior. |
| `m_starter_message_showed` | `std::atomic<bool>` | One-shot flag for the startup banner message. |

**Periodic-task timers** (all `epee::math_helper::once_a_time_seconds`):

| Timer | Interval | Task |
|-------|----------|------|
| `m_store_blockchain_interval` | 12 hours | Manual blockchain storage sync. |
| `m_fork_moaner` | 2 hours | Hard fork status warning. |
| `m_check_updates_interval` | 12 hours | DNS version check. |
| `m_check_disk_space_interval` | 10 minutes | Free disk space check. |
| `m_block_rate_interval` | 90 seconds | Block rate anomaly detection. |
| `m_blockchain_pruning_interval` | 5 hours | Incremental blockchain pruning. |
| `m_diff_recalc_interval` | 7 days | Difficulty recalculation past last checkpoint. |

### `cryptonote::test_options` (struct)

Used to configure the `core` for unit/integration tests.

| Field | Type | Description |
|-------|------|-------------|
| `hard_forks` | `const std::pair<uint8_t, uint64_t>*` | Array of (version, activation_height) pairs, null-terminated with version 0. |
| `long_term_block_weight_window` | `size_t` | Override for the long-term block weight averaging window. |

### `BlockchainAndPool` (struct)

Solves the circular-construction problem between `Blockchain` and `tx_memory_pool`, which each require a reference to the other at construction time. Contains:
- `Blockchain blockchain` -- constructed with a reference to `tx_pool`.
- `tx_memory_pool tx_pool` -- constructed with a reference to `blockchain`.

The constructor deliberately suppresses the GCC "uninitialized" warning because neither object is fully initialized when its reference is passed to the other's constructor.

### `i_core_events` (interface)

Pure virtual interface enabling the protocol layer to interact with core without a direct dependency on the `core` class.

| Method | Returns | Description |
|--------|---------|-------------|
| `get_current_blockchain_height() const` | `uint64_t` | Current chain height. |
| `is_synchronized() const` | `bool` | Whether daemon believes it is synced with the network. |
| `on_transactions_relayed(span<const blobdata>, relay_method)` | `void` | Called by levin_notify when txs are actually sent to peers. |

### `HAVE_BLOCK_*` enum (anonymous, file scope)

| Constant | Value | Meaning |
|----------|-------|---------|
| `HAVE_BLOCK_MAIN_CHAIN` | 0 | Block exists on the main chain. |
| `HAVE_BLOCK_ALT_CHAIN` | 1 | Block exists on an alternative chain. |
| `HAVE_BLOCK_INVALID` | 2 | Block is known to be invalid (cached). |

### `tx_verification_context` and `block_verification_context` (defined in `src/cryptonote_basic/verification_context.h`)

These are return-by-reference structs used throughout the `core` API to communicate verification outcomes. Key fields are documented inline in that header. Notable: the field names contain the historical misspelling `m_verifivation_failed` (not `verification`).

## Public API

### Lifecycle

| Signature | Description |
|-----------|-------------|
| `core(i_cryptonote_protocol* pprotocol)` | Constructor. Sets member variables to initial state. If `pprotocol` is null, uses internal `m_protocol_stub`. |
| `static void init_options(boost::program_options::options_description& desc)` | Registers all command-line options (data-dir, testnet, stagenet, regtest, sync modes, pruning, notifications, etc.) with the options parser. Also calls `miner::init_options()` and `BlockchainDB::init_options()`. |
| `bool init(const variables_map& vm, const test_options* test_options = NULL, const GetCheckpointsCallback& get_checkpoints = nullptr, bool allow_dns = true)` | Full initialization: parses CLI args, opens database, initializes Blockchain, mempool, miner, checkpoints, pruning. Returns false on failure. |
| `bool deinit()` | Shuts down miner, mempool, and blockchain storage. Always returns true. |
| `void stop()` | Stops the miner, cancels blockchain operations, and cancels any in-progress update download. |
| `void graceful_exit()` | Raises `SIGTERM` to trigger the installed signal handlers for clean shutdown. |
| `bool set_genesis_block(const block& b)` | Resets blockchain and sets the genesis block. Delegates to `Blockchain::reset_and_set_genesis_block`. |

### Block Handling

| Signature | Description |
|-----------|-------------|
| `bool handle_incoming_block(const blobdata& block_blob, const block* b, block_verification_context& bvc, bool update_miner_blocktemplate = true)` | Adds a block as part of a batch. **Precondition:** an active write transaction must exist (via `prepare_handle_incoming_blocks`). Parses blob if `b` is null. Updates miner template on success if requested. |
| `bool handle_incoming_block(const blobdata& block_blob, const block* b, block_verification_context& bvc, pool_supplement& extra_block_txs, bool update_miner_blocktemplate = true)` | Same as above but accepts supplemental transactions (from the pool or synced alongside the block). |
| `bool handle_single_incoming_block(const blobdata& block_blob, const block* b, block_verification_context& bvc, pool_supplement& extra_block_txs, bool update_miner_blocktemplate = true)` | Self-contained single-block handler: creates its own write transaction via `prepare_handle_incoming_block_no_preprocess` / `cleanup_handle_incoming_blocks`. Acquires `m_incoming_tx_lock`. |
| `bool prepare_handle_incoming_blocks(const vector<block_complete_entry>& blocks_entry, vector<block>& blocks)` | Acquires `m_incoming_tx_lock` and begins a blockchain write transaction for batch block processing. |
| `bool cleanup_handle_incoming_blocks(bool force_sync = false)` | Commits or aborts the batch write transaction started by `prepare_handle_incoming_blocks`. Releases `m_incoming_tx_lock`. |
| `bool check_incoming_block_size(const blobdata& block_blob) const` | Rejects blocks whose blob size exceeds `current_cumulative_block_weight_limit + 100` bytes (sanity leeway). |
| `bool handle_block_found(block& b, block_verification_context& bvc)` | Called by the miner when it finds a block. Pauses mining, stores the block, relays it as a fluffy block to peers, then resumes mining. Returns false if the block fails verification. Implements `i_miner_handler`. |

### Transaction Handling

| Signature | Description |
|-----------|-------------|
| `bool handle_incoming_tx(const blobdata& tx_blob, tx_verification_context& tvc, relay_method tx_relay, bool relayed)` | Main entry point for incoming transactions. Acquires `m_incoming_tx_lock`. Rejects blobs exceeding `get_max_tx_size()`. Parses, validates, and adds to the mempool. Sets `tvc` flags on failure. |
| `void on_transactions_relayed(span<const blobdata> tx_blobs, relay_method tx_relay)` | Called by `levin_notify` after txs are actually sent to peers. Marks them as relayed in the mempool and publishes ZMQ txpool events. Implements `i_core_events`. |
| `static bool check_tx_semantic(const transaction& tx, tx_verification_context& tvc, uint8_t hf_version)` | Validates basic transaction properties: non-empty inputs, supported input types, valid outputs, no money overflow, input > output for v1 txs, unique key images, distinct ring members (HF >= 6), key images in valid domain, correct output types. |
| `static bool check_tx_inputs_keyimages_diff(const transaction& tx)` | Returns false if any key image appears more than once in the transaction. |
| `static bool check_tx_inputs_ring_members_diff(const transaction& tx, uint8_t hf_version)` | From hard fork v6 onward, returns false if any ring has a zero offset after the first element (indicating a duplicate member). |
| `static bool check_tx_inputs_keyimages_domain(const transaction& tx)` | Verifies each key image is not the identity point and is in the correct subgroup (i.e., `l * K == identity`). |

### Block Template / Mining

| Signature | Description |
|-----------|-------------|
| `bool get_block_template(block& b, const account_public_address& adr, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, uint64_t& cumulative_weight, const blobdata& ex_nonce, uint64_t& seed_height, crypto::hash& seed_hash)` | Creates a new block template for solo mining. Implements `i_miner_handler`. |
| `bool get_block_template(block& b, const crypto::hash* prev_block, const account_public_address& adr, ...)` | Variant that allows specifying an explicit previous block hash (for mining on alternative tips). |
| `bool get_miner_data(uint8_t& major_version, uint64_t& height, crypto::hash& prev_id, crypto::hash& seed_hash, difficulty_type& difficulty, uint64_t& median_weight, uint64_t& already_generated_coins, vector<tx_block_template_backlog_entry>& tx_backlog)` | Returns data needed by external (e.g., Stratum) miners to construct their own block templates. |
| `miner& get_miner()` / `const miner& get_miner() const` | Direct access to the built-in miner instance. |
| `void pause_mine()` / `void resume_mine()` | Pause and resume the built-in miner. |

### Blockchain Queries

| Signature | Description |
|-----------|-------------|
| `uint64_t get_current_blockchain_height() const` | Current height (number of blocks). Implements `i_core_events`. |
| `void get_blockchain_top(uint64_t& height, crypto::hash& top_id) const` | Returns the top block's height and hash. |
| `bool get_blocks(uint64_t start_offset, size_t count, ...)` | Three overloads: returns blocks as (blob, block) pairs with txs, as (blob, block) pairs without txs, or as plain `block` objects. |
| `template<...> bool get_blocks(const t_ids_container&, t_blocks_container&, t_missed_container&)` | Template overload: fetches blocks by hash, reports missed hashes. |
| `crypto::hash get_block_id_by_height(uint64_t height) const` | Block hash at the given height. |
| `bool get_block_by_hash(const crypto::hash& h, block& blk, bool* orphan = NULL) const` | Fetches a block by hash, optionally indicating if it is an orphan. |
| `bool get_transactions(const vector<crypto::hash>&, vector<blobdata>&, vector<crypto::hash>& missed, bool pruned = false) const` | Fetches transaction blobs by hash from the blockchain DB. |
| `bool get_transactions(const vector<crypto::hash>&, vector<transaction>&, vector<crypto::hash>& missed, bool pruned = false) const` | Same but returns deserialized `transaction` objects. |
| `bool get_split_transactions_blobs(...)` | Returns transactions split into prunable and non-prunable parts. |
| `bool get_tx_outputs_gindexs(const crypto::hash& tx_id, vector<uint64_t>& indexs) const` | Global output indices for a transaction. |
| `bool get_tx_outputs_gindexs(const crypto::hash& tx_id, size_t n_txes, vector<vector<uint64_t>>& indexs) const` | Batch variant for multiple consecutive transactions. |
| `crypto::hash get_tail_id() const` | Hash of the most recent block. |
| `difficulty_type get_block_cumulative_difficulty(uint64_t height) const` | Cumulative difficulty at a given height. |
| `size_t get_blockchain_total_transactions() const` | Total number of transactions stored in the blockchain. |
| `bool have_block(const crypto::hash& id, int* where = NULL) const` | Checks if a block hash is known. `where` is set to one of `HAVE_BLOCK_MAIN_CHAIN`, `HAVE_BLOCK_ALT_CHAIN`, or `HAVE_BLOCK_INVALID`. |
| `bool have_block_unlocked(const crypto::hash& id, int* where = NULL) const` | Same as `have_block` but without holding the blockchain lock. |
| `bool get_alternative_blocks(vector<block>& blocks) const` | Returns all blocks on alternative chains. |
| `size_t get_alternative_blocks_count() const` | Number of blocks on alternative chains. |
| `bool get_short_chain_history(list<crypto::hash>& ids, uint64_t& current_height) const` | Returns a sparse set of block hashes for chain synchronization (exponentially spaced). |
| `bool find_blockchain_supplement(const list<crypto::hash>& qblock_ids, bool clip_pruned, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) const` | Finds the common block with a peer and returns the chain entry response. |
| `bool find_blockchain_supplement(uint64_t req_start_block, const list<crypto::hash>& qblock_ids, ..., size_t max_block_count, size_t max_tx_count) const` | Extended variant returning actual block and tx data for synchronization. |
| `bool handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp, cryptonote_connection_context& context)` | Handles a P2P "get objects" request. |
| `bool get_outs(const COMMAND_RPC_GET_OUTPUTS_BIN::request& req, COMMAND_RPC_GET_OUTPUTS_BIN::response& res) const` | Returns output details for given output indices. |
| `bool get_output_distribution(uint64_t amount, uint64_t from_height, uint64_t to_height, uint64_t& start_height, vector<uint64_t>& distribution, uint64_t& base) const` | Returns per-block output distribution for a given amount. |
| `std::pair<uint128_t, uint128_t> get_coinbase_tx_sum(uint64_t start_offset, size_t count)` | Sums coinbase emissions and fees over a block range. |
| `uint64_t prevalidate_block_hashes(uint64_t height, const vector<crypto::hash>& hashes, const vector<uint64_t>& weights)` | Validates block hashes against the precompiled hash set. Returns number of usable blocks. |
| `bool is_within_compiled_block_hash_area(uint64_t height) const` | Checks if a height falls within the range covered by compiled-in block hashes. |
| `bool has_block_weights(uint64_t height, uint64_t nblocks) const` | Checks if block weights are available for a given range. |

### Transaction Pool Queries

| Signature | Description |
|-----------|-------------|
| `bool pool_has_tx(const crypto::hash& txid) const` | Whether a tx is in the mempool. |
| `bool get_pool_transactions(vector<transaction>& txs, bool include_sensitive_txes = false) const` | All mempool transactions. |
| `bool get_pool_transaction_hashes(vector<crypto::hash>& txs, bool include_sensitive_txes = false) const` | All mempool transaction hashes. |
| `bool get_pool_transactions_info(const vector<crypto::hash>& txids, vector<pair<crypto::hash, tx_details>>& txs, bool include_sensitive = false) const` | Detailed info for specific mempool transactions. |
| `bool get_pool_info(time_t start_time, bool include_sensitive, size_t max_tx_count, vector<...>& added_txs, vector<crypto::hash>& remaining, vector<crypto::hash>& removed_txs, bool& incremental) const` | Incremental pool change information since a given timestamp. |
| `bool get_pool_transaction_stats(txpool_stats& stats, bool include_sensitive = false) const` | Aggregate mempool statistics. |
| `bool get_pool_transaction(const crypto::hash& id, blobdata& tx, relay_category tx_category) const` | Fetches a single mempool transaction blob. |
| `bool get_pool_transactions_and_spent_keys_info(vector<tx_info>&, vector<spent_key_image_info>&, bool include_sensitive = false) const` | Full mempool dump with spent key image info (used by RPC). |
| `bool get_pool_for_rpc(vector<rpc::tx_in_pool>&, rpc::key_images_with_tx_hashes&) const` | Mempool data formatted for the ZMQ RPC interface. |
| `size_t get_pool_transactions_count(bool include_sensitive = false) const` | Number of transactions in the mempool. |
| `bool get_txpool_backlog(vector<tx_backlog_entry>& backlog, bool include_sensitive = false) const` | Returns the txpool backlog for fee estimation. |
| `bool get_txpool_complement(const vector<crypto::hash>& hashes, vector<blobdata>& txes)` | Returns txpool transactions NOT in the given hash set (for fluffy block reconstruction). |

### Key Image Queries

| Signature | Description |
|-----------|-------------|
| `bool is_key_image_spent(const crypto::key_image& key_im) const` | Checks if a key image is spent in the blockchain. |
| `bool are_key_images_spent(const vector<crypto::key_image>& key_im, vector<bool>& spent) const` | Batch version; checks against blockchain. Always returns true. |
| `bool are_key_images_spent_in_pool(const vector<crypto::key_image>& key_im, vector<bool>& spent) const` | Checks if key images are spent in the mempool only. |

### Hard Fork Queries

| Signature | Description |
|-----------|-------------|
| `uint8_t get_ideal_hard_fork_version() const` | The newest hard fork version defined in the software. |
| `uint8_t get_ideal_hard_fork_version(uint64_t height) const` | The ideal hard fork version for a given height. |
| `uint8_t get_hard_fork_version(uint64_t height) const` | The actual hard fork version at a given height. |
| `uint64_t get_earliest_ideal_height_for_version(uint8_t version) const` | The earliest height at which a given hard fork version may activate. |

### Checkpoints

| Signature | Description |
|-----------|-------------|
| `const checkpoints& get_checkpoints() const` | Returns the current checkpoint set. |
| `void set_checkpoints(checkpoints&& chk_pts)` | Replaces the checkpoint set (move semantics). |
| `void set_checkpoints_file_path(const std::string& path)` | Sets path to the JSON checkpoints file. |
| `void set_enforce_dns_checkpoints(bool enforce_dns)` | Whether DNS checkpoints are enforced (hard vs. soft). |
| `void disable_dns_checkpoints(bool disable = true)` | Disables DNS checkpoint fetching entirely. |
| `bool update_checkpoints(bool skip_dns = false)` | Refreshes checkpoints from DNS (every 1 hour) and/or JSON file (every 10 minutes). Only runs on MAINNET. Calls `graceful_exit()` if checkpoint loading fails. |

### Pruning

| Signature | Description |
|-----------|-------------|
| `uint32_t get_blockchain_pruning_seed() const` | Returns the current pruning seed (0 if not pruned). |
| `bool prune_blockchain(uint32_t pruning_seed = 0)` | Prunes the blockchain (0 = default seed). |
| `bool update_blockchain_pruning()` | Incrementally prunes newly added blocks. |
| `bool check_blockchain_pruning()` | Validates the pruning state. |

### Synchronization and Protocol

| Signature | Description |
|-----------|-------------|
| `bool is_synchronized() const` | Delegates to `m_pprotocol->is_synchronized()`. Returns false if protocol is null. Implements `i_core_events`. |
| `void on_synchronized()` | Called when sync completes; notifies the miner. |
| `void set_cryptonote_protocol(i_cryptonote_protocol* pprotocol)` | Sets the protocol handler. Falls back to `m_protocol_stub` if null. |
| `i_cryptonote_protocol* get_protocol()` | Returns the current protocol handler pointer. |
| `void set_target_blockchain_height(uint64_t target)` | Sets the network's expected chain height. |
| `uint64_t get_target_blockchain_height() const` | Returns the target blockchain height. |
| `size_t get_block_sync_size(uint64_t height, uint64_t max_avg_blocksize_in_queue = 0) const` | Calculates the optimal number of blocks to request in the next sync batch. Uses adaptive sizing based on recent block weights and `batch_max_weight`, unless overridden by `--block-sync-size`. Clamped to `BLOCKS_SYNCHRONIZING_MAX_COUNT` (2048). |
| `void safesyncmode(const bool onoff)` | Toggles safe sync mode on the blockchain (forces synchronous DB writes). |

### Miscellaneous

| Signature | Description |
|-----------|-------------|
| `bool on_idle()` | Called periodically by the protocol layer. Shows startup banner (once), relays mempool transactions, triggers periodic checks (updates, disk space, block rate, pruning, difficulty recalculation), and calls `miner::on_idle()` and `mempool::on_idle()`. Always returns true. |
| `Blockchain& get_blockchain_storage()` / `const` variant | Direct access to the `Blockchain` instance. |
| `std::string print_pool(bool short_format) const` | Returns a human-readable dump of the mempool. |
| `network_type get_nettype() const` | Returns the network type (`MAINNET`, `TESTNET`, `STAGENET`, `FAKECHAIN`). |
| `bool is_update_available() const` | Returns the cached result of the last update check. |
| `uint64_t get_free_space() const` | Free disk space (bytes) on the blockchain partition. |
| `bool offline() const` | Whether the daemon is running in offline mode. |
| `std::time_t get_start_time() const` | Daemon start timestamp. |
| `void flush_invalid_blocks()` | Clears the invalid block cache. |

### Testing Helpers

| Signature | Description |
|-----------|-------------|
| `void test_drop_download()` | Sets `m_test_drop_download = false` (note: naming is inverted; this *enables* dropping). |
| `void test_drop_download_height(uint64_t height)` | Sets the height threshold for dropping downloaded blocks. |
| `bool get_test_drop_download() const` | Returns current drop-download flag. |
| `bool get_test_drop_download_height() const` | Returns true if current height <= threshold or threshold is 0. |

## Internal Logic

### Initialization Flow (`init`)

1. Record `start_time`.
2. Detect test/regtest mode: if `test_options` is non-null or `--regtest` is set, use `FAKECHAIN` network type.
3. `handle_command_line()`: parse network type, set data directory, load default checkpoints (MAINNET only), apply test flags.
4. Parse DB sync mode string (`--db-sync-mode`). Supported: `safe`, `fast` (default), `fastest`, with optional `sync`/`async` and numeric threshold.
5. Open the blockchain database (LMDB). For FAKECHAIN without `--keep-fakechain`, the DB is deleted first.
6. Configure `Blockchain` user options (thread count, sync mode, fast sync).
7. Set up notification handlers: `--block-notify`, `--reorg-notify`, `--block-rate-notify`.
8. For regtest mode, configure hard forks: v1 at height 0, latest mainnet version at height 1.
9. Initialize `Blockchain` with the database, network type, offline flag, test options, fixed difficulty.
10. Initialize `tx_memory_pool` with max weight. If the hard fork version changed since last run, validate the mempool against current rules.
11. Configure block sync batch sizing (`--block-sync-size`, `--batch-max-weight`).
12. Load and verify checkpoints (JSON and DNS).
13. Parse `--check-updates` level.
14. Initialize the miner.
15. Optionally drop alternative blocks, prune the blockchain.
16. Call `load_state_data()` (currently a no-op placeholder).

### Incoming Transaction Flow (`handle_incoming_tx`)

1. Zero-initialize `tvc`.
2. Acquire `m_incoming_tx_lock`.
3. Reject if blob size > `get_max_tx_size()` (sets `tvc.m_too_big`).
4. Parse and validate the blob into a `transaction` object.
5. Calculate transaction weight.
6. Call `add_new_tx()`:
   - If tx is already in the mempool or blockchain, return true (no-op).
   - Otherwise, call `tx_memory_pool::add_tx()` which performs full validation.
   - On success, notify ZMQ subscribers via `notify_txpool_event`.
7. Return based on `tvc` flags.

### Incoming Block Flow (`handle_incoming_block`)

**Batch mode** (called between `prepare_handle_incoming_blocks` and `cleanup_handle_incoming_blocks`):
1. Zero-initialize `bvc`.
2. Check block blob size against `check_incoming_block_size()`.
3. Parse blob if no pre-parsed block was provided.
4. Call `Blockchain::add_new_block()` which performs full validation.
5. If added to main chain and `update_miner_blocktemplate` is true, update the miner's template.

**Single block mode** (`handle_single_incoming_block`):
1. Estimate total block weight (blob + supplemental txs).
2. Acquire `m_incoming_tx_lock`.
3. Call `prepare_handle_incoming_block_no_preprocess()` to begin a write transaction.
4. Set up a scope guard to call `cleanup_handle_incoming_blocks()`.
5. Delegate to `handle_incoming_block()`.

### Mined Block Flow (`handle_block_found`)

1. Pause the miner.
2. Gather block and its transactions into a `block_complete_entry`.
3. `prepare_handle_incoming_blocks()` -> `add_new_block()` -> `cleanup_handle_incoming_blocks()`.
4. Update miner block template.
5. Resume the miner.
6. If block was added to the main chain, relay it as a fluffy block (header only, no tx bodies) to all peers.

### Idle Loop (`on_idle`)

Called periodically by the P2P layer. Performs:
1. One-time startup message display.
2. `relay_txpool_transactions()`: re-relays mempool txs that should be relayed. Sorts into public (fluff), private (local), and stem (Dandelion++) queues.
3. Periodic tasks via timer objects:
   - **Update check** (12h): DNS lookup for newer Monero versions. Can notify, download, or (not yet implemented) auto-update.
   - **Disk space check** (10m): Warns if free space < 1 GB.
   - **Block rate check** (90s): Uses Poisson distribution to detect statistically anomalous block rates over 10/20/30/60/90-minute windows. Triggers `--block-rate-notify` if probability drops below threshold (1 false positive per 10 days).
   - **Blockchain pruning** (5h): Incremental pruning of new blocks.
   - **Difficulty recalculation** (7d): Recalculates difficulties past the last checkpoint.
4. `miner::on_idle()` and `mempool::on_idle()`.

### Block Sync Size Calculation (`get_block_sync_size`)

If `--block-sync-size` is explicitly set, that value is used directly. Otherwise, adaptive sizing:
1. Look at the largest block weight in the last `BLOCKS_MAX_WINDOW` (100) blocks.
2. Take the larger of that max and the max average blocksize currently in the download queue.
3. If `projected_blocksize * BLOCKS_MAX_WINDOW < batch_max_weight`: request `BLOCKS_MAX_WINDOW` blocks.
4. If `projected_blocksize >= batch_max_weight / 2`: request just 1 block.
5. Otherwise: request `batch_max_weight / projected_blocksize` blocks.
6. Clamp to `BLOCKS_SYNCHRONIZING_MAX_COUNT` (2048), or to `SEEDHASH_EPOCH_BLOCKS` if set via environment variable.

### Checkpoint Update Logic (`update_checkpoints`)

- Only runs on MAINNET (unless DNS checkpoints are disabled).
- Uses `m_checkpoints_updating` atomic flag to prevent concurrent updates.
- DNS checkpoints: refreshed every 3600 seconds (1 hour).
- JSON checkpoints: refreshed every 600 seconds (10 minutes).
- If checkpoint loading fails, calls `graceful_exit()` (raises SIGTERM).

### Transaction Relay Logic (`relay_txpool_transactions`)

- Queries mempool for transactions that need relaying.
- Categorizes by `relay_method`:
  - `local` -> private relay (zone::invalid)
  - `forward` -> stem relay (Dandelion++)
  - `block`, `fluff`, `stem` -> public relay (zone::public_, method::fluff)
  - `none` -> not relayed
- Uses nil UUID as source (not from any specific peer).

## Dependencies

### This module depends on

| Dependency | Role |
|------------|------|
| `Blockchain` (cryptonote_core) | Block storage, validation, hard fork management, checkpoints. |
| `tx_memory_pool` (cryptonote_core) | Transaction pool management. |
| `miner` (cryptonote_basic) | Built-in CPU mining. |
| `i_cryptonote_protocol` (cryptonote_protocol) | P2P block and transaction relay. |
| `BlockchainDB` (blockchain_db) | Database abstraction (LMDB). |
| `ringct` | RingCT signature types and operations (key image domain checks). |
| `checkpoints` | Checkpoint loading and verification. |
| `hardforks` | Hard fork schedule definitions (`mainnet_hard_forks`, etc.). |
| `common` | Utilities: `Notify`, `download_async`, `command_line`, `threadpool`, `updates`. |
| `cncrypto` | Core cryptographic primitives. |
| `device` | Hardware device abstraction. |
| `version` | `MONERO_VERSION` for update comparison. |
| `rpc/zmq_pub` | ZMQ publish for txpool events. |
| Boost | `filesystem`, `program_options`, `thread`, `multiprecision` (uint128_t), `algorithm`, `uuid`. |

### What depends on this module

| Dependent | How it uses `core` |
|-----------|-------------------|
| `core_rpc_server` (rpc) | Calls nearly all public query methods for RPC endpoints. |
| `daemon_handler` (rpc) | ZMQ RPC handler; uses `core` for blockchain and pool queries. |
| `cryptonote_protocol_handler` (cryptonote_protocol) | Calls block/tx handling, sync queries, `on_idle`. |
| `net_node` (p2p) | Passes protocol events that flow through to `core`. |
| Various test harnesses | `core_tests`, `unit_tests` for `blockchain`, `tx_pool`, `output_distribution`, `long_term_block_weight`, etc. |

## Configuration

### Command-Line Options (registered in `core::init_options`)

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `--data-dir` | string | OS-specific default | Blockchain data directory. Automatically appended with `/testnet` or `/stagenet` if those flags are set. |
| `--testnet` | bool | false | Run on testnet. |
| `--stagenet` | bool | false | Run on stagenet. |
| `--regtest` | bool | false | Regression testing mode (uses FAKECHAIN with latest hard fork at height 1). |
| `--keep-fakechain` | bool | false | Do not delete existing DB when in fakechain mode. |
| `--fixed-difficulty` | uint64 | 0 | Fixed difficulty for testing (0 = normal difficulty). |
| `--offline` | bool | false | Do not listen for or connect to peers. |
| `--disable-dns-checkpoints` | bool | false | Do not retrieve checkpoints from DNS. |
| `--enforce-dns-checkpointing` | bool | false | Make DNS checkpoints enforced (hard checkpoints). |
| `--test-drop-download` | bool | false | Discard all downloaded blocks (net testing). |
| `--test-drop-download-height` | uint64 | 0 | Discard downloaded blocks only above this height. |
| `--test-dbg-lock-sleep` | int | 0 | Sleep time in ms before/after mutex locks (debugging). |
| `--fast-block-sync` | uint64 | 1 | Use embedded known block hashes for faster sync. |
| `--prep-blocks-threads` | uint64 | 4 | Max threads for preparing block hashes in groups. |
| `--show-time-stats` | uint64 | 0 | Show time statistics for block/tx processing. |
| `--block-sync-size` | size_t | 0 | Blocks to sync at once (0 = adaptive). |
| `--batch-max-weight` | size_t | 10 (MB) | Max megabytes per sync batch. Max allowed: 50 MB. |
| `--block-download-max-size` | size_t | 0 | Max block download queue size in bytes (0 = default). |
| `--span-limit` | size_t | 2 | Minutes of block sync data to request at a time. |
| `--sync-pruned-blocks` | bool | false | Allow syncing from nodes with only pruned blocks. |
| `--check-updates` | string | "notify" | Update check behavior: `disabled`, `notify`, `download`, `update`. |
| `--max-txpool-weight` | size_t | 648000000 | Max txpool weight in bytes (~3 days at 300KB blocks). |
| `--block-notify` | string | "" | Command to run on new block (`%s` = block hash). |
| `--reorg-notify` | string | "" | Command to run on reorg (`%s` = split height, `%h` = new height, `%n` = new blocks, `%d` = discarded blocks). |
| `--block-rate-notify` | string | "" | Command to run on block rate anomaly (`%t` = minutes, `%b` = blocks, `%e` = expected). |
| `--prune-blockchain` | bool | false | Enable blockchain pruning. |
| `--keep-alt-blocks` | bool | false | Keep alternative blocks on restart. |

Additionally, `miner::init_options()` and `BlockchainDB::init_options()` register their own options (e.g., `--db-sync-mode`, `--db-salvage`, `--start-mining`, etc.).

### Environment Variables

| Variable | Used in | Description |
|----------|---------|-------------|
| `SEEDHASH_EPOCH_BLOCKS` | `get_block_sync_size()` | Overrides `BLOCKS_SYNCHRONIZING_MAX_COUNT` as the maximum sync batch size. Value is rounded up to the next power of 2. |

### Compile-Time Constants (from `src/cryptonote_config.h`)

| Constant | Value | Description |
|----------|-------|-------------|
| `DIFFICULTY_TARGET_V2` | 120 | Target block time in seconds (2 minutes). |
| `BLOCKS_SYNCHRONIZING_MAX_COUNT` | 2048 | Max blocks per sync batch. Must be power of 2. |
| `BATCH_MAX_WEIGHT` | 10 | Default max batch weight in MB. |
| `BATCH_MAX_ALLOWED_WEIGHT` | 50 | Hard cap on batch weight in MB. |
| `BLOCKS_MAX_WINDOW` | 100 | Window size for historical max block weight lookup. |
| `DEFAULT_TXPOOL_MAX_WEIGHT` | 648000000 | Default max mempool size in bytes. |

### Source-Level Constants

| Constant | Value | Location |
|----------|-------|----------|
| `BLOCK_SIZE_SANITY_LEEWAY` | 100 | `cryptonote_core.cpp:72` -- extra bytes allowed beyond block weight limit in `check_incoming_block_size`. |

## Known Issues

No `TODO`, `FIXME`, `HACK`, or `XXX` comments were found in `cryptonote_core.h` or `cryptonote_core.cpp`.

### Other observations

- **Historical misspelling**: The field `m_verifivation_failed` (in both `tx_verification_context` and `block_verification_context`) is misspelled. This is a longstanding artifact preserved for compatibility across the codebase.
- **`load_state_data()` is a no-op**: The implementation at `cryptonote_core.cpp:772` contains only a comment "may be some code later" and returns true. It is called at the end of `init()`.
- **`check_updates` auto-update not implemented**: At `cryptonote_core.cpp:1806`, the `UPDATES_UPDATE` level logs `"Download/update not implemented yet"` and returns true without performing an update.
- **`test_drop_download()` naming inversion**: The method name suggests enabling test-drop-download, but it sets `m_test_drop_download = false`. The flag's semantics are inverted: `true` means blocks are *not* dropped (normal behavior), `false` means they *are* dropped.
- **`m_miner` comment**: At `cryptonote_core.h:1075`, a comment reads `"m_miner and m_miner_addres are probably temporary here"`, though `m_miner_address` no longer exists as a member. The miner has remained a permanent part of the core.
