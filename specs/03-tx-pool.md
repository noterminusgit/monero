# Transaction Pool

## Overview

The transaction pool (`tx_memory_pool`) manages all transactions that have been received by the node but have not yet been included in a block. It is the central staging area between transaction receipt (from the P2P network, local RPC submissions, or block reorganization rollbacks) and block construction by miners. The pool stores transactions, organizes them by fee-per-weight for efficient block template construction, enforces consensus rules and size limits, handles Dandelion++ privacy-preserving relay states, tracks spent key images to detect double spends, supports incremental pool change queries for wallets, and prunes low-priority transactions when the pool exceeds its configured maximum weight.

## Key Files

| File | Lines | Description |
|------|------:|-------------|
| `src/cryptonote_core/tx_pool.h` | 722 | Class declaration for `tx_memory_pool`, including all public/private methods, the `tx_details` struct, type aliases for sorted containers and key image maps, and all member variables. |
| `src/cryptonote_core/tx_pool.cpp` | 1959 | Full implementation of `tx_memory_pool`: transaction addition/removal, validation, block template filling, relay management, pruning, stuck-transaction removal, pool serialization, and incremental pool info queries. |
| `src/cryptonote_core/blockchain_and_pool.h` | 63 | Defines `BlockchainAndPool`, a helper struct that safely co-constructs `Blockchain` and `tx_memory_pool` (which have circular references). |
| `src/cryptonote_protocol/enums.h` | 46 | Defines `relay_method` enum used to track how a transaction was received/relayed. |
| `src/blockchain_db/blockchain_db.h` | (relevant: 154-193) | Defines `txpool_tx_meta_t` (192-byte packed struct stored in DB) and `relay_category` enum. |
| `src/rpc/core_rpc_server_commands_defs.h` | (relevant: 1627-1700) | Defines `tx_backlog_entry`, `txpool_histo`, and `txpool_stats` structs used by RPC responses. |
| `src/cryptonote_core/cryptonote_tx_utils.h` | (relevant: 111-116) | Defines `tx_block_template_backlog_entry` struct. |

## Data Structures

### `tx_memory_pool` (class)

The main pool class, declared `boost::noncopyable`. Constructed via `BlockchainAndPool` to resolve the circular dependency with `Blockchain`.

**Member variables:**

| Field | Type | Description |
|-------|------|-------------|
| `m_transactions_lock` | `epee::critical_section` (mutable) | Recursive mutex protecting all pool state. Made conditionally public under `DEBUG_CREATE_BLOCK_TEMPLATE`. |
| `m_blockchain` | `Blockchain&` | Reference to the blockchain instance for input validation, DB operations, and block reward calculations. |
| `m_spent_key_images` | `key_images_container` (`unordered_map<key_image, unordered_set<hash>>`) | Maps each spent key image to the set of transaction hashes that spend it. Multiple txes per key image are allowed only for `kept_by_block` transactions (from block reorgs). |
| `m_txs_by_fee_and_receive_time` | `sorted_tx_container` (Boost bimap) | Bidirectional map organizing transactions by `(fee_per_byte, receive_time)` on the left and `tx_hash` on the right. Left side uses `txFeeCompare` (highest fee first, then earliest time). Used for block template filling and pruning. |
| `m_txpool_max_weight` | `size_t` | Maximum allowed cumulative pool weight in bytes. Default: `648000000` (~648 MB, ~3 days at 300KB median blocks). |
| `m_txpool_weight` | `size_t` | Current cumulative pool weight in bytes. |
| `m_cookie` | `atomic<uint64_t>` | Monotonically increasing counter incremented on every pool mutation. Used by external consumers to detect changes. |
| `m_mine_stem_txes` | `bool` | If true, transactions in Dandelion++ stem state are eligible for block template inclusion. Set to true only for `FAKECHAIN` (test) networks. |
| `m_timed_out_transactions` | `unordered_set<hash>` | Hashes of transactions removed for being stuck/expired. These are rejected if re-submitted (unless `kept_by_block`). Not persisted to disk. |
| `m_input_cache` | `unordered_map<hash, tuple<bool, tvc, uint64_t, hash>>` (mutable) | Caches `check_tx_inputs` results keyed by txid. Cleared on blockchain height changes (`on_blockchain_inc`/`on_blockchain_dec`). Only used for non-`kept_by_block` transactions. |
| `m_parsed_tx_cache` | `unordered_map<hash, transaction>` | Caches parsed `transaction` objects for `kept_by_block` txes to avoid re-parsing. Cleared on blockchain height changes. |
| `m_added_txs_by_id` | `unordered_map<hash, time_t>` | Maps txid to the wall-clock time it was added to the pool. Used for incremental pool queries. |
| `m_added_txs_start_time` | `time_t` | Earliest time tracked in `m_added_txs_by_id`. Set to 0 when tracking has not started or was reset. |
| `m_removed_txs_by_time` | `multimap<time_t, removed_tx_info>` | Ordered record of removed transactions by removal time. Capped at 20000 entries; entries older than 30 minutes are periodically pruned. |
| `m_removed_txs_start_time` | `time_t` | Earliest time tracked in `m_removed_txs_by_time`. Set to 0 when tracking has not started or was reset. |
| `m_remove_stuck_tx_interval` | `once_a_time_seconds<30>` | Timer that triggers `remove_stuck_transactions()` at most once every 30 seconds via `on_idle()`. |
| `m_next_check` | `atomic<time_t>` | Next timestamp at which `get_relayable_transactions()` is allowed to perform a DB scan. Prevents excessive DB reads (minimum 2-minute interval). |

