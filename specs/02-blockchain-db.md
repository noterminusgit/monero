# Blockchain Database

## Overview

The blockchain database module (`blockchain_db`) provides the persistent storage layer for the Monero blockchain. It defines an abstract interface (`BlockchainDB`) that decouples the core blockchain logic from any specific storage engine, and ships a single concrete implementation backed by LMDB (Lightning Memory-Mapped Database). The module stores blocks, transactions (split into pruned and prunable parts), transaction outputs, spent key images, transaction pool metadata, alternative chain blocks, and hard fork versioning data. It supports batch write transactions for high-throughput block import, automatic memory-map resizing, blockchain pruning, and schema migrations across five DB versions.

## Key Files

| File | Lines | Description |
|------|------:|-------------|
| `src/blockchain_db/blockchain_db.h` | 1903 | Abstract `BlockchainDB` base class, data structures (`output_data_t`, `tx_data_t`, `txpool_tx_meta_t`, `alt_block_data_t`), exception hierarchy, relay category/method enums, `db_txn_guard` RAII helpers, and the factory function `new_db()`. |
| `src/blockchain_db/blockchain_db.cpp` | 510 | Concrete implementations of base class methods: `add_block`, `pop_block`, `add_transaction`, `remove_transaction`, `get_block`, `get_tx`, `fixup`, relay method matching, command-line argument definitions, and performance statistics. |
| `src/blockchain_db/lmdb/db_lmdb.h` | 502 | `BlockchainLMDB` class declaration, LMDB-specific structs (`mdb_txn_cursors`, `mdb_rflags`, `mdb_threadinfo`, `mdb_txn_safe`), cursor accessor macros, and all MDB_dbi handle members. |
| `src/blockchain_db/lmdb/db_lmdb.cpp` | 5730 | Full LMDB implementation: database open/close, all CRUD operations for blocks/transactions/outputs/spent keys/txpool/alt blocks, batch transaction management, pruning, auto-resize logic, output histogram/distribution, hard fork version storage, and five schema migration functions (v0 through v5). |
| `src/blockchain_db/locked_txn.h` | 55 | `LockedTXN` RAII wrapper that starts a batch transaction and commits or aborts on scope exit. Used with the transaction pool lock. |
| `src/blockchain_db/testdb.h` | -- | Stub implementation for unit testing (not analyzed in depth). |
| `src/blockchain_db/CMakeLists.txt` | -- | Build definition for the `blockchain_db` library target. |

## Data Structures

### output_data_t (blockchain_db.h:124-130)

Packed struct (1-byte alignment) holding per-output metadata stored in the `output_amounts` table.

| Field | Type | Description |
|-------|------|-------------|
| `pubkey` | `crypto::public_key` | Output's public key for spend verification |
| `unlock_time` | `uint64_t` | Unlock time or height |
| `height` | `uint64_t` | Height of the block that created this output |
| `commitment` | `rct::key` | RingCT commitment (only present for amount-0 / RCT outputs) |

**Invariant:** For pre-RCT outputs (non-zero amount), only the first three fields are stored on disk (as `pre_rct_output_data_t`); the commitment is computed on read via `rct::zeroCommit(amount)`.

### tx_data_t (blockchain_db.h:134-139)

Packed struct stored as the value component of `txindex` entries in the `tx_indices` table.

| Field | Type | Description |
|-------|------|-------------|
| `tx_id` | `uint64_t` | Sequential transaction ID (used as key in pruned/prunable tables) |
| `unlock_time` | `uint64_t` | Transaction unlock time |
| `block_id` | `uint64_t` | Height of the containing block |

### txpool_tx_meta_t (blockchain_db.h:154-193)

Fixed 192-byte struct for transaction pool metadata. Stored directly as the value in the `txpool_meta` LMDB table.

| Field | Type | Description |
|-------|------|-------------|
| `max_used_block_id` | `crypto::hash` | Hash of the highest block referenced by tx inputs |
| `last_failed_id` | `crypto::hash` | Hash of block where last validation failure occurred |
| `weight` | `uint64_t` | Transaction weight |
| `fee` | `uint64_t` | Transaction fee |
| `max_used_block_height` | `uint64_t` | Height of highest block referenced by inputs |
| `last_failed_height` | `uint64_t` | Height where last validation failure occurred |
| `receive_time` | `uint64_t` | When the transaction was received |
| `last_relayed_time` | `uint64_t` | Last relay timestamp (semantics vary by relay method) |
| `kept_by_block` | `uint8_t` | 1 if relay method is `block` |
| `relayed` | `uint8_t` | Whether the tx has been relayed |
| `do_not_relay` | `uint8_t` | 1 if relay method is `none` |
| `double_spend_seen` | `uint8_t:1` | Bitfield: double spend detected |
| `pruned` | `uint8_t:1` | Bitfield: tx is pruned |
| `is_local` | `uint8_t:1` | Bitfield: relay method is `local` |
| `dandelionpp_stem` | `uint8_t:1` | Bitfield: relay method is `stem` (Dandelion++) |
| `is_forwarding` | `uint8_t:1` | Bitfield: relay method is `forward` |
| `bf_padding` | `uint8_t:3` | Padding bits |
| `padding[44]` | `uint8_t[]` | Reserved padding to reach 160 bytes |
| `valid_input_verification_id` | `crypto::hash` | Verification ID for cached ring proof validity |

