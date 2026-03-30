# Wallet2

## Overview

`wallet2` is the core wallet implementation in Monero, responsible for all client-side wallet operations including key management, blockchain synchronization, balance computation, transaction construction and signing, subaddress management, multisig coordination, and persistent storage of wallet state. It communicates with a Monero daemon over RPC to fetch blocks and broadcast transactions, while all cryptographic operations (key derivation, output scanning, key image generation) happen locally. The class supports full wallets, watch-only wallets, hardware device wallets, multisig wallets, and background-syncing wallets. It resides in the `tools` namespace and is consumed by both the CLI wallet (`simplewallet`), the RPC wallet server (`wallet_rpc_server`), and the C++ wallet API (`src/wallet/api/`).

## Key Files

| File | Lines | Description |
|------|-------|-------------|
| `src/wallet/wallet2.h` | ~2503 | Class definition, nested data structures, serialization, public API declarations |
| `src/wallet/wallet2.cpp` | ~15429 | Full implementation of all wallet2 methods |
| `src/wallet/wallet_errors.h` | - | Exception types thrown by wallet2 |
| `src/wallet/node_rpc_proxy.h` | - | Caching proxy for daemon RPC calls, used by wallet2 |
| `src/wallet/ringdb.h` | - | Ring database for storing ring member selections |
| `src/wallet/fee_priority.h` | - | Fee priority enum type |
| `src/wallet/fee_algorithm.h` | - | Fee algorithm enum type |
| `src/wallet/message_store.h` | - | MMS (Multisig Messaging System) store, owned by wallet2 |

## Data Structures

### Core Class: `tools::wallet2`

The wallet2 class is defined at `wallet2.h:217` within the `tools` namespace. It maintains the full wallet state including keys, blockchain history, transfer records, and configuration.

### Enumerations

| Enum | Values | Description |
|------|--------|-------------|
| `RefreshType` | `RefreshFull`, `RefreshOptimizeCoinbase`, `RefreshNoCoinbase` | Controls how coinbase transactions are handled during sync. Default is `RefreshOptimizeCoinbase`. |
| `AskPasswordType` | `AskPasswordNever`, `AskPasswordOnAction`, `AskPasswordToDecrypt` | When to prompt for password. Default is `AskPasswordToDecrypt`. |
| `BackgroundSyncType` | `BackgroundSyncOff`, `BackgroundSyncReusePassword`, `BackgroundSyncCustomPassword` | Background sync mode. Off by default. |
| `BackgroundMiningSetupType` | `BackgroundMiningMaybe`, `BackgroundMiningYes`, `BackgroundMiningNo` | Whether to set up background mining. |
| `ExportFormat` | `Binary`, `Ascii` | Output export format. |

### `transfer_details` (wallet2.h:333)

Represents a single owned output (enote) in the wallet.

| Field | Type | Description |
|-------|------|-------------|
| `m_block_height` | `uint64_t` | Block height where the output was received |
| `m_tx` | `transaction_prefix` | The transaction containing this output (prefix only, pruned) |
| `m_txid` | `crypto::hash` | Transaction hash |
| `m_internal_output_index` | `uint64_t` | Index of this output within the transaction |
| `m_global_output_index` | `uint64_t` | Global output index on the blockchain |
| `m_spent` | `bool` | Whether the output has been spent |
| `m_frozen` | `bool` | Whether the output is frozen (excluded from spending) |
| `m_spent_height` | `uint64_t` | Block height where spent (0 if unspent) |
| `m_key_image` | `crypto::key_image` | Key image for this output |
| `m_mask` | `rct::key` | RingCT mask for amount commitment |
| `m_amount` | `uint64_t` | Decrypted amount |
| `m_rct` | `bool` | Whether this is an RCT output |
| `m_key_image_known` | `bool` | Whether the key image has been computed |
| `m_key_image_request` | `bool` | Whether key image was requested (for view/cold wallets) |
| `m_pk_index` | `uint64_t` | TX public key index used for derivation |
| `m_subaddr_index` | `subaddress_index` | Subaddress (major, minor) this output was received at |
| `m_key_image_partial` | `bool` | Whether key image is partial (multisig) |
| `m_multisig_k` | `vector<rct::key>` | Multisig nonces |
| `m_multisig_info` | `vector<multisig_info>` | Multisig info from other participants |
| `m_uses` | `vector<pair<uint64_t, hash>>` | Tracked usage of this output in ring signatures |

Key methods: `is_rct()`, `amount()`, `get_public_key()`.

Boost serialization version: 12.

### `payment_details` (wallet2.h:432)

Represents an incoming payment.

| Field | Type | Description |
|-------|------|-------------|
| `m_tx_hash` | `crypto::hash` | Transaction hash |
| `m_amount` | `uint64_t` | Total amount received |
| `m_amounts` | `vector<uint64_t>` | Individual output amounts |
| `m_fee` | `uint64_t` | Transaction fee |
| `m_block_height` | `uint64_t` | Confirmation height |
| `m_unlock_time` | `uint64_t` | Unlock time |
| `m_timestamp` | `uint64_t` | Block timestamp |
| `m_coinbase` | `bool` | Whether from a coinbase transaction |
| `m_subaddr_index` | `subaddress_index` | Receiving subaddress |

### `unconfirmed_transfer_details` (wallet2.h:476)

Represents an outgoing transaction not yet confirmed.

| Field | Type | Description |
|-------|------|-------------|
| `m_tx` | `transaction_prefix` | The transaction |
| `m_amount_in` | `uint64_t` | Total input amount |
| `m_amount_out` | `uint64_t` | Total output amount (including change) |
| `m_change` | `uint64_t` | Change amount |
| `m_sent_time` | `time_t` | When the transaction was sent |
| `m_dests` | `vector<tx_destination_entry>` | Destinations |
| `m_payment_id` | `crypto::hash` | Payment ID |
| `m_state` | enum | `pending`, `pending_in_pool`, `failed` |
| `m_timestamp` | `uint64_t` | Timestamp |
| `m_subaddr_account` | `uint32_t` | Originating subaddress account |
| `m_subaddr_indices` | `set<uint32_t>` | Input subaddress indices |
| `m_rings` | `vector<pair<key_image, vector<uint64_t>>>` | Ring members (relative offsets) |

### `confirmed_transfer_details` (wallet2.h:509)

Same as `unconfirmed_transfer_details` but for confirmed outgoing transactions. Adds `m_block_height` and `m_unlock_time`.

### `tx_construction_data` (wallet2.h:546)

Data needed to construct a transaction: sources, destinations (with change), selected transfers, extra bytes, unlock time, RCT config, subaddress account/indices.

### `pending_tx` (wallet2.h:641)