### `tx_details` (nested struct in `tx_memory_pool`)

Returned by `get_transaction_info()` and `get_pool_info()`. Contains a complete snapshot of a single pooled transaction.

| Field | Type | Description |
|-------|------|-------------|
| `tx` | `transaction` | The parsed transaction object. |
| `tx_blob` | `blobdata` | The serialized transaction blob (only populated when `include_blob` is true). |
| `blob_size` | `size_t` | Size of the serialized blob in bytes. |
| `weight` | `size_t` | Transaction weight (may differ from blob size due to RingCT padding). |
| `fee` | `uint64_t` | Transaction fee in atomic units. |
| `max_used_block_id` | `hash` | Hash of the highest block referenced by an input. |
| `max_used_block_height` | `uint64_t` | Height of the highest block referenced by an input. |
| `kept_by_block` | `bool` | True if the tx was returned to pool from a popped block (reorg). |
| `last_failed_height` | `uint64_t` | Block height at which input validation last failed. |
| `last_failed_id` | `hash` | Block hash at which input validation last failed. |
| `receive_time` | `time_t` | Time the transaction entered the pool (redacted to 0 in non-sensitive mode). |
| `last_relayed_time` | `time_t` | Last relay timestamp (redacted for Dandelion++ stem txes or non-sensitive mode). |
| `relayed` | `bool` | Whether the transaction has been relayed to the network. |
| `do_not_relay` | `bool` | If true, transaction should not be relayed (set via RPC). |
| `double_spend_seen` | `bool` | True if another transaction spending the same key images was observed. |

### `txpool_tx_meta_t` (defined in `blockchain_db.h`)

A 192-byte packed struct persisted in the LMDB database for each pooled transaction. Layout-sensitive (a `static_assert` enforces exact size for DB migration safety).

Key fields beyond those mirrored in `tx_details`: `dandelionpp_stem` (1 bit, true if in D++ stem phase), `is_local` (1 bit, received via local RPC), `is_forwarding` (1 bit, in forward-delay state), `pruned` (1 bit, tx blob is pruned), `valid_input_verification_id` (hash, cached verification ID at offset 160).

Methods: `set_relay_method()`, `get_relay_method()`, `upgrade_relay_method()` (returns true if method was actually upgraded), `matches(relay_category)`.

### `relay_method` (enum, `enums.h`)

Tracks how a transaction was received and should be relayed:

| Value | Meaning |
|-------|---------|
| `none` (0) | Received via RPC with `do_not_relay` set. |
| `local` | Received via RPC; attempting send over i2p/tor. |
| `forward` | Received over i2p/tor; timer-delayed before public broadcast. |
| `stem` | Dandelion++ stem phase (private relay to a single peer). |
| `fluff` | Dandelion++ fluff phase (broadcast to all peers). |
| `block` | Received as part of a block (highest priority, never demoted). |

### `relay_category` (enum, `blockchain_db.h`)

Filtering categories for pool queries:

| Value | Meaning |
|-------|---------|
| `broadcasted` | Only public txes (received via `block` or `fluff`). |
| `relayable` | Every tx not marked `relay_method::none`. |
| `legacy` | `broadcasted` plus `relay_method::none` (for historical/RPC compatibility). |
| `all` | Everything in the DB, including stem, local, and forward txes. |