**Invariant:** `sizeof(txpool_tx_meta_t) == 192` (enforced by `static_assert`). The relay method is encoded across five bitfields (`kept_by_block`, `do_not_relay`, `is_local`, `is_forwarding`, `dandelionpp_stem`) where exactly one may be set at a time; the default (all zero) maps to `relay_method::fluff`.

### alt_block_data_t (blockchain_db.h:142-149)

Metadata for alternative chain blocks.

| Field | Type | Description |
|-------|------|-------------|
| `height` | `uint64_t` | Block height in the alternative chain |
| `cumulative_weight` | `uint64_t` | Cumulative weight up to this alt block |
| `cumulative_difficulty_low` | `uint64_t` | Low 64 bits of cumulative difficulty |
| `cumulative_difficulty_high` | `uint64_t` | High 64 bits of cumulative difficulty |
| `already_generated_coins` | `uint64_t` | Total coins generated up to this block |

### mdb_block_info (db_lmdb.cpp:320-333, typedef as mdb_block_info_4)

The current (version 4/5) on-disk block metadata record stored in the `block_info` DUPFIXED table.

| Field | Type | Description |
|-------|------|-------------|
| `bi_height` | `uint64_t` | Block height (also serves as DUPSORT key) |
| `bi_timestamp` | `uint64_t` | Block timestamp |
| `bi_coins` | `uint64_t` | Cumulative coins generated |
| `bi_weight` | `uint64_t` | Block weight (stored as uint64 for 32-bit compat) |
| `bi_diff_lo` | `uint64_t` | Low 64 bits of cumulative difficulty |
| `bi_diff_hi` | `uint64_t` | High 64 bits of cumulative difficulty |
| `bi_hash` | `crypto::hash` | Block hash |
| `bi_cum_rct` | `uint64_t` | Cumulative RCT output count |
| `bi_long_term_block_weight` | `uint64_t` | Long-term block weight |

Previous versions of this struct (`mdb_block_info_1` through `mdb_block_info_3`) had fewer fields and different difficulty representations, handled by schema migrations.

### txindex (db_lmdb.h:43-46)

Composite key/data struct used in the `tx_indices` DUPFIXED table.

| Field | Type | Description |
|-------|------|-------------|
| `key` | `crypto::hash` | Transaction hash (serves as DUPSORT key) |
| `data` | `tx_data_t` | Transaction metadata (tx_id, unlock_time, block_id) |

### outkey / pre_rct_outkey (db_lmdb.cpp:340-350)

Records stored in the `output_amounts` DUPFIXED table. For zero-amount (RCT) outputs, `outkey` (with commitment) is stored; for non-zero amounts, the shorter `pre_rct_outkey` (without commitment) is stored, saving 32 bytes per record.

### outtx (db_lmdb.cpp:352-356)

Records stored in the `output_txs` DUPFIXED table, mapping global output ID to transaction hash and local index.

### mdb_txn_safe (db_lmdb.h:128-167)

RAII wrapper for LMDB transactions providing:
- Automatic abort in destructor if not committed.
- Global active transaction counting via `static std::atomic<uint64_t> num_active_txns`.
- A creation gate (`static std::atomic_flag creation_gate`) to serialize transaction creation.
- Static methods `prevent_new_txns()`, `wait_no_active_txns()`, `allow_new_txns()` for coordinating DB resizes.
- A `m_batch_txn` flag to distinguish batch transactions that must be aborted before `mdb_env_close`.

### mdb_threadinfo (db_lmdb.h:119-126)

Per-thread read transaction state, stored in a `boost::thread_specific_ptr`. Contains a read-only MDB_txn, a set of cursors, and flags indicating which cursors are currently valid.

### relay_category / relay_method (blockchain_db.h:109-115 / cryptonote_protocol/enums.h)

`relay_category` is an enum used to filter txpool queries:
- `broadcasted`: Only txes received via block or fluff relay.
- `relayable`: Everything except `relay_method::none`.
- `legacy`: `broadcasted` plus `none` (for RPC relay requests and historical reasons).
- `all`: No filtering.

The `matches_category()` function (blockchain_db.cpp:47-76) implements the category matching logic.

### Exception Hierarchy (blockchain_db.h:210-365)

All exceptions derive from `DB_EXCEPTION` (which extends `std::exception`):

| Exception | Meaning |
|-----------|---------|
| `DB_ERROR` | Generic database error |
| `DB_ERROR_TXN_START` | Error starting a transaction |
| `DB_OPEN_FAILURE` | Failed to open database |
| `DB_CREATE_FAILURE` | Failed to create database |
| `DB_SYNC_FAILURE` | Failed to sync to disk |
| `BLOCK_DNE` | Requested block does not exist |
| `BLOCK_PARENT_DNE` | Block's parent does not exist |
| `BLOCK_EXISTS` | Block already exists (during add) |
| `BLOCK_INVALID` | Block failed validation |
| `TX_DNE` | Transaction does not exist |
| `TX_EXISTS` | Transaction already exists |
| `OUTPUT_DNE` | Output does not exist |
| `OUTPUT_EXISTS` | Output already exists |
| `KEY_IMAGE_EXISTS` | Spent key image already exists |

## Public API

### Database Lifecycle