A fully constructed but not yet committed transaction. Contains the `cryptonote::transaction`, dust, fee, change destination, selected transfers, tx secret key, additional keys, destinations, multisig signatures, and the `tx_construction_data`.

### `unsigned_tx_set` / `signed_tx_set` / `multisig_tx_set` (wallet2.h:682-737)

Containers for offline signing workflows:
- `unsigned_tx_set`: construction data + exported transfer details for cold signing
- `signed_tx_set`: signed pending transactions + key images
- `multisig_tx_set`: pending transactions + set of signers

### `keys_file_data` / `cache_file_data` (wallet2.h:739-759)

Encrypted on-disk containers:
- `keys_file_data`: ChaCha20 IV + encrypted account data (JSON with key_data, settings)
- `cache_file_data`: ChaCha20 IV + encrypted wallet cache (serialized wallet2 state)

### `address_book_row` (wallet2.h:762)

GUI address book entry: address, payment ID (8-byte), description, is_subaddress flag.

### `background_sync_data_t` (wallet2.h:824)

Background sync state: first_refresh_done flag, start_height, map of background-synced transactions, and saved wallet settings (refresh height, lookahead, refresh type).

### `hashchain` (wallet2.h:177)

A deque-based chain of block hashes with an offset, supporting efficient trimming of old hashes while preserving correct height indexing. Represents the wallet's known blockchain.

### Type Aliases

```cpp
typedef std::vector<transfer_details> transfer_container;
typedef std::unordered_multimap<crypto::hash, payment_details> payment_container;
typedef std::set<uint32_t> unique_index_container;
typedef std::tuple<uint64_t, crypto::public_key, rct::key> get_outs_entry;
```

### Key Member Variables

**Account & Keys:**
| Variable | Type | Description |
|----------|------|-------------|
| `m_account` | `cryptonote::account_base` | The cryptographic account (keys + address) |
| `m_account_public_address` | `account_public_address` | Cached public address |
| `m_watch_only` | `bool` | True if no spend key is available |
| `m_multisig` | `bool` | True if this is a multisig wallet |
| `m_multisig_threshold` | `uint32_t` | M-of-N threshold |
| `m_multisig_signers` | `vector<public_key>` | Public keys of all multisig participants |
| `m_multisig_rounds_passed` | `uint32_t` | Key exchange rounds completed |
| `m_key_device_type` | `hw::device::device_type` | SOFTWARE or hardware device type |

**Transfer & Payment State:**
| Variable | Type | Description |
|----------|------|-------------|
| `m_transfers` | `transfer_container` | All owned outputs (enotes) |
| `m_key_images` | `unordered_map<key_image, size_t>` | Key image to transfer index mapping |
| `m_pub_keys` | `unordered_map<public_key, size_t>` | Output public key to transfer index |
| `m_payments` | `payment_container` | Incoming payments (by payment ID) |
| `m_unconfirmed_txs` | `unordered_map<hash, unconfirmed_transfer_details>` | Pending outgoing transactions |
| `m_confirmed_txs` | `unordered_map<hash, confirmed_transfer_details>` | Confirmed outgoing transactions |
| `m_unconfirmed_payments` | `unordered_multimap<hash, pool_payment_details>` | Pool incoming payments |
| `m_tx_keys` | `unordered_map<hash, secret_key>` | TX secret keys by TX hash |
| `m_additional_tx_keys` | `unordered_map<hash, vector<secret_key>>` | Additional TX keys (subaddress) |

**Blockchain State:**
| Variable | Type | Description |
|----------|------|-------------|
| `m_blockchain` | `hashchain` | Known block hash chain |
| `m_checkpoints` | `cryptonote::checkpoints` | Hardcoded checkpoints |
| `m_last_block_reward` | `uint64_t` | Last seen block reward |
| `m_has_ever_refreshed_from_node` | `bool` | Whether any refresh has completed |

**Subaddress State:**
| Variable | Type | Description |
|----------|------|-------------|
| `m_subaddresses` | `unordered_map<public_key, subaddress_index>` | Spend public key to subaddress index |
| `m_subaddress_labels` | `vector<vector<string>>` | Labels indexed by [major][minor] |
| `m_subaddress_lookahead_major` | `size_t` | Major index lookahead (default: 50) |
| `m_subaddress_lookahead_minor` | `size_t` | Minor index lookahead (default: 200) |

**Configuration:**
| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `m_refresh_type` | `RefreshType` | `RefreshOptimizeCoinbase` | How to handle coinbase during sync |
| `m_refresh_from_block_height` | `uint64_t` | 0 | Wallet restore height |
| `m_default_mixin` | `uint32_t` | 0 | Default ring size offset |
| `m_default_priority` | `fee_priority` | Default | Transaction fee priority |
| `m_auto_refresh` | `bool` | true | Whether to auto-refresh |
| `m_trusted_daemon` | `bool` | false | Whether daemon is trusted |
| `m_max_reorg_depth` | `uint64_t` | ORPHANED_BLOCKS_MAX_COUNT | Maximum allowed reorg depth |
| `m_ignore_outputs_above` | `uint64_t` | MONEY_SUPPLY | Ignore outputs larger than this |
| `m_ignore_outputs_below` | `uint64_t` | 0 | Ignore outputs smaller than this |
| `m_track_uses` | `bool` | false | Track output usage in rings |
| `m_segregate_pre_fork_outputs` | `bool` | true | Segregate pre-fork outputs in rings |

**Daemon Communication:**
| Variable | Type | Description |
|----------|------|-------------|
| `m_daemon_address` | `string` | Daemon URL |
| `m_daemon_login` | `optional<login>` | Daemon authentication |
| `m_proxy` | `string` | SOCKS proxy address |
| `m_http_client` | `unique_ptr<http_client>` | HTTP client for daemon RPC |
| `m_daemon_rpc_mutex` | `recursive_mutex` | Mutex for RPC calls |
| `m_node_rpc_proxy` | `NodeRPCProxy` | Caching RPC proxy |
| `m_offline` | `bool` | Whether wallet operates offline |

**Background Sync:**
| Variable | Type | Description |
|----------|------|-------------|
| `m_background_syncing` | `bool` | Currently background syncing |
| `m_background_sync_type` | `BackgroundSyncType` | Background sync mode |
| `m_background_sync_data` | `background_sync_data_t` | Background sync state |
| `m_custom_background_key` | `optional<chacha_key>` | Custom encryption key for background cache |