### `sorted_tx_container` (type alias)

```cpp
boost::bimap<
  boost::bimaps::multiset_of<std::pair<double, std::time_t>, txFeeCompare>,
  boost::bimaps::set_of<crypto::hash, hashCompare>
>
```

A Boost bimap providing:
- Left view: transactions sorted by `(fee_per_byte DESC, receive_time ASC)` -- used by `fill_block_template` to iterate highest-paying transactions first, and by `prune` to remove lowest-paying transactions from the end.
- Right view: lookup by transaction hash -- used by `find_tx_in_sorted_container`.

### `removed_tx_info` (nested struct)

| Field | Type | Description |
|-------|------|-------------|
| `txid` | `hash` | Hash of the removed transaction. |
| `sensitive` | `bool` | True if the tx was non-broadcasted (stem/local/forward). Used to filter incremental removal reports. |

## Public API

### Transaction Addition and Removal

#### `add_tx` (full signature)
```cpp
bool add_tx(transaction &tx, const crypto::hash &id, const cryptonote::blobdata &blob,
  size_t tx_weight, tx_verification_context& tvc, relay_method tx_relay, bool relayed,
  uint8_t version, uint8_t nic_verified_hf_version = 0,
  const crypto::hash &valid_input_verification_id = crypto::null_hash);
```
Primary entry point for adding a transaction to the pool. Performs the following checks in order: (1) rejects previously timed-out txes unless `kept_by_block`; (2) non-input consensus verification (skipped if `nic_verified_hf_version == version`); (3) fee adequacy check (skipped for `kept_by_block`); (4) tx_extra size limit (max 1060 bytes); (5) nonzero unlock_time rejection; (6) double-spend key image check (skipped for `kept_by_block`); (7) input validation via `check_tx_inputs`. On success, writes tx metadata and blob to DB, inserts key images, updates transient lists, increments cookie, and triggers pruning. Sets `tvc.m_added_to_pool` on success or `tvc.m_verifivation_impossible` if inputs failed but tx is `kept_by_block`. Handles Dandelion++ loop detection (stem tx seen again is upgraded to fluff). Returns false on any verification failure.

**Preconditions:** Caller must hold `m_transactions_lock` (enforced by `CRITICAL_REGION_LOCAL`).

#### `add_tx` (simplified signature)
```cpp
bool add_tx(transaction &tx, tx_verification_context& tvc, relay_method tx_relay, bool relayed,
  uint8_t version, uint8_t nic_verified_hf_version = 0,
  const crypto::hash &valid_input_verification_id = crypto::null_hash);
```
Convenience wrapper that computes the transaction hash, blob, and weight, then delegates to the full `add_tx`. Returns false if serialization or hashing fails.

#### `take_tx`
```cpp
bool take_tx(const crypto::hash &id, transaction &tx, cryptonote::blobdata &txblob,
  size_t& tx_weight, uint64_t& fee, crypto::hash &valid_input_verification_id,
  bool &relayed, bool &do_not_relay, bool &double_spend_seen, bool &pruned,
  bool suppress_missing_msgs = false);
```
Atomically removes a transaction from the pool and returns its details. Used during block application and pool re-validation. Removes the tx from DB, reduces pool weight, removes key images, removes from transient lists, and increments cookie. Returns false if the tx is not found.

### Query Methods

#### `have_tx`
```cpp
bool have_tx(const crypto::hash &id, relay_category tx_category) const;
```
Returns true if a transaction with the given hash exists in the pool and matches the specified relay category. Delegates to `BlockchainDB::txpool_has_tx`.

#### `get_transaction`
```cpp
bool get_transaction(const crypto::hash& h, cryptonote::blobdata& txblob, relay_category tx_category) const;
```
Retrieves a transaction blob by hash. Returns false if not found or does not match the relay category.

#### `get_transaction_info`
```cpp
bool get_transaction_info(const crypto::hash &txid, tx_details &td,
  bool include_sensitive_data, bool include_blob = false) const;
```
Populates a `tx_details` struct for a single transaction. When `include_sensitive_data` is false, `receive_time` is zeroed and stem-phase txes are excluded entirely. The `include_blob` parameter controls whether `td.tx_blob` is populated. Returns false if the tx is not found or is filtered by sensitivity.