| Method | Signature | Description |
|--------|-----------|-------------|
| `new_db()` | `BlockchainDB* new_db()` | Factory function. Always returns `new BlockchainLMDB()`. |
| `open` | `virtual void open(const std::string& filename, const int db_flags = 0)` | Opens or creates the database at the given path. Flags: `DBF_SAFE` (1), `DBF_FAST` (2), `DBF_FASTEST` (4), `DBF_RDONLY` (8), `DBF_SALVAGE` (0x10). LMDB maps these to `MDB_NOSYNC`, `MDB_WRITEMAP|MDB_MAPASYNC`, `MDB_RDONLY`, `MDB_PREVSNAPSHOT` respectively. Also triggers schema migration if the DB version is older than current (version 5). Throws `DB_OPEN_FAILURE` on error. |
| `close` | `virtual void close()` | Aborts any active batch, syncs, closes the LMDB environment. |
| `sync` | `virtual void sync()` | Forces `mdb_env_sync` if not read-only. Throws `DB_SYNC_FAILURE`. |
| `safesyncmode` | `virtual void safesyncmode(const bool onoff)` | Toggles `MDB_NOSYNC|MDB_MAPASYNC` flags at runtime. |
| `reset` | `virtual void reset()` | Drops all tables and reinitializes with current version. Destructive. |
| `is_open` | `bool is_open() const` | Returns `m_open` state. |
| `is_read_only` | `virtual bool is_read_only() const` | Checks `MDB_RDONLY` flag on the environment. |
| `get_db_name` | `virtual std::string get_db_name() const` | Returns `"lmdb"`. |
| `get_filenames` | `virtual std::vector<std::string> get_filenames() const` | Returns paths to `data.mdb` and `lock.mdb`. |
| `remove_data_file` | `virtual bool remove_data_file(const std::string& folder) const` | Deletes `data.mdb` from the given folder. |
| `get_database_size` | `virtual uint64_t get_database_size() const` | Returns the file size of `data.mdb`. |

### Block Operations

| Method | Description | Error Behavior |
|--------|-------------|----------------|
| `add_block(blk, block_weight, long_term_block_weight, cumulative_difficulty, coins_generated, txs)` | Adds a block and all its transactions. Computes block hash, adds miner tx and each regular tx via `add_transaction`, stores block blob and metadata, notifies HardFork. Returns the height at which the block was added. Checks every 1024 blocks for resize need. | Throws `std::runtime_error` if tx/hash count mismatch. |
| `pop_block(blk, txs)` | Removes the top block. Retrieves the block, calls `remove_block()`, then removes each transaction (in reverse order) and the miner tx. LMDB wraps this in a write transaction and aborts on exception. | Throws `DB_ERROR` if a transaction cannot be retrieved. |
| `block_exists(h, *height)` | Checks if a block with hash `h` exists. Optionally returns its height. | Returns `false` if not found. |
| `get_block_height(h)` | Returns the height of the block with hash `h`. | Throws `BLOCK_DNE` if not found. |
| `get_block(h)` / `get_block_from_height(height)` | Returns a parsed `block` object. | Throws `BLOCK_DNE` or `DB_ERROR`. |
| `get_block_blob(h)` / `get_block_blob_from_height(height)` | Returns raw serialized block data. | Throws `BLOCK_DNE`. |
| `get_block_header(h)` | Returns block header (auto-cast from `block`). | Throws `BLOCK_DNE`. |
| `height()` | Returns current blockchain height (number of blocks). LMDB implements this via `mdb_stat` on the `m_blocks` table. | Throws `DB_ERROR` on stat failure. |
| `top_block_hash(*block_height)` | Returns the hash of the top block. Returns `null_hash` if chain is empty. | -- |
| `get_top_block()` | Returns the top block. Returns a default-constructed block if chain is empty. | -- |
| `get_blocks_range(h1, h2)` | Returns a vector of blocks from height h1 to h2 inclusive. | Throws `BLOCK_DNE` for out-of-range. |
| `get_hashes_range(h1, h2)` | Returns a vector of block hashes from h1 to h2 inclusive. | Throws `BLOCK_DNE`. |

### Block Metadata Queries

| Method | Description |
|--------|-------------|
| `get_block_timestamp(height)` | Returns `bi_timestamp` from block_info. |
| `get_top_block_timestamp()` | Returns timestamp of top block, or 0 if chain is empty. |
| `get_block_weight(height)` | Returns `bi_weight` from block_info. |
| `get_block_weights(start_height, count)` | Returns vector of weights. Uses optimized `get_block_info_64bit_fields` with `MDB_NEXT_MULTIPLE` for batch reads. |
| `get_block_cumulative_difficulty(height)` | Returns 128-bit difficulty reconstructed from `bi_diff_lo` and `bi_diff_hi`. |
| `get_block_difficulty(height)` | Returns the difference between cumulative difficulties at `height` and `height-1`. |
| `correct_block_cumulative_difficulties(start_height, new_cumulative_difficulties)` | Overwrites cumulative difficulties from `start_height` to chain tip. Used to fix the "difficulty drift" bug. Operates within a write transaction. |
| `get_block_already_generated_coins(height)` | Returns `bi_coins`. |
| `get_block_long_term_weight(height)` | Returns `bi_long_term_block_weight`. |
| `get_long_term_block_weights(start_height, count)` | Batch read of long-term weights. |
| `get_block_hash_from_height(height)` | Returns `bi_hash` from block_info. |
| `get_block_cumulative_rct_outputs(heights)` | Returns cumulative RCT output counts for the given heights. Optimized for sequential access with `MDB_NEXT_MULTIPLE`. |