**Storage & Security:**
| Variable | Type | Description |
|----------|------|-------------|
| `m_wallet_file` | `string` | Path to wallet cache file |
| `m_keys_file` | `string` | Path to keys file |
| `m_cache_key` | `chacha_key` | Encryption key for cache |
| `m_kdf_rounds` | `uint64_t` | Key derivation function rounds |
| `m_ring_database` | `string` | Path to shared ring database |
| `m_ringdb` | `unique_ptr<ringdb>` | Ring database handle |

## Public API

### Static Factory Methods

```cpp
static pair<unique_ptr<wallet2>, password_container> make_from_json(vm, unattended, json_file, password_prompter);
static pair<unique_ptr<wallet2>, password_container> make_from_file(vm, unattended, wallet_file, password_prompter);
static pair<unique_ptr<wallet2>, password_container> make_new(vm, unattended, password_prompter);
static unique_ptr<wallet2> make_dummy(vm, unattended, password_prompter);
```

Create wallet2 instances from various sources. Parse command-line variables, prompt for passwords, and initialize the wallet.

```cpp
static bool verify_password(keys_file_name, password, no_spend_key, hwdev, kdf_rounds);
static bool verify_password(keys_file_name, password, no_spend_key, hwdev, kdf_rounds, spend_key_out);
static bool query_device(device_type, keys_file_name, password, kdf_rounds);
```

Static verification without loading a full wallet instance.

```cpp
static void init_options(boost::program_options::options_description& desc_params);
```

Registers wallet2-specific command-line options (daemon-address, proxy, testnet, stagenet, kdf-rounds, hw-device, etc.).

### Initialization & Lifecycle

```cpp
wallet2(network_type nettype = MAINNET, uint64_t kdf_rounds = 1, bool unattended = false, unique_ptr<http_client_factory> factory = ...);
~wallet2();
```

Constructor initializes all member variables to defaults. Destructor calls `deinit()`.

```cpp
bool init(string daemon_address, optional<login>, string proxy, uint64_t upper_tx_weight, bool trusted, ssl_options);
bool deinit();
```

`init()` sets the proxy, initializes checkpoints, and connects to the daemon. `deinit()` is a cleanup no-op that returns true.

```cpp
bool set_daemon(string daemon_address, optional<login>, bool trusted, ssl_options, string proxy);
bool set_proxy(string address);
void stop();
```

Reconfigure daemon connection at runtime. `stop()` sets `m_run = false` to interrupt refresh loops.

### Wallet Generation

```cpp
crypto::secret_key generate(string wallet, wipeable_string password, secret_key recovery_param = {}, bool recover = false, bool two_random = false, bool create_address_file = false);
```

Generate a new wallet or restore from a secret key. Returns the spend secret key. For new wallets, estimates blockchain height as refresh start. For non-deterministic wallets, pass `two_random = true`.

```cpp
void generate(string wallet, wipeable_string password, account_public_address, secret_key viewkey, bool create_address_file = false);
```

Create a watch-only wallet from a public address and view key.

```cpp
void generate(string wallet, wipeable_string password, account_public_address, secret_key spendkey, secret_key viewkey, bool create_address_file = false);
```

Create a full wallet from explicit key pair.

```cpp
void generate(string wallet, wipeable_string password, wipeable_string multisig_data, bool create_address_file = false);
```

Restore a multisig wallet from serialized multisig seed data.

```cpp
void restore(string wallet, wipeable_string password, string device_name, bool create_address_file = false);
```

Restore a wallet from a hardware device.

### Key Management

```cpp
bool is_deterministic() const;
bool get_seed(wipeable_string& electrum_words, wipeable_string passphrase = {}) const;
const string& get_seed_language() const;
void set_seed_language(const string& language);
```

Seed/mnemonic management. Deterministic wallets derive view key from spend key via Keccak hash.

```cpp
void encrypt_keys(const chacha_key& key);
void encrypt_keys(const wipeable_string& password);
void decrypt_keys(const chacha_key& key);
void decrypt_keys(const wipeable_string& password);
bool is_key_encryption_enabled() const;
```

In-memory key encryption. When `AskPasswordToDecrypt` is set, keys are encrypted in RAM and only decrypted when needed.

```cpp
bool verify_password(const wipeable_string& password);
bool verify_password(const wipeable_string& password, secret_key& spend_key_out);
```

Verify the wallet password matches the stored keys.

```cpp
void change_password(string filename, wipeable_string original, wipeable_string new_password);
```

Re-encrypt the keys file with a new password.

```cpp
account_base& get_account();
bool watch_only() const;
bool key_on_device() const;
hw::device::device_type get_device_type() const;
bool reconnect_device();
```

Account and device access.

### Sync & Refresh

```cpp
void refresh(bool trusted_daemon);
void refresh(bool trusted_daemon, uint64_t start_height, uint64_t& blocks_fetched);
void refresh(bool trusted_daemon, uint64_t start_height, uint64_t& blocks_fetched, bool& received_money, bool check_pool = true, bool try_incremental = true, uint64_t max_blocks = MAX);
bool refresh(bool trusted_daemon, uint64_t& blocks_fetched, bool& received_money, bool& ok);
```

Main sync entry points. The full overload (`wallet2.cpp:4074`) implements the core sync loop:
1. Build short chain history (exponentially spaced block hashes)
2. If start height is ahead, do a fast refresh (hash-only) up to that point
3. Enter a pipelined loop: fetch next blocks in a background thread while processing current blocks
4. Process each block batch via `process_parsed_blocks()` which parallelizes tx scanning
5. After blocks are synced, process pool transactions
6. Retry up to 3 times on transient errors

```cpp
void set_refresh_from_block_height(uint64_t height);
uint64_t get_refresh_from_block_height() const;
void set_refresh_type(RefreshType type);
RefreshType get_refresh_type() const;
```

Refresh configuration.

```cpp
void update_pool_state(vector<tuple<transaction, hash, bool>>& process_txs, bool refreshed = false, bool try_incremental = false);
void process_pool_state(const vector<tuple<transaction, hash, bool>>& txs);
```

Pool monitoring. `update_pool_state` supports both incremental (via getblocks.bin) and legacy (separate pool hash query) methods.

```cpp
void scan_tx(const unordered_set<hash>& txids);
void rescan_spent();
void rescan_blockchain(bool hard, bool refresh = true, bool keep_key_images = false);
```

Targeted and full rescan operations.

### Balance & Transfer Queries

```cpp
uint64_t balance(uint32_t subaddr_index_major, bool strict) const;
uint64_t unlocked_balance(uint32_t subaddr_index_major, bool strict, uint64_t* blocks_to_unlock = NULL, uint64_t* time_to_unlock = NULL);
map<uint32_t, uint64_t> balance_per_subaddress(uint32_t subaddr_index_major, bool strict) const;
map<uint32_t, pair<uint64_t, pair<uint64_t, uint64_t>>> unlocked_balance_per_subaddress(uint32_t subaddr_index_major, bool strict);
uint64_t balance_all(bool strict) const;
uint64_t unlocked_balance_all(bool strict, uint64_t* blocks_to_unlock = NULL, uint64_t* time_to_unlock = NULL);
```