#### `get_transactions_info`
```cpp
bool get_transactions_info(const std::vector<crypto::hash>& txids,
  std::vector<std::pair<crypto::hash, tx_details>>& txs, bool include_sensitive_data = false) const;
```
Batch version of `get_transaction_info`. Skips txes that are not found. Always returns true.

#### `get_transactions`
```cpp
void get_transactions(std::vector<transaction>& txs, bool include_sensitive = false) const;
```
Returns all parsed transaction objects in the pool. Filters by `relay_category::broadcasted` unless `include_sensitive` is true.

#### `get_transaction_hashes`
```cpp
void get_transaction_hashes(std::vector<crypto::hash>& txs, bool include_sensitive = false) const;
```
Returns all transaction hashes in the pool. Same sensitivity filtering as `get_transactions`.

#### `get_transactions_count`
```cpp
size_t get_transactions_count(bool include_sensitive = false) const;
```
Returns the total number of transactions in the pool, filtered by sensitivity.

#### `get_transaction_backlog`
```cpp
void get_transaction_backlog(std::vector<tx_backlog_entry>& backlog, bool include_sensitive = false) const;
```
Returns `(weight, fee, time_in_pool)` tuples for all transactions. Time is computed as `receive_time - now` (negative value representing seconds ago).

#### `get_block_template_backlog`
```cpp
void get_block_template_backlog(std::vector<tx_block_template_backlog_entry>& backlog,
  bool include_sensitive = false) const;
```
Returns `(id, weight, fee)` for transactions eligible for block inclusion. Limits output to 112.5% of the current median block weight. Sorts by fee/weight ratio when total exceeds the limit. Filters out transactions that are not ready to go (`is_transaction_ready_to_go`) or have conflicting key images.

#### `get_transaction_stats`
```cpp
void get_transaction_stats(struct txpool_stats& stats, bool include_sensitive = false) const;
```
Computes aggregate statistics: total bytes, min/max/median weight, total fees, oldest tx, count of failing/unrelayed/double-spend txes, count older than 10 minutes, and a 10-bin age histogram (with the 98th percentile separated).

#### `get_transactions_and_spent_keys_info`
```cpp
bool get_transactions_and_spent_keys_info(std::vector<tx_info>& tx_infos,
  std::vector<spent_key_image_info>& key_image_infos, bool include_sensitive_data = false) const;
```
Full pool dump for RPC. Returns parsed tx info plus all spent key images with their associated tx hashes. Sensitivity filtering applies to both tx visibility and field redaction (`receive_time`, `last_relayed_time`).

#### `get_pool_for_rpc`
```cpp
bool get_pool_for_rpc(std::vector<cryptonote::rpc::tx_in_pool>& tx_infos,
  cryptonote::rpc::key_images_with_tx_hashes& key_image_infos) const;
```
Similar to `get_transactions_and_spent_keys_info` but uses the `rpc::tx_in_pool` structure. Always filters to `relay_category::broadcasted` for txes.

#### `check_for_key_images`
```cpp
bool check_for_key_images(const std::vector<crypto::key_image>& key_images,
  std::vector<bool>& spent) const;
```
For each key image, checks whether it is spent by any broadcasted transaction in the pool. Returns a parallel boolean vector. Always returns true.

#### `get_complement`
```cpp
bool get_complement(const std::vector<crypto::hash> &hashes,
  std::vector<cryptonote::blobdata> &txes) const;
```
Returns blobs for all broadcasted pool transactions whose hashes are NOT in the provided set. Used for compact block relay to send transactions the peer is missing.

#### `get_pool_info`
```cpp
bool get_pool_info(time_t start_time, bool include_sensitive, size_t max_tx_count,
  std::vector<std::pair<crypto::hash, tx_details>>& added_txs,
  std::vector<crypto::hash>& remaining_added_txids,
  std::vector<crypto::hash>& removed_txs, bool& incremental) const;
```
Supports incremental pool synchronization for wallets. If `start_time == 0` or tracking data does not extend far enough back, returns the entire pool (non-incremental). Otherwise, returns only txes added since `start_time` and txes removed since `start_time`. If the number of added txes exceeds `max_tx_count`, the excess is returned as hash-only in `remaining_added_txids`.

### Relay Management