### Transaction Operations

| Method | Description | Error Behavior |
|--------|-------------|----------------|
| `tx_exists(h)` / `tx_exists(h, tx_id)` | Checks if a transaction exists by hash. The second overload also returns the tx_id. | Returns `false` if not found. |
| `get_tx(h)` / `get_tx(h, tx)` | Returns a full transaction (pruned + prunable). The overload returning `bool` returns `false` if not found; the other throws `TX_DNE`. | `TX_DNE` or `DB_ERROR`. |
| `get_pruned_tx(h)` / `get_pruned_tx(h, tx)` | Returns only the pruned (non-prunable) portion. | `TX_DNE` or `DB_ERROR`. |
| `get_tx_blob(h, bd)` | Returns full tx blob (pruned + prunable concatenated). | Returns `false` if not found. |
| `get_pruned_tx_blob(h, bd)` | Returns only the pruned tx blob. | Returns `false` if not found. |
| `get_pruned_tx_blobs_from(h, count, bd)` | Returns `count` sequential pruned tx blobs starting from tx with hash `h`. | Returns `false` if first tx not found or insufficient count. |
| `get_prunable_tx_blob(h, bd)` | Returns only the prunable portion of a tx. | Returns `false` if not found or pruned away. |
| `get_prunable_tx_hash(tx_hash, prunable_hash)` | Returns the hash of the prunable part (stored separately for v2+ txes). | Returns `false` if not found. |
| `get_tx_count()` | Returns total transaction count via `mdb_stat` on `txs_pruned`. | -- |
| `get_tx_list(hlist)` | Returns transactions for each hash in the list. | Throws `TX_DNE` for missing entries. |
| `get_tx_unlock_time(h)` | Returns the unlock time from the tx index. | Throws `TX_DNE`. |
| `get_tx_block_height(h)` | Returns the block height containing the transaction. | Throws `TX_DNE`. |
| `get_blocks_from(start_height, min_block_count, max_block_count, max_tx_count, max_size, blocks, pruned, get_miner_tx_hash)` | Efficient sequential fetch of blocks and their transactions. Supports pruned mode. Walks cursors linearly through `blocks`, `txs_pruned`, and optionally `txs_prunable` tables. | Throws on DB errors. |
| `get_txids_loose(txid_template, nbits, max_num_txs)` | Finds all txids (chain + pool) matching a partial hash template. Uses `MDB_GET_BOTH_RANGE` for efficient prefix search. | Throws `TX_EXISTS` if matches exceed `max_num_txs`. |

### Output Operations

| Method | Description |
|--------|-------------|
| `get_num_outputs(amount)` | Returns the count of outputs with the given amount. |
| `get_output_key(amount, index, include_commitment)` | Returns `output_data_t` for the output at the given amount-specific index. For pre-RCT outputs, commitment is computed as `rct::zeroCommit(amount)` if `include_commitment` is true. |
| `get_output_key(amounts, offsets, outputs, allow_partial)` | Batch version; `amounts` can be a single value or match `offsets` in size. |
| `get_output_tx_and_index(amount, index)` | Returns `(tx_hash, local_index)` for the output. |
| `get_output_tx_and_index(amount, offsets, indices)` | Batch version of the above. |
| `get_output_tx_and_index_from_global(index)` | Returns `(tx_hash, local_index)` for a global output ID. |
| `get_tx_amount_output_indices(tx_id, n_txes)` | Returns amount output indices for `n_txes` transactions starting at `tx_id`. |
| `get_output_histogram(amounts, unlocked, recent_cutoff, min_count)` | Returns `map<amount, tuple<total, unlocked, recent>>`. Iterates all outputs for the given amounts. |
| `get_output_distribution(amount, from_height, to_height, distribution, base)` | Returns cumulative output distribution for a given amount across a height range. |

### Key Image Operations

| Method | Description |
|--------|-------------|
| `has_key_image(img)` | Returns `true` if the key image is in the spent set. |
| `has_key_images(img_span)` | Batch check; returns a vector of booleans. LMDB override is more efficient than the base class default (single transaction, single cursor). |

### Transaction Pool Operations

| Method | Description |
|--------|-------------|
| `add_txpool_tx(txid, blob, meta)` | Adds tx metadata and blob to the pool tables. Throws `DB_ERROR` if already exists. |
| `update_txpool_tx(txid, meta)` | Deletes and re-inserts metadata for an existing pool tx (LMDB does not support in-place update for non-DUPSORT tables). |
| `remove_txpool_tx(txid)` | Removes both metadata and blob. Tolerates not-found. |
| `get_txpool_tx_count(category)` | Returns count of pool txes. For `relay_category::all`, uses fast `mdb_stat`; otherwise iterates and filters. |
| `txpool_has_tx(txid, tx_category)` | Checks existence with optional category filter. |
| `get_txpool_tx_meta(txid, meta)` | Returns metadata. Returns `false` if not found. |
| `get_txpool_tx_blob(txid, bd, tx_category)` | Returns blob with optional category check. Returns `false` if not found or category mismatch. |
| `txpool_tx_matches_category(tx_hash, category)` | Convenience: checks if a pool tx's relay method matches the category. |
| `for_all_txpool_txes(f, include_blob, category)` | Iterates all pool txes matching the category. Callback receives `(hash, meta, blob_ptr)`. |

### Alternative Block Operations