Balance calculations. When `strict = true`, only uses on-chain confirmed data. When `strict = false`, includes unconfirmed change and self-transfers. Iterates `m_transfers` summing amounts for unspent, unfrozen outputs matching the subaddress account.

```cpp
void get_transfers(transfer_container& incoming) const;
void get_payments(const hash& payment_id, list<payment_details>& payments, uint64_t min_height = 0, ...);
void get_payments(list<pair<hash, payment_details>>& payments, uint64_t min_height, uint64_t max_height, ...);
void get_payments_out(list<pair<hash, confirmed_transfer_details>>& confirmed, uint64_t min, uint64_t max, ...);
void get_unconfirmed_payments_out(list<pair<hash, unconfirmed_transfer_details>>& unconfirmed, ...);
void get_unconfirmed_payments(list<pair<hash, pool_payment_details>>& unconfirmed, ...);
```

Query payment history by type and height range.

```cpp
bool is_transfer_unlocked(const transfer_details& td);
bool is_transfer_unlocked(uint64_t unlock_time, uint64_t block_height);
bool is_tx_spendtime_unlocked(uint64_t unlock_time, uint64_t block_height);
```

Check if an output is spendable (past unlock time and default spendable age).

```cpp
size_t get_num_transfer_details() const;
const transfer_details& get_transfer_details(size_t idx) const;
uint64_t get_blockchain_current_height() const;
```

Direct access to transfer details and blockchain height.

### Transaction Creation

```cpp
vector<pending_tx> create_transactions_2(vector<tx_destination_entry> dsts, size_t fake_outs_count, fee_priority priority, vector<uint8_t> extra, uint32_t subaddr_account, set<uint32_t> subaddr_indices, unique_index_container subtract_fee_from_outputs = {});
```

Primary transaction creation method. Selects inputs, determines fee, picks decoy outputs, constructs and signs one or more transactions. Supports fee subtraction from specific output indices. Uses the internal TX struct for incremental construction.

```cpp
vector<pending_tx> create_transactions_all(uint64_t below, account_public_address address, bool is_subaddress, size_t outputs, size_t fake_outs_count, fee_priority priority, vector<uint8_t> extra, uint32_t subaddr_account, set<uint32_t> subaddr_indices);
vector<pending_tx> create_transactions_single(const key_image& ki, account_public_address address, ...);
vector<pending_tx> create_transactions_from(account_public_address address, ..., vector<size_t> unused_transfers, vector<size_t> unused_dust, ...);
vector<pending_tx> create_unmixable_sweep_transactions();
```

Sweep and targeted transaction creation variants.

```cpp
void commit_tx(pending_tx& ptx);
void commit_tx(vector<pending_tx>& ptx_vector);
```

Broadcast transactions to the network via daemon RPC.

```cpp
template<typename T> void transfer_selected(dsts, selected_transfers, fake_outputs_count, outs, valid_keys_cache, fee, extra, split_strategy, dust_policy, tx, ptx, use_view_tags);
void transfer_selected_rct(dsts, selected_transfers, fake_outputs_count, outs, valid_keys_cache, fee, extra, tx, ptx, rct_config, use_view_tags);
```

Lower-level transaction construction from pre-selected inputs.

### Signing & Offline Workflows

```cpp
bool sign_tx(string unsigned_filename, string signed_filename, vector<pending_tx>& ptx, function<bool(const unsigned_tx_set&)> accept_func = NULL, bool export_raw = false);
bool sign_tx(unsigned_tx_set& exported_txs, vector<pending_tx>& ptx, signed_tx_set& signed_txs);
string sign_tx_dump_to_str(unsigned_tx_set& exported_txs, vector<pending_tx>& ptx, signed_tx_set& signed_txes);
```

Sign unsigned transactions (for cold-signing workflow).

```cpp
bool save_tx(const vector<pending_tx>& ptx_vector, const string& filename) const;
string dump_tx_to_str(const vector<pending_tx>& ptx_vector) const;
bool load_unsigned_tx(string unsigned_filename, unsigned_tx_set& exported_txs) const;
bool parse_unsigned_tx_from_str(string unsigned_tx_st, unsigned_tx_set& exported_txs) const;
bool load_tx(string signed_filename, vector<pending_tx>& ptx, function<bool(const signed_tx_set&)> accept_func = NULL);
bool parse_tx_from_str(string signed_tx_st, vector<pending_tx>& ptx, function<bool(const signed_tx_set&)> accept_func);
```

Serialization for offline signing.

### Address Management

```cpp
account_public_address get_subaddress(const subaddress_index& index) const;
account_public_address get_address() const;  // {0,0}
optional<subaddress_index> get_subaddress_index(const account_public_address& address) const;
public_key get_subaddress_spend_public_key(const subaddress_index& index) const;
vector<public_key> get_subaddress_spend_public_keys(uint32_t account, uint32_t begin, uint32_t end) const;
string get_subaddress_as_str(const subaddress_index& index) const;
string get_address_as_str() const;
string get_integrated_address_as_str(const hash8& payment_id) const;
```

Subaddress derivation and formatting.

```cpp
void add_subaddress_account(const string& label);
void add_subaddress(uint32_t index_major, const string& label);
void expand_subaddresses(const subaddress_index& index);
void create_one_off_subaddress(const subaddress_index& index);
size_t get_num_subaddress_accounts() const;
size_t get_num_subaddresses(uint32_t index_major) const;
string get_subaddress_label(const subaddress_index& index) const;
void set_subaddress_label(const subaddress_index& index, const string& label);
void set_subaddress_lookahead(size_t major, size_t minor);
pair<size_t, size_t> get_subaddress_lookahead() const;
```

Subaddress account and index management. Default lookahead: 50 major x 200 minor.

### Store & Load

```cpp
void load(const string& wallet, const wipeable_string& password, const string& keys_buf = "", const string& cache_buf = "");
void store();
void store_to(const string& path, const wipeable_string& password, bool force_rewrite_keys = false);
```

**Load** (`wallet2.cpp:6498`):
1. Clears wallet state and prepares file names
2. Loads and decrypts keys file (JSON with account data, settings)
3. Loads and decrypts cache file (binary-serialized wallet2 state)
4. Falls back through multiple decryption/deserialization strategies for backward compatibility (chacha20 -> chacha8 -> portable binary -> unportable binary -> unencrypted)
5. Verifies genesis block hash
6. Initializes MMS and processes background cache