#### `get_relayable_transactions`
```cpp
bool get_relayable_transactions(
  std::vector<std::tuple<crypto::hash, cryptonote::blobdata, relay_method>>& txs);
```
Collects transactions eligible for relay: must have nonzero fee, not be pruned, not be `do_not_relay`, not be `relay_method::none`, and not have been relayed too recently (backoff delay between `MIN_RELAY_TIME` = 5 min and `MAX_RELAY_TIME` = 4 hours). Transactions older than half their max lifetime are skipped to avoid relay-flush oscillation. For `stem`/`forward` txes whose embargo timer has not expired, they are skipped. Rate-limited to at most one DB scan every 2 minutes via `m_next_check`. Returns true if DB was checked, false if skipped.

#### `set_relayed`
```cpp
void set_relayed(epee::span<const crypto::hash> hashes, relay_method tx_relay,
  std::vector<bool> &just_broadcasted);
```
Called after transactions are relayed. Updates `meta.relayed`, upgrades relay method, and sets `last_relayed_time`. For Dandelion++ stem txes, `last_relayed_time` is set to a future embargo deadline (Poisson-distributed around 39 seconds). For fluff/block txes, set to current time. `just_broadcasted[i]` is true if the i-th tx transitioned from non-broadcasted to broadcasted state (used to trigger txpool event notifications).

### Block Template Construction

#### `fill_block_template`
```cpp
bool fill_block_template(block &bl, size_t median_weight, uint64_t already_generated_coins,
  size_t &total_weight, uint64_t &fee, uint64_t &expected_reward, uint8_t version);
```
Selects transactions for a new block. Iterates `m_txs_by_fee_and_receive_time` (highest fee/byte first). For each candidate: skips non-legacy txes (unless `m_mine_stem_txes`), skips pruned txes, enforces max block weight (pre-v5: 130% of median; v5+: 200% of median, minus coinbase reservation of 600 bytes), and for v5+ checks that adding the tx does not reduce the total coinbase below the acceptance threshold (100% of the best so far). Validates inputs via `is_transaction_ready_to_go` and checks for key image conflicts among already-selected txes. Returns total weight, total fees, and expected reward.

### Lifecycle

#### `init`
```cpp
bool init(size_t max_txpool_weight = 0, bool mine_stem_txes = false);
```
Loads pool state from the database. Clears all in-memory structures, then iterates all DB txpool entries in two passes (non-`kept_by_block` first, then `kept_by_block`) to rebuild key images and the sorted container. Removes unparseable transactions. Sets `m_mine_stem_txes` and resets the cookie. Always returns true unless a fatal key image insertion error occurs.

#### `deinit`
```cpp
bool deinit();
```
Currently a no-op that returns true. Pool state is persisted in the LMDB database, so no explicit save is needed.

### Pool Maintenance

#### `on_idle`
```cpp
void on_idle();
```
Called periodically by the core. Delegates to `remove_stuck_transactions()` at most once every 30 seconds.

#### `on_blockchain_inc` / `on_blockchain_dec`
```cpp
bool on_blockchain_inc(uint64_t new_block_height, const crypto::hash& top_block_id);
bool on_blockchain_dec(uint64_t new_block_height, const crypto::hash& top_block_id);
```
Called when a block is added to or removed from the main chain. Both clear `m_input_cache` and `m_parsed_tx_cache` to invalidate stale verification results. Always return true.

#### `validate`
```cpp
size_t validate(uint8_t version);
```
Re-validates all non-pruned transactions against a new hard fork version. Takes each tx out via `take_tx` and re-adds it via `add_tx`. Transactions that fail re-validation are dropped. Returns the count of removed transactions. Resets incremental tracking (`m_added_txs_by_id`, `m_removed_txs_by_time`).

#### `prune`
```cpp
void prune(size_t bytes = 0);
```
See Internal Logic section.

### Weight Management

#### `get_txpool_weight`
```cpp
size_t get_txpool_weight() const;
```
Returns current cumulative pool weight in bytes.

#### `set_txpool_max_weight`
```cpp
void set_txpool_max_weight(size_t bytes);
```
Sets the maximum pool weight. Takes effect on the next `prune` call.

#### `reduce_txpool_weight`
```cpp
void reduce_txpool_weight(size_t weight);
```
Subtracts the given weight from `m_txpool_weight`. If the subtraction would underflow, clamps to 0 and logs an error.

### Utility

#### `print_pool`
```cpp
std::string print_pool(bool short_format) const;
```
Returns a human-readable string dump of all pool transactions (including sensitive ones). In short format, omits the full JSON and blob size.

#### `cookie`
```cpp
uint64_t cookie() const;
```
Returns the current mutation counter. Callers can compare with a previous value to detect pool changes.