| Method | Description |
|--------|-------------|
| `add_alt_block(blkid, data, blob)` | Stores alt block metadata and blob concatenated in a single value. |
| `get_alt_block(blkid, *data, *blob)` | Retrieves alt block. Returns `false` if not found. |
| `remove_alt_block(blkid)` | Deletes an alt block. |
| `get_alt_block_count()` | Returns count of stored alt blocks via `mdb_stat`. |
| `drop_alt_blocks()` | Drops all alt blocks (via `mdb_drop` with delete=0). |
| `for_all_alt_blocks(f, include_blob)` | Iterates all alt blocks. |

### Batch Transaction Management

| Method | Description |
|--------|-------------|
| `set_batch_transactions(bool)` | Enables or disables batch mode. |
| `batch_start(batch_num_blocks, batch_bytes)` | Starts a batch transaction. Estimates needed space, checks for resize, begins a write txn. Returns `true` if started, `false` if already active. Resets per-thread read state. |
| `batch_commit()` | Commits the current batch and releases the write txn. Does NOT end batch mode (can start a new batch). |
| `batch_stop()` | Commits the batch and ends batch mode. |
| `batch_abort()` | Aborts the batch transaction, discarding all uncommitted changes. |

**Preconditions for all batch methods:** `m_batch_transactions` must be true, must be called from the writer thread. `batch_commit/stop/abort` require an active batch.

### Block-Level Transaction Management

| Method | Description |
|--------|-------------|
| `block_wtxn_start()` | Starts a per-block write transaction (if no batch is active). Creates a new write txn and resets cursors. |
| `block_wtxn_stop()` | Commits the per-block write transaction (no-op during batch). |
| `block_wtxn_abort()` | Aborts the per-block write transaction (no-op during batch). |
| `block_rtxn_start()` | Starts or reuses a per-thread read transaction. If the writer thread calls this, it reuses the write txn's cursors. Returns `true` if a new read txn was created. |
| `block_rtxn_stop()` | Resets the per-thread read transaction (returns it to a "not active" state). |
| `block_rtxn_abort()` | Same as `block_rtxn_stop()` in the LMDB implementation. |

### Hard Fork Version Storage

| Method | Description |
|--------|-------------|
| `set_hard_fork_version(height, version)` | Stores the hard fork version for a given height. Uses `MDB_APPEND` first, falling back to overwrite. |
| `get_hard_fork_version(height)` | Returns the stored hard fork version at the given height. Throws `DB_ERROR` if not found. |
| `check_hard_fork_info()` | No-op in the LMDB implementation. |
| `drop_hard_fork_info()` | Drops the `hf_starting_heights` and `hf_versions` tables. |

### Pruning

| Method | Description |
|--------|-------------|
| `get_blockchain_pruning_seed()` | Returns the pruning seed from the `properties` table, or 0 if not pruned. |
| `prune_blockchain(pruning_seed)` | Initiates full pruning. Stores the pruning seed, iterates all transactions, removes prunable data for blocks that should be pruned per the seed. Commits in batches of 4096 deletions. |
| `update_pruning()` | Incremental pruning update. Processes the `txs_prunable_tip` table to prune newly-eligible transactions. |
| `check_pruning()` | Verification mode: checks that prunable data is present/absent as expected per the pruning seed. |

All three modes share a single `prune_worker(mode, pruning_seed)` implementation.

### Enumeration / Iteration

| Method | Description |
|--------|-------------|
| `for_all_key_images(f)` | Iterates all spent key images. |
| `for_blocks_range(h1, h2, f)` | Iterates blocks in a height range, calling `f(height, hash, block)`. |
| `for_all_transactions(f, pruned)` | Iterates all transactions. |
| `for_all_outputs(f)` | Iterates all outputs with `(amount, tx_hash, height, tx_idx)`. |
| `for_all_outputs(amount, f)` | Iterates all outputs of a specific amount with `(height)`. |

### Statistics and Fixup

| Method | Description |
|--------|-------------|
| `reset_stats()` | Zeros performance counters. |
| `show_stats()` | Logs performance counters (block hash time, tx exists time, etc.). |
| `fixup()` | Checks for a known historical bug where key images were missing for transactions without outputs (mainnet blocks 202612 and 685498). If detected, pops blocks back to height 202612 to force re-sync. |

### RAII Transaction Guards (blockchain_db.h:1851-1897)

| Class | Description |
|-------|-------------|
| `db_txn_guard` | Base class. Constructor starts a read or write txn; `stop()` commits/stops; `abort()` aborts. Destructor calls `stop()`. |
| `db_rtxn_guard` | Convenience subclass for read-only transactions. |
| `db_wtxn_guard` | Convenience subclass for write transactions. |
| `LockedTXN` | (locked_txn.h) Starts a batch in its constructor, commits on `commit()`, aborts on destruction. Used when operating under the txpool lock. |

## Internal Logic

### LMDB Database Schema

The DB uses 19 named sub-databases (current schema version 5):