**Store** (`wallet2.cpp:6825`):
1. Trims hashchain
2. If same file: optionally rewrite keys, write cache to `.new`, atomically rename
3. If different file: write new keys, write new cache, remove old files
4. Stores MMS file if active
5. Updates background cache if using custom background password

```cpp
optional<keys_file_data> get_keys_file_data(const wipeable_string& password, bool watch_only);
optional<cache_file_data> get_cache_file_data();
void rewrite(const string& wallet_name, const wipeable_string& password);
void write_watch_only_wallet(const string& wallet_name, const wipeable_string& password, string& new_keys_filename);
```

Keys file format: JSON document encrypted with ChaCha20 using password-derived key. Contains `key_data` (binary-serialized account), `seed_language`, `watch_only`, `multisig`, and ~40 wallet settings stored as JSON fields.

Cache file format: Binary-serialized wallet2 state encrypted with ChaCha20 using cache key (derived from password + secret keys).

### Export & Import

```cpp
tuple<uint64_t, uint64_t, vector<exported_transfer_details>> export_outputs(bool all = false, uint32_t start = 0, uint32_t count = MAX) const;
string export_outputs_to_str(bool all = false, uint32_t start = 0, uint32_t count = MAX) const;
size_t import_outputs(const tuple<uint64_t, uint64_t, vector<exported_transfer_details>>& outputs);
size_t import_outputs(const tuple<uint64_t, uint64_t, vector<transfer_details>>& outputs);
size_t import_outputs_from_str(const string& outputs_st);
```

Output export/import for view-only wallet workflow. Magic: `"Monero output export\004"`.

```cpp
bool export_key_images(const string& filename, bool all = false) const;
pair<uint64_t, vector<pair<key_image, signature>>> export_key_images(bool all = false) const;
uint64_t import_key_images(const vector<pair<key_image, signature>>& signed_key_images, size_t offset, uint64_t& spent, uint64_t& unspent, bool check_spent = true);
uint64_t import_key_images(const string& filename, uint64_t& spent, uint64_t& unspent);
bool import_key_images(vector<key_image> key_images, size_t offset = 0, optional<unordered_set<size_t>> selected = boost::none);
```

Key image export/import. Magic: `"Monero key image export\003"`. Export signs each key image with the output secret key for verification.

```cpp
payment_container export_payments() const;
void import_payments(const payment_container& payments);
void import_payments_out(const list<pair<hash, confirmed_transfer_details>>& confirmed);
tuple<size_t, hash, vector<hash>> export_blockchain() const;
void import_blockchain(const tuple<size_t, hash, vector<hash>>& bc);
```

Payment and blockchain data import/export.

### Multisig Support

```cpp
multisig::multisig_account_status get_multisig_status() const;
string get_multisig_first_kex_msg() const;
string make_multisig(const wipeable_string& password, const vector<string>& kex_messages, uint32_t threshold);
string exchange_multisig_keys(const wipeable_string& password, const vector<string>& kex_messages, bool force_update = false);
string get_multisig_key_exchange_booster(const wipeable_string& password, const vector<string>& kex_messages, uint32_t threshold, uint32_t num_signers);
```

Multisig wallet setup via key exchange protocol.

```cpp
blobdata export_multisig();
size_t import_multisig(vector<blobdata> info);
```

Export/import multisig info (partial key images and nonces). Magic: `"Monero multisig export\001"`.

```cpp
string save_multisig_tx(multisig_tx_set txs);
bool save_multisig_tx(const multisig_tx_set& txs, const string& filename);
bool load_multisig_tx(blobdata blob, multisig_tx_set& exported_txs, function<bool(const multisig_tx_set&)> accept_func = NULL);
bool sign_multisig_tx(multisig_tx_set& exported_txs, vector<hash>& txids);
```

Multisig transaction signing workflow. Magic: `"Monero multisig unsigned tx set\001"`.

### Proofs & Verification

```cpp
string get_tx_proof(const hash& txid, const account_public_address& address, bool is_subaddress, const string& message);
bool check_tx_proof(const hash& txid, const account_public_address& address, bool is_subaddress, const string& message, const string& sig_str, uint64_t& received, bool& in_pool, uint64_t& confirmations);
string get_spend_proof(const hash& txid, const string& message);
bool check_spend_proof(const hash& txid, const string& message, const string& sig_str);
string get_reserve_proof(const optional<pair<uint32_t, uint64_t>>& account_minreserve, const string& message);
bool check_reserve_proof(const account_public_address& address, const string& message, const string& sig_str, uint64_t& total, uint64_t& spent);
```

Cryptographic proof generation and verification for transaction ownership, spending, and reserve amounts.

### Message Signing

```cpp
string sign(const string& data, message_signature_type_t type, subaddress_index index = {0,0}) const;
message_signature_result_t verify(const string& data, const account_public_address& address, const string& signature) const;
string sign_multisig_participant(const string& data) const;
bool verify_with_public_key(const string& data, const public_key& pkey, const string& signature) const;
```

Arbitrary message signing with spend key or view key.

### Freeze & Output Management

```cpp
void freeze(size_t idx);
void freeze(const key_image& ki);
void thaw(size_t idx);
void thaw(const key_image& ki);
bool frozen(size_t idx) const;
bool frozen(const key_image& ki) const;
bool frozen(const transfer_details& td) const;
bool frozen(const multisig_tx_set& txs) const;
```

Freeze/thaw individual outputs to prevent them from being spent.

```cpp
void discard_unmixable_outputs();
vector<size_t> select_available_outputs_from_histogram(uint64_t count, bool atleast, bool unlocked, bool allow_rct);
vector<size_t> select_available_outputs(const function<bool(const transfer_details&)>& f);
vector<size_t> select_available_unmixable_outputs();
vector<size_t> select_available_mixable_outputs();
```

Output selection utilities.

### Ring Database

```cpp
bool set_ring_database(const string& filename);
bool get_ring(const key_image& ki, vector<uint64_t>& outs);
bool get_rings(const hash& txid, vector<pair<key_image, vector<uint64_t>>>& outs);
bool set_ring(const key_image& ki, const vector<uint64_t>& outs, bool relative);
bool unset_ring(const vector<key_image>& key_images);
bool unset_ring(const hash& txid);
```

Persistent ring member storage for key reuse avoidance.

```cpp
bool blackball_output(const pair<uint64_t, uint64_t>& output);
bool set_blackballed_outputs(const vector<pair<uint64_t, uint64_t>>& outputs, bool add = false);
bool unblackball_output(const pair<uint64_t, uint64_t>& output);
bool is_output_blackballed(const pair<uint64_t, uint64_t>& output) const;
```