#### `lock` / `unlock`
```cpp
void lock() const;
void unlock() const;
```
Expose the pool's critical section for external locking (used by `Blockchain` when it needs atomic operations spanning both pool and chain).

## Internal Logic

### Transaction Addition Flow (`add_tx`)

1. Acquire `m_transactions_lock`.
2. Reject if txid is in `m_timed_out_transactions` (unless `kept_by_block`).
3. Run non-input consensus checks (`ver_non_input_consensus`) unless the caller certifies the tx already passed for this hard fork version.
4. Compute fee via `get_tx_fee()`. For non-`kept_by_block` txes, verify fee meets minimum via `Blockchain::check_fee()`.
5. Reject if `tx.extra` exceeds `MAX_TX_EXTRA_SIZE` (1060 bytes).
6. Reject if `tx.unlock_time` is nonzero.
7. For non-`kept_by_block`: check if any key images are already spent in the pool. If so, mark existing txes as double-spend and reject.
8. Validate inputs via `check_tx_inputs()` (which caches results in `m_input_cache`).
9. If input validation fails but `kept_by_block`, store the tx anyway with `last_failed_height = 0` and set `tvc.m_verifivation_impossible`.
10. If input validation succeeds, check for existing tx in DB (Dandelion++ loop detection: if a stem tx is seen again as stem, upgrade to fluff). Write metadata to DB, insert key images, add to transient lists.
11. Update `m_txpool_weight`, increment `m_cookie`, call `prune(m_txpool_max_weight)`.

### Pruning Algorithm (`prune`)

Removes lowest fee-per-byte transactions until `m_txpool_weight <= bytes`:

1. Start from the end of `m_txs_by_fee_and_receive_time` (lowest fee/byte).
2. Skip `kept_by_block` transactions (they are from reorgs and should be preserved).
3. For each candidate: remove from DB, reduce pool weight, remove key images, remove from transient lists.
4. Iterate backward toward higher-fee transactions until weight is within budget.
5. Note: the very first entry (highest fee/byte) is never removed due to the loop guard (`it != begin()`).

### Stuck Transaction Removal (`remove_stuck_transactions`)

Called every 30 seconds via `on_idle()`:

1. Iterate all pool txes.
2. Remove if: age > `CRYPTONOTE_MEMPOOL_TX_LIVETIME` (3 days) and not `kept_by_block`, OR age > `CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME` (7 days) and `kept_by_block`.
3. Add txid to `m_timed_out_transactions` to prevent re-acceptance.
4. Remove from DB, key images, and transient lists.

### Dandelion++ Relay State Machine

Transactions progress through relay states managed by `relay_method` and the metadata bit fields:

1. **Local origin** (`relay_method::local`): Tx submitted via RPC. Sent over i2p/tor. Not visible to non-sensitive queries.
2. **Forward delay** (`relay_method::forward`): Tx received over i2p/tor. `last_relayed_time` is set to a future time (Poisson-distributed with mean `CRYPTONOTE_FORWARD_DELAY_AVERAGE` = 22.5 seconds). The tx is held until this timer expires before being relayed over public networks.
3. **Stem phase** (`relay_method::stem`): Tx is relayed to a single stem peer. `dandelionpp_stem = true`. `last_relayed_time` is set to a future embargo deadline (Poisson-distributed with mean 39 seconds). If the embargo expires before fluffing, the node fluffs it.
4. **Fluff phase** (`relay_method::fluff`): Tx is broadcast to all peers. `dandelionpp_stem = false`. `last_relayed_time` is the actual relay timestamp.
5. **Block** (`relay_method::block`): Tx came from a popped block during reorg. Highest priority; never demoted.

**Loop detection:** If a stem tx is seen again as stem by the same node, `upgrade_relay_method` switches it to fluff (line 296). Local-state txes do not trigger loop detection since they represent the node's own outgoing transactions.

**Relay timing:** `get_relayable_transactions()` uses an increasing backoff for re-relay: `d = ((last_relay - receive_time + 5min) / 5min) * 5min`, capped at 4 hours. Transactions older than half their max lifetime are never re-relayed to avoid relay-flush oscillation.

### Block Template Filling (`fill_block_template`)