| Table Name | Key | Data | Flags |
|------------|-----|------|-------|
| `blocks` | block height (uint64) | block blob | `INTEGERKEY` |
| `block_heights` | zerokval (dummy) | `{hash, height}` | `INTEGERKEY, DUPSORT, DUPFIXED` |
| `block_info` | zerokval (dummy) | `mdb_block_info` struct | `INTEGERKEY, DUPSORT, DUPFIXED` |
| `txs_pruned` | tx_id (uint64) | pruned tx blob | `INTEGERKEY` |
| `txs_prunable` | tx_id (uint64) | prunable tx blob | `INTEGERKEY` |
| `txs_prunable_hash` | tx_id (uint64) | prunable tx hash | `INTEGERKEY, DUPSORT, DUPFIXED` |
| `txs_prunable_tip` | tx_id (uint64) | block height | `INTEGERKEY, DUPSORT, DUPFIXED` |
| `tx_indices` | zerokval (dummy) | `txindex` struct | `INTEGERKEY, DUPSORT, DUPFIXED` |
| `tx_outputs` | tx_id (uint64) | array of `uint64_t` amount output indices | `INTEGERKEY` |
| `output_txs` | zerokval (dummy) | `outtx` struct | `INTEGERKEY, DUPSORT, DUPFIXED` |
| `output_amounts` | amount (uint64) | `outkey` or `pre_rct_outkey` | `INTEGERKEY, DUPSORT, DUPFIXED` |
| `spent_keys` | zerokval (dummy) | key_image (32 bytes) | `INTEGERKEY, DUPSORT, DUPFIXED` |
| `txpool_meta` | tx hash (32 bytes) | `txpool_tx_meta_t` | (default) |
| `txpool_blob` | tx hash (32 bytes) | tx blob | (default) |
| `alt_blocks` | block hash (32 bytes) | `alt_block_data_t` + block blob | (default) |
| `hf_versions` | height (uint64) | version (uint8) | `INTEGERKEY` |
| `properties` | string key | variable | (default) |

**DUPFIXED optimization:** Many tables use a dummy zero key with `MDB_DUPSORT|MDB_DUPFIXED`, which saves 8 bytes per record by embedding the logical key as a prefix in the data. Custom comparison functions (`compare_hash32`, `compare_uint64`) are set on the DUPSORT tables.

**Properties stored:** `"version"` (uint32 = 5), `"pruning_seed"` (uint32, optional).

### Add Block Flow

1. `BlockchainLMDB::add_block(pair<block, blobdata>, ...)` checks every 1024 blocks for LMDB resize need.
2. Calls `BlockchainDB::add_block(...)` which:
   a. Verifies `blk.tx_hashes.size() == txs.size()`.
   b. Computes the block hash.
   c. Serializes and adds the miner tx via `add_transaction`.
   d. Adds each regular tx via `add_transaction`.
   e. Calls `BlockchainLMDB::add_block(blk, weight, ...)` to store the block blob and block_info record. Verifies block hash uniqueness, parent existence, writes blob with `MDB_APPEND`, constructs `mdb_block_info` including cumulative RCT output count (accumulated from previous block for major_version >= 4).
   f. Notifies `m_hardfork->add(blk, height)`.

### Add Transaction Flow

`BlockchainDB::add_transaction(blk_hash, tx, blob, ...)`:
1. Computes tx hash and prunable hash if not provided.
2. For each `txin_to_key` input, calls `add_spent_key(k_image)`.
3. Calls `add_transaction_data(...)` which stores the tx index, pruned blob, prunable blob, prunable hash (for v2+ txes), and optionally the prunable tip record (if pruning is enabled).
4. For each output, calls `add_output(...)` which stores the output in `output_txs` and `output_amounts`. RCT outputs (amount 0) store the full `outkey` with commitment; pre-RCT outputs store the shorter `pre_rct_outkey`.
5. Calls `add_tx_amount_output_indices(...)` to store the per-tx output index array.

### Pop Block Flow

`BlockchainLMDB::pop_block(blk, txs)`:
1. Wraps the operation in `block_wtxn_start()` / `block_wtxn_stop()`.
2. `BlockchainDB::pop_block(blk, txs)` retrieves the top block, calls `remove_block()`, then for each tx hash in reverse order: retrieves the tx, adds to txs vector, calls `remove_transaction(h)`.
3. `remove_transaction` fetches the pruned tx, removes all spent key images, then calls `remove_transaction_data` which removes entries from tx_indices, txs_pruned, txs_prunable, txs_prunable_hash, txs_prunable_tip, and tx_outputs. Also calls `remove_tx_outputs` to remove each output from `output_amounts` and `output_txs`.

### Auto-Resize Flow

1. `need_resize(threshold_size)` checks if the DB map is more than 90% full (`RESIZE_PERCENT = 0.9f`) or, for batch transactions, if remaining space is below the threshold.
2. `do_resize(increase_size)` adds 1 GB (or the specified increase), checks disk capacity, prevents new transactions, waits for active transactions to complete, calls `mdb_env_set_mapsize`, then allows new transactions.
3. `lmdb_resized()` handles `MDB_MAP_RESIZED` errors (when another process resized the map): prevents new txns, sets mapsize to 0 (re-reads from env), allows new txns.
4. For batch transactions, `check_and_resize_for_batch` estimates the needed size using recent block weight averages multiplied by safety factors (`batch_safety_factor = 1.7`, `db_expand_factor = 4.5`, minimum `batch_fudge_factor = 5000`).

### Batch Transaction Flow