Output blacklisting (known-spent outputs excluded from ring selection).

### Background Sync

```cpp
void setup_background_sync(BackgroundSyncType type, const wipeable_string& wallet_password, const optional<wipeable_string>& background_cache_password);
void start_background_sync();
void stop_background_sync(const wipeable_string& wallet_password, const secret_key& spend_secret_key = null_skey);
bool is_background_syncing() const;
```

Background sync allows the wallet to sync using only the view key (spend key wiped from memory) for improved security. On `start_background_sync()`, the spend key is forgotten via `m_account.forget_spend_key()`. On `stop_background_sync()`, the wallet password is verified, the spend key is recovered, and background-synced transactions are reprocessed with full key image computation.

### Encryption Utilities

```cpp
string encrypt(const char* plaintext, size_t len, const secret_key& skey, bool authenticated = true) const;
string encrypt_with_view_secret_key(const string& plaintext, bool authenticated = true) const;
template<typename T> T decrypt(const string& ciphertext, const secret_key& skey, bool authenticated = true) const;
string decrypt_with_view_secret_key(const string& ciphertext, bool authenticated = true) const;
```

General-purpose ChaCha20 encryption using wallet keys.

### URI

```cpp
string make_uri(const string& address, const string& payment_id, uint64_t amount, const string& tx_description, const string& recipient_name, string& error) const;
bool parse_uri(const string& uri, string& address, string& payment_id, uint64_t& amount, string& tx_description, string& recipient_name, vector<string>& unknown_parameters, string& error);
```

Monero URI (`monero:` scheme) generation and parsing.

### Fee Estimation

```cpp
static uint64_t estimate_fee(bool use_per_byte_fee, bool use_rct, int n_inputs, int mixin, int n_outputs, size_t extra_size, bool bulletproof, bool clsag, bool bulletproof_plus, bool use_view_tags, uint64_t base_fee, uint64_t fee_quantization_mask);
uint64_t get_fee_multiplier(fee_priority priority, fee_algorithm algo = Unset);
uint64_t get_base_fee(fee_priority priority);
uint64_t get_base_fee();
uint64_t get_fee_quantization_mask();
pair<size_t, uint64_t> estimate_tx_size_and_weight(bool use_rct, int n_inputs, int ring_size, int n_outputs, size_t extra_size);
```

### Miscellaneous

```cpp
void set_attribute(const string& key, const string& value);
bool get_attribute(const string& key, string& value) const;
void set_tx_note(const hash& txid, const string& note);
string get_tx_note(const hash& txid) const;
void set_description(const string& description);
string get_description() const;
vector<address_book_row> get_address_book() const;
bool add_address_book_row(...);
bool set_address_book_row(...);
bool delete_address_book_row(size_t row_id);
bool check_connection(uint32_t* version = NULL, bool* ssl = NULL, uint32_t timeout = 200000, ...);
bool is_synced();
uint64_t get_blockchain_height_by_date(uint16_t year, uint8_t month, uint8_t day);
vector<pair<uint64_t, uint64_t>> estimate_backlog(const vector<pair<double, double>>& fee_levels);
```

## Internal Logic

### Refresh Loop (wallet2.cpp:4074)

The refresh algorithm uses a pipelined producer-consumer pattern:

1. **Build short chain history**: Exponentially-spaced block hashes from the wallet's local chain, sent to the daemon to find the fork point.
2. **Fast refresh**: If the wallet's start height is ahead of the current chain, fetch only block hashes (no full blocks) up to that height using `fast_refresh()`.
3. **Main loop**: Alternate between fetching and processing:
   - Submit `pull_and_parse_next_blocks()` to the thread pool (fetches blocks from daemon, parses them)
   - Simultaneously process previously fetched blocks via `process_parsed_blocks()`
   - Continue until no new blocks or `stop()` is called
4. **Pool processing**: After block sync, process pool transactions gathered during the block pull.
5. **Error handling**: Retries up to 3 times on transient errors. Handles hash chain bounds errors by resetting and rebuilding.

### Output Scanning (process_parsed_blocks, wallet2.cpp:3243)

Processing a batch of blocks:

1. **Parallel TX caching**: Submit all transactions to thread pool for `cache_tx_data()` -- extracts tx extra fields, public keys.
2. **Key derivation**: In parallel, compute key derivations for each tx public key using the wallet's view secret key.
3. **Output matching**: For each transaction output, test whether it belongs to any wallet subaddress using `is_out_to_acc_precomp()` with the pre-computed derivations. Uses view tags (from hard fork v15+) for early rejection.
4. **Process matches**: For each owned output, call `process_new_transaction()` which:
   - Checks unconfirmed transactions (`process_unconfirmed`)
   - Computes key images (or defers for multisig/background sync)
   - Decodes RingCT amounts
   - Creates `transfer_details` entries
   - Detects and handles self-spends
   - Triggers callbacks (`on_money_received`, `on_money_spent`)

### Key Image Generation (scan_output, wallet2.cpp:2208)

For each received output:
1. If keys are encrypted (`AskPasswordToDecrypt`), prompt for password on first receipt
2. For multisig or background-syncing wallets: set null key image (deferred)
3. For normal wallets: call `generate_key_image_helper_precomp()` with the account keys, output public key, and derivation to produce the key image and ephemeral keypair
4. Decode RingCT amount using `decodeRct()` with the derivation
5. Verify output public key matches derived ephemeral key

### Pool Handling (update_pool_state, wallet2.cpp:3751)

Two strategies:
1. **Incremental** (preferred): Use `getblocks.bin` with `POOL_ONLY` request type and `pool_info_since` timestamp for delta updates.
2. **Legacy**: Call `get_transaction_pool_hashes.bin`, diff against known pool state, fetch full transactions for new entries.

Both methods feed into `process_pool_state()` which calls `process_new_transaction()` with `pool = true`.

### Decoy Selection (gamma_picker)

Ring member (decoy) selection uses a gamma distribution (shape=19.28, scale=1/1.61) to model the age distribution of spent outputs, with parameters derived from the Monero Research Lab paper (monerolink.pdf). 50% of decoys are from the recent zone (~1.8 days). The `get_outs()` method fetches decoy outputs from the daemon and validates their public keys.

### Background Sync Process

1. **Start**: Forget spend key, reset background sync data, set `m_background_syncing = true`
2. **During sync**: Wallet refreshes normally but with null key images (view-key-only scanning). Transactions are stored in `m_background_sync_data.txs`.
3. **Stop**: Verify password, recover spend key, reload wallet state from disk (if custom password mode), reprocess all background-synced transactions with full key image computation.

## Dependencies

### What wallet2 depends on:

| Dependency | Purpose |
|-----------|---------|
| `cryptonote::account_base` | Cryptographic account (key generation, encryption) |
| `cryptonote::transaction` / `block` | Core data structures |
| `crypto::*` | Key derivation, key images, signatures, ChaCha20, Keccak |
| `ringct::*` | RingCT operations (amount encoding/decoding, signatures) |
| `multisig::multisig_account` | Multisig key exchange and management |
| `epee::net_utils::http` | HTTP client for daemon RPC |
| `tools::ringdb` | Persistent ring member database (LMDB) |
| `tools::NodeRPCProxy` | Caching proxy for daemon RPC calls |
| `mms::message_store` | Multisig messaging system |
| `hw::device` | Hardware wallet abstraction |
| `cryptonote::checkpoints` | Blockchain checkpoints |
| `crypto::ElectrumWords` | Mnemonic seed word encoding |
| `tools::threadpool` | Parallel block processing |
| `boost::serialization` | Legacy cache serialization |
| `rapidjson` | Keys file JSON serialization |

### What depends on wallet2:

| Consumer | Purpose |
|---------|---------|
| `src/simplewallet/simplewallet.h` | CLI wallet interface |
| `src/wallet/wallet_rpc_server.h` | RPC wallet server |
| `src/wallet/api/wallet.h` | C++ wallet API (libwallet) |
| `src/wallet/api/pending_transaction.h` | Pending TX wrapper |
| `src/wallet/api/unsigned_transaction.h` | Unsigned TX wrapper |
| `src/wallet/api/address_book.h` | Address book API |
| `src/wallet/api/subaddress.h` | Subaddress API |
| `src/wallet/api/subaddress_account.h` | Subaddress account API |
| `src/gen_multisig/gen_multisig.cpp` | Multisig wallet generation tool |
| `tests/unit_tests/` | Various unit tests |

## Configuration

### Command-Line Options (registered in `init_options`)

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `--daemon-address` | string | "" | Daemon host:port |
| `--daemon-host` | string | "" | Daemon host |
| `--daemon-port` | int | 0 | Daemon port (default 18081) |
| `--daemon-login` | string | "" | Daemon username:password |
| `--proxy` | string | "" | SOCKS proxy for daemon connections |
| `--trusted-daemon` | bool | false | Enable trusted daemon commands |
| `--untrusted-daemon` | bool | false | Force untrusted daemon |
| `--testnet` | bool | false | Use testnet |
| `--stagenet` | bool | false | Use stagenet |
| `--shared-ringdb-dir` | string | `~/.shared-ringdb` | Ring database directory |
| `--kdf-rounds` | uint64 | 1 | Password KDF rounds |
| `--hw-device` | string | "" | Hardware device name |
| `--hw-device-deriv-path` | string | "" | Hardware device derivation path (SLIP-10) |
| `--tx-notify` | string | "" | Program to run for incoming transactions (%s = txid) |
| `--no-dns` | bool | false | Disable DNS |
| `--offline` | bool | false | Offline mode (no daemon, no DNS) |
| `--extra-entropy` | string | "" | File with extra PRNG entropy |
| `--password` | string | "" | Wallet password |
| `--daemon-ssl` | string | "autodetect" | SSL mode: enabled/disabled/autodetect |
| `--daemon-ssl-private-key` | string | "" | PEM private key path |
| `--daemon-ssl-certificate` | string | "" | PEM certificate path |
| `--allow-mismatched-daemon-version` | bool | false | Allow version-mismatched daemon |

### Compile-Time Constants (wallet2.cpp)

| Constant | Value | Description |
|----------|-------|-------------|
| `APPROXIMATE_INPUT_BYTES` | 80 | Estimated bytes per input for tx sizing |
| `TX_WEIGHT_TARGET(bytes)` | bytes*2/3 | Target block weight |
| `RECENT_OUTPUT_RATIO` | 0.5 | 50% decoys from recent zone |
| `RECENT_OUTPUT_DAYS` | 1.8 | Recent zone length in days |
| `FEE_ESTIMATE_GRACE_BLOCKS` | 10 | Fee estimate validity window |
| `SUBADDRESS_LOOKAHEAD_MAJOR` | 50 | Default major index lookahead |
| `SUBADDRESS_LOOKAHEAD_MINOR` | 200 | Default minor index lookahead |
| `GAMMA_SHAPE` | 19.28 | Gamma distribution shape parameter |
| `GAMMA_SCALE` | 1/1.61 | Gamma distribution scale parameter |
| `DEFAULT_MIN_OUTPUT_COUNT` | 5 | Minimum output count before warning |
| `DEFAULT_MIN_OUTPUT_VALUE` | 2*COIN | Minimum output value before warning |
| `DEFAULT_INACTIVITY_LOCK_TIMEOUT` | 90 | Lock timeout in seconds |
| `FIRST_REFRESH_GRANULARITY` | 1024 | Short chain history granularity for first refresh |
| `SEGREGATION_FORK_HEIGHT` | 99999999 | Segregation fork height (effectively disabled) |
| `DEFAULT_UNLOCK_TIME` | SPENDABLE_AGE * TARGET_V2 | Default unlock time in seconds |
| `RECENT_SPEND_WINDOW` | 15 * TARGET_V2 | Recent spend detection window |

### File Magic Strings

| Magic | Value |
|-------|-------|
| Unsigned TX | `"Monero unsigned tx set\005"` |
| Signed TX | `"Monero signed tx set\005"` |
| Multisig unsigned TX | `"Monero multisig unsigned tx set\001"` |
| Key image export | `"Monero key image export\003"` |
| Multisig export | `"Monero multisig export\001"` |
| Output export | `"Monero output export\004"` |
| ASCII output | `"MoneroAsciiDataV1"` |
| Multisig signature | `"SigMultisigPkV1"` |

### Serialization Versions

| Type | Version |
|------|---------|
| `wallet2` (Boost) | 31 |
| `wallet2` (new binary) | 2 |
| `transfer_details` | 12 |
| `payment_details` | 5 |
| `pool_payment_details` | 1 |
| `unconfirmed_transfer_details` | 8 |
| `confirmed_transfer_details` | 6 |
| `address_book_row` | 18 |
| `background_synced_tx_t` | 0 |
| `background_sync_data_t` | 0 |

## Refresh Protocol (Wallet-Daemon RPC Sequence)

Source: `wallet2::refresh()` at wallet2.cpp:4074, `wallet2::pull_blocks()`, `wallet2::pull_and_parse_next_blocks()`.

### Sync Sequence

A full wallet refresh follows this RPC sequence:

1. **Get daemon info:** `get_info` RPC → obtain `height` and `top_block_hash`. This determines whether the wallet needs to sync.