1. Compute baseline empty-block reward.
2. Calculate `max_total_weight`: pre-v5 = 130% of median - 600; v5+ = 200% of median - 600.
3. Iterate `m_txs_by_fee_and_receive_time` (highest fee/byte first).
4. Skip: non-legacy/non-stem txes, pruned txes, txes that would exceed max weight.
5. For v5+: compute new coinbase if this tx were included. Skip if it drops below `ACCEPT_THRESHOLD` (100%) of the current best coinbase (the optimal filling algorithm ensures block reward is maximized despite the quadratic penalty).
6. For pre-v5: stop if total weight exceeds median (penalty-free zone).
7. Validate tx inputs via `is_transaction_ready_to_go()`. Update `last_failed_height`/`last_failed_id` on failure.
8. Check for key image conflicts with already-selected transactions.
9. Append to `bl.tx_hashes`, accumulate weight and fees.

### `is_transaction_ready_to_go`

Uses lazy parsing: the transaction blob is only parsed if `check_tx_inputs` actually needs the parsed transaction (deferred via a `transaction_parser` functor).

1. If `last_failed_id` matches the current top block hash, return false immediately (known to fail at this chain tip).
2. Call `check_tx_inputs()` (which may use `m_input_cache`).
3. On failure, record `last_failed_height` and `last_failed_id`.

### Incremental Pool Info (`get_pool_info`)

Supports wallet polling for pool changes:

1. If `start_time == 0`, return the entire pool as "added" transactions (non-incremental).
2. If either `m_added_txs_start_time` or `m_removed_txs_start_time` does not cover `start_time`, fall back to non-incremental.
3. Otherwise, return txes from `m_added_txs_by_id` with timestamps >= `start_time`, and removed txids from `m_removed_txs_by_time` with timestamps >= `start_time`.
4. If added txes exceed `max_tx_count`, the overflow is returned as hash-only in `remaining_added_txids`.

### Removed Transaction Tracking (`track_removed_tx`)

Maintains `m_removed_txs_by_time` with two cleanup strategies:

- **Size-based:** If the map exceeds 20000 entries, erase the oldest 25%.
- **Time-based:** Otherwise, erase entries older than 30 minutes.

## Dependencies

### This module depends on:

| Dependency | Usage |
|------------|-------|
| `Blockchain` (cryptonote_core) | Input validation (`check_tx_inputs`), fee checking (`check_fee`), block reward calculation (`get_block_reward`), DB access for txpool operations (`add_txpool_tx`, `remove_txpool_tx`, `get_txpool_tx_meta`, `for_all_txpool_txes`, etc.), chain height queries. |
| `BlockchainDB` (blockchain_db) | LMDB storage backend for pool metadata and transaction blobs. Accessed through `Blockchain` reference. Uses `LockedTXN` for atomic multi-operation transactions. |
| `cryptonote_basic` | Transaction parsing (`parse_and_validate_tx_from_blob`), hashing (`get_transaction_hash`), weight calculation (`get_transaction_weight`), fee extraction (`get_tx_fee`). |
| `tx_verification_utils` | `ver_non_input_consensus()` for non-input consensus rule checking. |
| `cryptonote_tx_utils` | `get_block_reward()` for coinbase computation during block template filling. |
| `cryptonote_config.h` | All timing and size constants (mempool lifetimes, Dandelion++ parameters, max extra size, default pool weight). |
| `cryptonote_protocol/enums.h` | `relay_method` enum. |
| `epee` | `critical_section` for locking, `math_helper::once_a_time_seconds` for periodic callbacks, `string_tools` for hex conversion. |
| `crypto` | `hash`, `key_image`, `random_poisson_seconds` for Dandelion++ timer generation. |

### What depends on this module:

| Dependent | Usage |
|-----------|-------|
| `cryptonote_core` | Owns the pool via `BlockchainAndPool`. Calls `add_tx` for incoming transactions, `fill_block_template` for mining, `get_relayable_transactions`/`set_relayed` for relay, `on_idle` for maintenance, `validate` on hard fork transitions, and all query methods for RPC. |
| `Blockchain` | Takes a reference to `tx_memory_pool` in its constructor. Calls pool lock/unlock during block addition/removal. Calls `on_blockchain_inc`/`on_blockchain_dec`. |
| `core_rpc_server` | Uses query methods (`get_transactions_and_spent_keys_info`, `get_pool_for_rpc`, `check_for_key_images`, `get_transaction_stats`, etc.) to serve RPC endpoints. |
| `levin_notify` / P2P layer | Receives relay lists from `get_relayable_transactions` and reports back via `set_relayed`. |
| Wallet RPC / Light wallet | Uses `get_pool_info` for incremental pool synchronization. |

## Configuration