1. `batch_start`: Checks for resize, creates a new `mdb_txn_safe`, begins a write txn, marks it as `m_batch_txn = true`, sets `m_batch_active = true`, zeros all write cursors, resets per-thread read state.
2. During batch: all write operations use `m_write_txn` (the batch txn). Read operations from the writer thread also use this txn to see uncommitted writes.
3. `batch_commit`: Commits the txn, nulls the pointer, zeros cursors. Does NOT reset `m_batch_active` -- the caller must call `batch_start` again for a new batch.
4. `batch_stop`: Commits and sets `m_batch_active = false`.
5. `batch_abort`: Calls `abort()` on the txn, cleans up.

### Read Transaction Management

The LMDB implementation uses per-thread read transactions stored in `boost::thread_specific_ptr<mdb_threadinfo>`. The `TXN_PREFIX_RDONLY()` macro at the start of every read method calls `block_rtxn_start()` which either:
- Reuses the write transaction if called from the writer thread (allowing reads to see uncommitted batch writes).
- Creates a new `mdb_threadinfo` with a new read txn if none exists for this thread.
- Renews the existing read txn if it was previously reset.

Cursors are lazily opened via the `RCURSOR(name)` macro, which opens a cursor on first use and renews it on subsequent uses within the same read transaction.

### Schema Migration

`BlockchainLMDB::migrate(oldversion)` is called during `open()` when the stored version is less than `VERSION` (5). Migrations run sequentially:

| Migration | Description |
|-----------|-------------|
| `migrate_0_1` | Converts `block_heights` from k(hash)/v(height) to DUPFIXED, consolidates five separate block metadata tables (`block_coins`, `block_diffs`, `block_hashes`, `block_sizes`, `block_timestamps`) into a single `block_info` table with `mdb_block_info_1`, converts `hf_versions` to INTEGERKEY, rebuilds output and spent key indices. |
| `migrate_1_2` | Adds `bi_cum_rct` field to block_info records (creates `mdb_block_info_2`), splitting tx blob storage into `txs_pruned` and `txs_prunable`. |
| `migrate_2_3` | Adds `bi_long_term_block_weight` field (creates `mdb_block_info_3`). |
| `migrate_3_4` | Splits `bi_diff` into `bi_diff_lo`/`bi_diff_hi` for 128-bit difficulty support (creates `mdb_block_info_4`). Also adds `txs_prunable_tip` table. |
| `migrate_4_5` | Restructures `block_info` from `mdb_block_info_3` to `mdb_block_info_4` format (the version 4 struct with split difficulty). |

All migrations commit in batches (every 1000-2000 records) to avoid unbounded memory growth, and use in-place record-by-record copying with intermediate table names followed by rename.

### Pruning

The `prune_worker` function operates in three modes:
- **prune**: Initial pruning. Sets the pruning seed, iterates all transactions via `tx_indices`, populates `txs_prunable_tip` for recent blocks, and deletes prunable data for blocks that shouldn't be stored per `tools::has_unpruned_block()`. Skips v1 transactions (which have no prunable data). Commits every 4096 deletions.
- **update**: Incremental. Iterates `txs_prunable_tip` to find transactions whose blocks are now old enough (beyond `CRYPTONOTE_PRUNING_TIP_BLOCKS`), prunes them if appropriate, and removes the tip entry.
- **check**: Read-only verification that all prunable data is present where expected and absent where expected.

## Dependencies

### What this module depends on

| Dependency | Usage |
|------------|-------|
| **LMDB** (`<lmdb.h>`) | Core storage engine -- all data persistence |
| `cryptonote_basic/cryptonote_basic.h` | Block, transaction, and output types |
| `cryptonote_basic/cryptonote_format_utils.h` | Serialization, hashing, parsing of blocks/transactions |
| `cryptonote_basic/difficulty.h` | `difficulty_type` (128-bit multiprecision) |
| `cryptonote_basic/hardfork.h` | `HardFork` class (notified on block add) |
| `cryptonote_protocol/enums.h` | `relay_method` enum |
| `crypto/hash.h`, `crypto/crypto.h` | Hash types, key image types |
| `ringct/rctOps.h`, `ringct/rctTypes.h` | `rct::zeroCommit()`, `rct::key` |
| `common/command_line.h` | Command-line argument descriptors |
| `common/util.h` | `tools::is_hdd()`, `tools::get_max_concurrency()` |
| `common/pruning.h` | Pruning utility functions |
| `epee` library | `critical_section`, `string_tools`, `span`, logging macros |
| `boost::filesystem` | Directory creation, file operations, disk space checks |
| `boost::thread` | `thread_specific_ptr`, `thread::id` for per-thread state |
| `boost::program_options` | CLI option definitions |

### What depends on this module

| Dependent | Usage |
|-----------|-------|
| `cryptonote_core/blockchain.h` | Primary consumer: owns a `BlockchainDB*`, calls all CRUD operations during block verification and chain management |
| `cryptonote_core/tx_pool.h` | Uses txpool operations (`add_txpool_tx`, `remove_txpool_tx`, etc.) |
| `cryptonote_core/tx_pool.cpp` | Uses `LockedTXN` for batch pool operations |
| `blockchain_utilities/` | Import/export tools use `BlockchainDB` directly |
| `rpc/` | RPC handlers query blockchain data through `Blockchain` which delegates to `BlockchainDB` |

## Configuration

### Command-Line Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `--db-sync-mode` | `string` | `"fast:async:250000000bytes"` | Sync mode in format `[safe\|fast\|fastest]:[sync\|async]:[<nblocks>blocks\|<nbytes>bytes]`. `safe` = full sync, `fast` = `MDB_NOSYNC`, `fastest` = `MDB_NOSYNC\|MDB_WRITEMAP\|MDB_MAPASYNC`. |
| `--db-salvage` | `bool` | `false` | If true, opens the DB with `MDB_PREVSNAPSHOT` to attempt recovery from corruption. |