2. **Build short chain history:** Construct an exponentially-spaced list of known block hashes from the wallet's local `m_blockchain` hashchain. The spacing doubles at each step (heights: tip, tip-1, tip-2, tip-4, tip-8, ..., 0). Always ends with the genesis hash.

3. **Fast refresh (if needed):** If `start_height` is ahead of the wallet's current chain, fetch only block hashes (via `getblocks.bin` with no tx data) up to that height using `fast_refresh()`.

4. **Pipelined block fetch loop:**
   - Submit `pull_and_parse_next_blocks()` to the thread pool → fetches blocks from daemon via `getblocks.bin` RPC with the short chain history.
   - Simultaneously process previously fetched blocks via `process_parsed_blocks()`.
   - The `getblocks.bin` request includes: block IDs (short history), start_height, prune flag, and pool info request.
   - Continue until daemon reports no new blocks or `stop()` is called.

5. **Pool processing:** After block sync completes, process pool transactions gathered during block pulls.

6. **Error handling:** Retry up to 3 times on transient errors. Handle hash chain bounds errors by resetting.

### RPC Calls During Refresh

| Step | RPC Call | Purpose |
|------|----------|---------|
| Info check | `get_info` | Get current height and top hash |
| Block fetch | `getblocks.bin` | Fetch blocks with short chain history; includes pool info |
| Output details | `get_outs.bin` | Fetch output keys for ring member validation (during tx construction, not refresh) |
| Fee estimate | `get_fee_estimate` | Get current fee parameters (during tx construction) |

## Output Scanning Pipeline

Source: `wallet2::process_parsed_blocks()` at wallet2.cpp:3243, `wallet2::process_new_transaction()`.

### Scanning Algorithm

For each block in a batch:

1. **Parallel TX caching:** Submit all transactions to the thread pool for `cache_tx_data()` — extracts tx extra fields, public keys.

2. **Key derivation:** In parallel, compute key derivations for each tx public key using the wallet's view secret key: `derivation = generate_key_derivation(tx_pub_key, view_secret_key)`.

3. **Output matching:** For each transaction output at index `i`:
   - **View tag check (HF v15+):** Compute expected view tag from derivation and output index. If it doesn't match the output's view tag, skip (early rejection — saves ~99.6% of full derivations).
   - **Full derivation:** `derive_public_key(derivation, i, spend_public_key)` → compute the expected output public key.
   - **Compare:** If derived key matches the output key, this output belongs to the wallet.

4. **Subaddress scanning:** For each output, try derivation against all subaddress spend public keys in the lookahead range (default: 50 major × 200 minor indices). The wallet maintains a map `m_subaddresses: spend_public_key → subaddress_index` for O(1) lookup.

5. **Process match** (in `process_new_transaction()`):
   - Compute key image (or defer for multisig/background sync).
   - Decode RingCT amount using `decodeRct()` with the derivation.
   - Create `transfer_details` entry in `m_transfers`.
   - Detect and handle self-spends.
   - Trigger callbacks (`on_money_received`, `on_money_spent`).

## Pool Synchronization

Source: `wallet2::update_pool_state()` at wallet2.cpp:3751, `wallet2::process_pool_state()`.

### Incremental Pool Sync (Preferred)

1. Use `getblocks.bin` with `POOL_ONLY` request type and `pool_info_since` timestamp.
2. Daemon returns only pool transactions added since the last known timestamp.
3. Feed into `process_pool_state()` → calls `process_new_transaction()` with `pool = true`.

### Legacy Pool Sync (Fallback)

1. Call `get_transaction_pool_hashes.bin` → get all pool tx hashes.
2. Diff against wallet's known pool state (`m_unconfirmed_payments`).
3. For new hashes: fetch full transactions via `get_transactions`.
4. For removed hashes: remove from `m_unconfirmed_payments`.

### Key Properties

- Pool transactions are NOT persisted to the wallet file — they are re-scanned on each refresh.
- Pool transactions appear in `get_transfers` with `type: "pool"`.
- When a pool tx is confirmed in a block, it transitions from `m_unconfirmed_payments` to `m_payments`.

## Reorg Detection and Handling

Source: `wallet2::process_blocks()`, `wallet2::detach_blockchain()`.

### Detection

During block processing, the wallet compares received block hashes with its local `m_blockchain` hashchain:
- If the daemon's blocks at a given height have a different hash than the wallet's stored hash, a fork/reorg has occurred.
- The wallet scans backward to find the common ancestor (last matching hash).

### Handling

1. **Detach:** `detach_blockchain(reorg_height)` removes wallet state from `reorg_height` onward:
   - Remove block hashes from `m_blockchain` above `reorg_height`.
   - For each `transfer_details` received at or above `reorg_height`: mark as unspent, clear key image spent status.
   - Move confirmed outgoing transactions (`m_confirmed_txs`) back to unconfirmed (`m_unconfirmed_txs`) if their block height >= `reorg_height`.
   - Remove incoming payments at or above `reorg_height`.

2. **Re-scan:** The wallet re-processes blocks from the common ancestor height using the normal scanning pipeline.

3. **Balance recalculation:** After re-scan, balances are recalculated from the updated `m_transfers` state. Transactions that were in the old chain but not in the new chain will have their effects reversed.

### Maximum Reorg Depth

The wallet limits reorg depth to `m_max_reorg_depth` (default: `ORPHANED_BLOCKS_MAX_COUNT`). Deeper reorgs require manual intervention (rescan_blockchain).

## Known Issues

The following TODO/FIXME/HACK/XXX comments were found:

- `wallet2.h:343` -- `TODO: key_image stored twice :(` -- The key image is stored redundantly in both `transfer_details::m_key_image` and the key image map.
- `wallet2.h:1945` -- `TODO: auto-calc this value or request from daemon, now use some fixed value` -- `m_upper_transaction_weight_limit` is not dynamically determined.
- `wallet2.cpp:1913` -- `TODO: handle this sweep case` -- Incomplete handling of a sweep transaction edge case.
- `wallet2.cpp:3699` -- `TODO: set tx_propagation_timeout to CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE * 3 / 2 after v15 hardfork` -- Dandelion++ timeout not yet adjusted for post-v15.
- `wallet2.cpp:4003` -- `FIXME: this isn't right, but simplewallet just logs that we got a block.` -- Incorrect block notification behavior.
- `wallet2.cpp:4105` -- `TODO moneromooo-monero says this about the "refreshed" variable:` -- Known subtle race condition with txpool state that was never fully resolved after a code reorder for timing leak fix.
- `wallet2.cpp:7352` -- `XXX: this needs to be fast, so we'd need to get the starting heights` -- Performance concern in height calculation logic.