### Command-Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--max-txpool-weight` | `648000000` (648 MB) | Maximum cumulative transaction pool weight in bytes. Defined in `cryptonote_core.cpp` with `arg_max_txpool_weight`. Passed to `tx_memory_pool::init()`. |

### Compile-Time Constants (from `cryptonote_config.h`)

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_TXPOOL_MAX_WEIGHT` | `648000000` | Default max pool weight (~3 days at 300KB median blocks). |
| `CRYPTONOTE_MEMPOOL_TX_LIVETIME` | `259200` (3 days) | Maximum age of a normal transaction before removal. |
| `CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME` | `604800` (7 days) | Maximum age of a `kept_by_block` transaction before removal. |
| `MAX_TX_EXTRA_SIZE` | `1060` | Maximum allowed `tx.extra` field size in bytes. |
| `CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE` | `39` | Mean embargo duration for Dandelion++ stem phase (seconds). |
| `CRYPTONOTE_FORWARD_DELAY_BASE` | `15` | Base delay for forwarding from i2p/tor to public (seconds). Computed as `NOISE_MIN_DELAY + NOISE_DELAY_RANGE`. |
| `CRYPTONOTE_FORWARD_DELAY_AVERAGE` | `22` | Mean forward delay (seconds). Computed as `FORWARD_DELAY_BASE + FORWARD_DELAY_BASE / 2`. |
| `CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE` | `600` | Bytes reserved for coinbase in block weight calculations. |

### Internal Constants (in `tx_pool.cpp` anonymous namespace)

| Constant | Value | Description |
|----------|-------|-------------|
| `MIN_RELAY_TIME` | `300` (5 min) | Minimum interval before a transaction can be re-relayed. |
| `MAX_RELAY_TIME` | `14400` (4 hours) | Maximum interval between re-relays (backoff ceiling). |
| `ACCEPT_THRESHOLD` | `1.0` | Multiplier for block template coinbase acceptance (100% -- tx is only added if it does not decrease coinbase). |
| `max_relayable_check` | `2 min` | Minimum interval between DB scans in `get_relayable_transactions`. |

### Compile Flags

| Flag | Effect |
|------|--------|
| `DEBUG_CREATE_BLOCK_TEMPLATE` | Makes `m_transactions_lock` public for direct access in tests/debugging of `fill_block_template`. |

## Known Issues

The following TODO/FIXME/HACK/XXX comments are present in the source files:

| Location | Comment |
|----------|---------|
| `tx_pool.cpp:87` | `TODO: constants such as these should at least be in the header, but probably somewhere more accessible to the rest of the codebase.` -- Refers to `MIN_RELAY_TIME`, `MAX_RELAY_TIME`, `ACCEPT_THRESHOLD` being hard-coded in the anonymous namespace. |
| `tx_pool.cpp:206` | `TODO: Investigate why not?` -- Questions why key image double-spend checks are skipped for `kept_by_block` transactions. |
| `tx_pool.cpp:504` | `FIXME: Can return early before removal of all of the key images. At the least, need to make sure that a false return here is treated properly. Should probably not return early, however.` -- `remove_transaction_keyimages()` may leave partial state if it returns false mid-iteration. |
| `tx_pool.cpp:730` | `TODO: investigate whether boolean return is appropriate` -- `remove_stuck_transactions()` always returns true; boolean return may be misleading. |
| `tx_pool.cpp:785` | `TODO: investigate whether boolean return is appropriate` -- `get_relayable_transactions()` return value semantics (true = DB checked, false = skipped) are unusual. |
| `tx_pool.cpp:1190` | `TODO: investigate whether boolean return is appropriate` -- `get_transactions_and_spent_keys_info()` always returns true. |
| `tx_pool.cpp:1567` | `TODO: investigate whether boolean return is appropriate` -- `fill_block_template()` always returns true (except for the degenerate `get_block_reward` failure). |
| `tx_pool.h:631` | `TODO: confirm the below comments and investigate whether or not this is the desired behavior` -- Questions whether multiple transactions per key image (in `m_spent_key_images`) is intended behavior. |
| `tx_pool.h:653` | `TODO: this time should be a named constant somewhere, not hard-coded` -- The 30-second interval for `m_remove_stuck_tx_interval` is a template parameter, not a named constant. |
| `tx_pool.h:657` | `TODO: look into doing this better` -- Comment on `m_txs_by_fee_and_receive_time` suggesting the sorted container could be improved. |