### DB Flags (compile/runtime)

| Flag | Value | LMDB Effect |
|------|-------|-------------|
| `DBF_SAFE` | 1 | (Default LMDB behavior, no special flags) |
| `DBF_FAST` | 2 | `MDB_NOSYNC` |
| `DBF_FASTEST` | 4 | `MDB_NOSYNC \| MDB_WRITEMAP \| MDB_MAPASYNC` |
| `DBF_RDONLY` | 8 | `MDB_RDONLY` |
| `DBF_SALVAGE` | 16 | `MDB_PREVSNAPSHOT` |

### Compile-Time Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `VERSION` | 5 | Current DB schema version |
| `ENABLE_AUTO_RESIZE` | (defined) | Enables automatic map size growth; always defined in `db_lmdb.h` |
| `DEFAULT_MAPSIZE` | `1 << 30` (1 GB) | Initial LMDB map size (with auto-resize); `1 << 31` on 32-bit ARM; `1 << 33` without auto-resize |
| `RESIZE_PERCENT` | 0.9 | Trigger resize when 90% full |
| `MDB_NORDAHEAD` | (LMDB flag) | Always set on open to disable OS readahead (random access pattern) |

### Platform-Specific Behavior

- **OpenBSD**: Forces `MDB_WRITEMAP` flag.
- **Windows**: Disables NTFS compression on the DB directory and data file to prevent corruption.
- **HDD detection**: Warns if the blockchain is on a rotating disk.

## LMDB Thread Safety and Shutdown Constraints

Source: `db_lmdb.cpp:1637`, `mdb_txn_safe`, LMDB documentation.

### Read Transaction Thread Safety

- **Multiple concurrent readers:** LMDB supports any number of concurrent read transactions across different threads. Each thread gets its own `mdb_threadinfo` via `boost::thread_specific_ptr`.
- **Read transaction reuse:** The writer thread can reuse the write transaction for reads via `block_rtxn_start()`, allowing it to see its own uncommitted writes.
- **Cursor management:** Each thread's read transaction has its own set of cursors (`mdb_txn_cursors`), lazily opened via the `RCURSOR()` macro.

### Write Transaction Thread Safety

- **Single writer:** LMDB allows only one write transaction at a time. All write operations in Monero go through a single writer thread.
- **Batch transactions:** During batch operations (e.g., block import), a single write transaction spans multiple blocks. `m_write_batch_txn` holds the batch transaction.
- **Creation gate:** `mdb_txn_safe::creation_gate` (an `std::atomic_flag`) serializes transaction creation to prevent races during resize operations.

### Shutdown Sequence

The LMDB `close()` operation (`mdb_env_close`) is **NOT thread-safe** — all read transactions must be finished before closing the environment. The FIXME at db_lmdb.cpp:1637 notes this explicitly.

**Race window during daemon shutdown:**
1. Background sync threads (wallet refresh, block verification) may hold active read transactions.
2. The daemon's `deinit()` signals threads to stop via `m_run = false` and similar flags.
3. There is a timing window where `mdb_env_close()` could be called while a read transaction is still active.

**Mitigation strategy in Monero:**
- `blockchain.cpp::deinit()` calls `m_db->close()` after stopping the blockchain sync loop.
- `mdb_txn_safe` provides static methods:
  - `prevent_new_txns()`: Sets the creation gate to block new transaction creation.
  - `wait_no_active_txns()`: Busy-waits until `num_active_txns` reaches 0.
  - `allow_new_txns()`: Clears the creation gate.
- During resize (`do_resize()`), the same prevent/wait/allow sequence is used to ensure no transactions are active when the map size changes.

### Active Transaction Counting

`mdb_txn_safe` maintains a global `static std::atomic<uint64_t> num_active_txns` counter:
- Incremented in the constructor when a new transaction is created.
- Decremented in the destructor when a transaction completes (commit or abort).
- Used by `wait_no_active_txns()` to ensure safe shutdown and resize operations.

## Known Issues

The following TODO/FIXME/HACK/XXX comments are present in the source:

| File | Line | Comment |
|------|------|---------|
| `blockchain_db.h` | 1141 | `TODO: Rewrite (if necessary) such that all calls to remove_* are done in concrete members of this base class.` |
| `blockchain_db.h` | 1364 | `TODO: decide if this behavior is correct for missing transactions` (re: `get_tx_list` throwing on missing tx) |
| `blockchain_db.h` | 1394 | `TODO: should outputs spent with a low mixin (especially 0) be excluded from the count?` (re: `get_num_outputs`) |
| `blockchain_db.h` | 1481 | `FIXME: Need to check with git blame and ask what this does to document it` (re: `can_thread_bulk_indices`) |
| `blockchain_db.h` | 1829 | `TODO: this should perhaps be (or call) a series of functions which progressively update through version updates` (re: `fixup`) |
| `db_lmdb.cpp` | 972 | `TODO: compare pros and cons of looking up the tx hash's tx index once and passing it in to functions like this` (re: `remove_transaction_data`) |
| `db_lmdb.cpp` | 1637 | `FIXME: not yet thread safe!!! Use with care.` (re: `close()` calling `mdb_env_close`) |
