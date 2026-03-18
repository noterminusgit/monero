# Wallet RPC Server

## Overview

The wallet RPC server (`monero-wallet-rpc`) provides a JSON-RPC 2.0 interface for programmatic access to Monero wallet functionality. It wraps the `wallet2` library and exposes endpoints for balance queries, transfers, address management, multisig operations, cryptographic proofs, and wallet lifecycle management. The server runs as a standalone daemon process, listening on a configurable HTTP port with optional digest authentication and SSL/TLS support. It is the primary interface used by external applications, exchanges, and services to interact with Monero wallets without requiring the interactive CLI.

## Key Files

| File | Lines | Description |
|------|-------|-------------|
| `src/wallet/wallet_rpc_server.h` | 296 | Class declaration for `wallet_rpc_server`, URI-to-handler mapping via macros, handler signatures, helper methods, and member variables |
| `src/wallet/wallet_rpc_server.cpp` | 5166 | Full implementation of all RPC handlers, server lifecycle (`init`/`run`/`stop`), auto-refresh logic, error handling, `main()` entry point, and the `t_daemon`/`t_executor` wrapper classes |
| `src/wallet/wallet_rpc_server_commands_defs.h` | 2840 | Request/response struct definitions for every RPC command, RPC version constants (`WALLET_RPC_VERSION_MAJOR=1`, `WALLET_RPC_VERSION_MINOR=29`), shared data types (`transfer_entry`, `transfer_destination`, `payment_details`) |
| `src/wallet/wallet_rpc_server_error_codes.h` | 86 | Numeric error code definitions (`-1` through `-53`) for all wallet RPC error conditions |

## Data Structures

### Shared Types

**`transfer_destination`** -- Represents a single output destination in a transfer.
- `amount` (uint64): Amount in atomic units
- `address` (string): Destination Monero address

**`transfer_entry`** -- Represents a wallet transfer record (incoming, outgoing, pending, failed, or pool).
- `txid`, `payment_id`, `height`, `timestamp`, `amount`, `amounts` (per-output breakdown), `fee`, `note`, `destinations`, `type` ("in", "out", "pending", "failed", "pool", "block"), `unlock_time`, `locked`, `subaddr_index`, `subaddr_indices`, `address`, `double_spend_seen`, `confirmations`, `suggested_confirmations_threshold`

**`payment_details`** -- Legacy payment record.
- `payment_id`, `tx_hash`, `amount`, `block_height`, `unlock_time`, `locked`, `subaddr_index`, `address`

**`transfer_details`** -- Detailed information about an owned output (used by `incoming_transfers`).
- `amount`, `spent`, `global_index`, `tx_hash`, `subaddr_index`, `key_image`, `pubkey`, `block_height`, `frozen`, `unlocked`

**`single_transfer_response`** -- Response for single-transaction operations (`transfer`, `sweep_single`).
- `tx_hash`, `tx_key`, `amount`, `amounts_by_dest`, `fee`, `weight`, `tx_blob`, `tx_metadata`, `multisig_txset`, `unsigned_txset`, `spent_key_images`

**`split_transfer_response`** -- Response for multi-transaction operations (`transfer_split`, `sweep_all`, `sweep_dust`).
- Lists of: `tx_hash_list`, `tx_key_list`, `amount_list`, `amounts_by_dest_list`, `fee_list`, `weight_list`, `tx_blob_list`, `tx_metadata_list`, `spent_key_images_list`; plus `multisig_txset`, `unsigned_txset`

**`key_image_list`** / **`amounts_list`** -- Helper list types for key images and per-destination amounts.

**`uri_spec`** -- Monero URI fields: `address`, `payment_id`, `amount`, `tx_description`, `recipient_name`.

### Error Codes

Defined in `wallet_rpc_server_error_codes.h`:

| Code | Name | Description |
|------|------|-------------|
| -1 | `UNKNOWN_ERROR` | Catch-all error |
| -2 | `WRONG_ADDRESS` | Invalid Monero address |
| -3 | `DAEMON_IS_BUSY` | Daemon busy |
| -4 | `GENERIC_TRANSFER_ERROR` | Transfer construction failed |
| -5 | `WRONG_PAYMENT_ID` | Invalid payment ID |
| -6 | `TRANSFER_TYPE` | Invalid transfer type filter |
| -7 | `DENIED` | Command unavailable in restricted mode |
| -8 | `WRONG_TXID` | Invalid transaction ID |
| -9 | `WRONG_SIGNATURE` | Invalid signature |
| -10 | `WRONG_KEY_IMAGE` | Invalid key image |
| -11 | `WRONG_URI` | Invalid Monero URI |
| -12 | `WRONG_INDEX` | Index out of range |
| -13 | `NOT_OPEN` | No wallet file loaded |
| -14 | `ACCOUNT_INDEX_OUT_OF_BOUNDS` | Account index exceeds number of accounts |
| -15 | `ADDRESS_INDEX_OUT_OF_BOUNDS` | Address index exceeds subaddresses |
| -16 | `TX_NOT_POSSIBLE` | Transaction cannot be constructed |
| -17 | `NOT_ENOUGH_MONEY` | Insufficient balance |
| -18 | `TX_TOO_LARGE` | Transaction exceeds size limit (use `transfer_split`) |
| -19 | `NOT_ENOUGH_OUTS_TO_MIX` | Not enough outputs for ring construction |
| -20 | `ZERO_DESTINATION` | Transaction has no destination |
| -21 | `WALLET_ALREADY_EXISTS` | Wallet file already exists |
| -22 | `INVALID_PASSWORD` | Incorrect password |
| -23 | `NO_WALLET_DIR` | `--wallet-dir` not configured |
| -24 | `NO_TXKEY` | No tx secret key stored for this transaction |
| -25 | `WRONG_KEY` | Malformed key |
| -26 | `BAD_HEX` | Failed to parse hex input |
| -27 | `BAD_TX_METADATA` | Failed to parse tx metadata |
| -28 | `ALREADY_MULTISIG` | Wallet is already multisig |
| -29 | `WATCH_ONLY` | Command not supported by watch-only wallet |
| -30 | `BAD_MULTISIG_INFO` | Invalid multisig info data |
| -31 | `NOT_MULTISIG` | Wallet is not multisig (or not finalized) |
| -32 | `WRONG_LR` | Wrong LR values |
| -33 | `THRESHOLD_NOT_REACHED` | Not enough signers/participants |
| -34 | `BAD_MULTISIG_TX_DATA` | Failed to parse multisig tx data |
| -35 | `MULTISIG_SIGNATURE` | Multisig signing failed |
| -36 | `MULTISIG_SUBMISSION` | Multisig tx submission failed |
| -37 | `NOT_ENOUGH_UNLOCKED_MONEY` | Sufficient total balance but insufficient unlocked balance |
| -38 | `NO_DAEMON_CONNECTION` | Cannot connect to daemon |
| -39 | `BAD_UNSIGNED_TX_DATA` | Failed to parse unsigned tx data |
| -40 | `BAD_SIGNED_TX_DATA` | Failed to parse signed tx data |
| -41 | `SIGNED_SUBMISSION` | Signed tx submission failed |
| -42 | `SIGN_UNSIGNED` | Failed to sign unsigned tx |
| -43 | `NON_DETERMINISTIC` | Wallet is non-deterministic; seed unavailable |
| -44 | `INVALID_LOG_LEVEL` | Log level outside 0-4 range |
| -45 | `ATTRIBUTE_NOT_FOUND` | Requested wallet attribute not found |
| -46 | `ZERO_AMOUNT` | Amount is zero |
| -47 | `INVALID_SIGNATURE_TYPE` | Invalid signature type (must be "spend" or "view") |
| -48 | `DISABLED` | Multisig is disabled (experimental feature) |
| -49 | `PROXY_ALREADY_DEFINED` | Cannot set per-daemon proxy when `--proxy` is used |
| -50 | `NONZERO_UNLOCK_TIME` | Nonzero unlock_time is no longer allowed |
| -51 | `IS_BACKGROUND_WALLET` | Command disabled for background wallets |
| -52 | `IS_BACKGROUND_SYNCING` | Command disabled during background syncing |
| -53 | `INVALID_FEE_PRIORITY` | Priority must be 0-4 |

## RPC Endpoints

All endpoints are served via JSON-RPC 2.0 at the `/json_rpc` path. Some methods have legacy aliases (e.g., `getbalance` for `get_balance`, `getaddress` for `get_address`, `getheight` for `get_height`, `sweep_unmixable` for `sweep_dust`).

### Wallet Management

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `create_wallet` | Create a new wallet file in `--wallet-dir` | `filename`, `password`, `language` | (empty) |
| `open_wallet` | Open an existing wallet file | `filename`, `password`, `autosave_current` (default: true) | (empty) |
| `close_wallet` | Close the currently open wallet | `autosave_current` (default: true) | (empty) |
| `change_wallet_password` | Change the wallet encryption password | `old_password`, `new_password` | (empty) |
| `generate_from_keys` | Restore a wallet from address + view key (+ optional spend key) | `restore_height`, `filename`, `address`, `spendkey`, `viewkey`, `password`, `autosave_current`, `language` | `address`, `info` |
| `restore_deterministic_wallet` | Restore a wallet from a mnemonic seed | `restore_height`, `filename`, `seed`, `seed_offset`, `password`, `language`, `autosave_current`, `enable_multisig_experimental` | `address`, `seed`, `info`, `was_deprecated` |
| `stop_wallet` | Store wallet and stop the RPC server | (empty) | (empty) |
| `store` | Save the wallet state to disk | (empty) | (empty) |
| `set_daemon` | Configure the daemon connection | `address`, `username`, `password`, `trusted`, `ssl_support`, `ssl_private_key_path`, `ssl_certificate_path`, `ssl_ca_file`, `ssl_allowed_fingerprints`, `ssl_allow_any_cert`, `proxy` | (empty) |
| `get_version` | Get wallet RPC version and release status | (empty) | `version` (uint32 packed major/minor), `release` (bool) |

### Account Management

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `get_accounts` | List all accounts with balances | `tag` (filter), `strict_balances`, `regexp` | `total_balance`, `total_unlocked_balance`, `subaddress_accounts[]` (each: `account_index`, `base_address`, `balance`, `unlocked_balance`, `label`, `tag`) |
| `create_account` | Create a new subaddress account | `label` | `account_index`, `address` |
| `label_account` | Set label for an account | `account_index`, `label` | (empty) |
| `get_account_tags` | Get all account tag definitions | (empty) | `account_tags[]` (each: `tag`, `label`, `accounts[]`) |
| `tag_accounts` | Apply a tag to specified accounts | `tag`, `accounts` (set of indices) | (empty) |
| `untag_accounts` | Remove tags from specified accounts | `accounts` (set of indices) | (empty) |
| `set_account_tag_description` | Set the description for an account tag | `tag`, `description` | (empty) |

### Address Management

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `get_address` | Get addresses for an account (aliases: `getaddress`) | `account_index`, `address_index[]` (optional filter) | `address` (primary), `addresses[]` (each: `address`, `label`, `address_index`, `used`) |
| `get_address_index` | Look up the account/subaddress index for an address | `address` | `index` (major/minor) |
| `create_address` | Create new subaddresses (1 to 65536 at once) | `account_index`, `count` (default: 1), `label` | `address`, `address_index`, `addresses[]`, `address_indices[]` |
| `label_address` | Set label for a subaddress | `index` (major/minor), `label` | (empty) |
| `set_subaddress_lookahead` | Set subaddress lookahead parameters (requires wallet password) | `password`, `major_idx`, `minor_idx` | (empty) |
| `validate_address` | Validate a Monero address | `address`, `any_net_type`, `allow_openalias` | `valid`, `integrated`, `subaddress`, `nettype`, `openalias_address` |
| `make_integrated_address` | Create an integrated address | `standard_address` (optional), `payment_id` (optional, random if empty) | `integrated_address`, `payment_id` |
| `split_integrated_address` | Decompose an integrated address | `integrated_address` | `standard_address`, `payment_id`, `is_subaddress` |

### Transfer Operations

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `transfer` | Send XMR (single transaction only; errors if split needed) | `destinations[]`, `account_index`, `subaddr_indices`, `subtract_fee_from_outputs`, `priority` (0-4), `ring_size`, `unlock_time` (must be 0), `payment_id`, `get_tx_key`, `do_not_relay`, `get_tx_hex`, `get_tx_metadata` | `tx_hash`, `tx_key`, `amount`, `amounts_by_dest`, `fee`, `weight`, `tx_blob`, `tx_metadata`, `multisig_txset`, `unsigned_txset`, `spent_key_images` |
| `transfer_split` | Send XMR allowing automatic transaction splitting | Same as `transfer` (uses `get_tx_keys` instead of `get_tx_key`) | Lists of: `tx_hash_list`, `tx_key_list`, `amount_list`, `fee_list`, `weight_list`, etc. |
| `sweep_all` | Send all unlocked balance to one address | `address`, `account_index`, `subaddr_indices`, `subaddr_indices_all`, `priority`, `ring_size`, `outputs`, `unlock_time`, `payment_id`, `get_tx_keys`, `below_amount`, `do_not_relay`, `get_tx_hex`, `get_tx_metadata` | Split transfer response |
| `sweep_single` | Sweep a single output by key image | `address`, `priority`, `ring_size`, `outputs`, `unlock_time`, `payment_id`, `get_tx_key`, `key_image`, `do_not_relay`, `get_tx_hex`, `get_tx_metadata` | Single transfer response |
| `sweep_dust` / `sweep_unmixable` | Sweep unmixable dust outputs | `get_tx_keys`, `do_not_relay`, `get_tx_hex`, `get_tx_metadata` | Split transfer response |
| `sign_transfer` | Sign an unsigned transaction set (cold-signing workflow) | `unsigned_txset` (hex), `export_raw`, `get_tx_keys` | `signed_txset`, `tx_hash_list`, `tx_raw_list`, `tx_key_list` |
| `describe_transfer` | Describe an unsigned or multisig transaction set without signing | `unsigned_txset`, `multisig_txset` | `desc[]` (each: `amount_in`, `amount_out`, `ring_size`, `unlock_time`, `recipients[]`, `payment_id`, `change_amount`, `change_address`, `fee`, `dummy_outputs`, `extra`), `summary` |
| `submit_transfer` | Submit a previously signed transaction set | `tx_data_hex` | `tx_hash_list` |
| `relay_tx` | Relay a previously created (do_not_relay) transaction | `hex` (tx metadata) | `tx_hash` |
| `estimate_tx_size_and_weight` | Estimate transaction size and weight | `n_inputs`, `n_outputs`, `ring_size`, `rct` (default: true) | `size`, `weight` |
| `get_default_fee_priority` | Get the daemon-adjusted default fee priority | (empty) | `priority` (uint32) |

### Query Operations

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `get_balance` (alias: `getbalance`) | Get wallet balance | `account_index`, `address_indices`, `all_accounts`, `strict` | `balance`, `unlocked_balance`, `multisig_import_needed`, `per_subaddress[]` (each: `account_index`, `address_index`, `address`, `balance`, `unlocked_balance`, `label`, `num_unspent_outputs`, `blocks_to_unlock`, `time_to_unlock`), `blocks_to_unlock`, `time_to_unlock` |
| `get_height` (alias: `getheight`) | Get wallet's synced blockchain height | (empty) | `height` |
| `get_transfers` | Get filtered list of wallet transfers | `in`, `out`, `pending`, `failed`, `pool` (booleans), `filter_by_height`, `min_height`, `max_height`, `account_index`, `subaddr_indices`, `all_accounts` | `in[]`, `out[]`, `pending[]`, `failed[]`, `pool[]` (each is a list of `transfer_entry`) |
| `get_transfer_by_txid` | Look up a specific transfer by txid | `txid`, `account_index` (default: 0) | `transfer` (single entry, backward compat), `transfers[]` (may contain multiple matches) |
| `get_payments` | Get incoming payments by payment ID | `payment_id` | `payments[]` |
| `get_bulk_payments` | Get payments for multiple payment IDs above a block height | `payment_ids[]`, `min_block_height` | `payments[]` |
| `incoming_transfers` | List owned outputs | `transfer_type` ("all", "available", "unavailable"), `account_index`, `subaddr_indices` | `transfers[]` (each: `amount`, `spent`, `global_index`, `tx_hash`, `subaddr_index`, `key_image`, `pubkey`, `block_height`, `frozen`, `unlocked`) |
| `get_tx_notes` | Get notes attached to transaction IDs | `txids[]` | `notes[]` |
| `get_attribute` | Get a wallet attribute by key | `key` | `value` |

### Key Management

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `query_key` | Retrieve wallet secret keys or mnemonic seed | `key_type` ("mnemonic", "view_key", "spend_key") | `key` |
| `get_tx_key` | Get the secret tx key for a sent transaction | `txid` | `tx_key` |
| `check_tx_key` | Check amount received at an address using a tx key | `txid`, `tx_key`, `address` | `received`, `in_pool`, `confirmations` |
| `get_tx_proof` | Generate a proof that a transaction was sent to an address | `txid`, `address`, `message` | `signature` |
| `check_tx_proof` | Verify a transaction proof | `txid`, `address`, `message`, `signature` | `good`, `received`, `in_pool`, `confirmations` |
| `get_spend_proof` | Generate a proof that a transaction was spent by the wallet | `txid`, `message` | `signature` |
| `check_spend_proof` | Verify a spend proof | `txid`, `message`, `signature` | `good` |
| `get_reserve_proof` | Generate a proof of reserves (balance ownership) | `all`, `account_index`, `amount`, `message` | `signature` |
| `check_reserve_proof` | Verify a reserve proof | `address`, `message`, `signature` | `good`, `total`, `spent` |
| `sign` | Sign a message with spend or view key | `data`, `account_index`, `address_index`, `signature_type` ("spend" or "view") | `signature` |
| `verify` | Verify a signed message | `data`, `address`, `signature` | `good`, `version`, `old`, `signature_type` |
| `export_outputs` | Export wallet outputs for view-only wallets | `all`, `start`, `count` | `outputs_data_hex` |
| `import_outputs` | Import outputs into a view-only wallet | `outputs_data_hex` | `num_imported` |
| `export_key_images` | Export signed key images | `all` | `offset`, `signed_key_images[]` (each: `key_image`, `signature`) |
| `import_key_images` | Import signed key images (requires trusted daemon) | `offset`, `signed_key_images[]` | `height`, `spent`, `unspent` |

### Output Management

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `freeze` | Freeze an output by key image (prevent spending) | `key_image` | (empty) |
| `thaw` | Unfreeze a previously frozen output | `key_image` | (empty) |
| `frozen` | Check if an output is frozen | `key_image` | `frozen` (bool) |

### Multisig Operations

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `is_multisig` | Check if the wallet is multisig | (empty) | `multisig`, `kex_is_done`, `ready`, `threshold`, `total` |
| `prepare_multisig` | Prepare this wallet for multisig by exporting first key exchange message | `enable_multisig_experimental` | `multisig_info` |
| `make_multisig` | Create a multisig wallet from key exchange info | `multisig_info[]`, `threshold`, `password` | `address`, `multisig_info` (for additional rounds) |
| `exchange_multisig_keys` | Exchange multisig keys for additional rounds | `password`, `multisig_info[]`, `force_update_use_with_caution` | `address` (when ready), `multisig_info` |
| `get_multisig_key_exchange_booster` | Get key exchange booster data (allows pre-computation) | `password`, `multisig_info[]`, `threshold`, `num_signers` | `multisig_info` |
| `export_multisig_info` | Export multisig info for sharing with cosigners | (empty) | `info` (hex) |
| `import_multisig_info` | Import multisig info from cosigners | `info[]` (hex strings) | `n_outputs` |
| `sign_multisig` | Sign a multisig transaction | `tx_data_hex` | `tx_data_hex` (updated), `tx_hash_list` |
| `submit_multisig` | Submit a fully-signed multisig transaction | `tx_data_hex` | `tx_hash_list` |
| `finalize_multisig` | Deprecated/NOP -- always returns error with `CHECK_MULTISIG_ENABLED()` |  |  |

### Background Sync Operations

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `setup_background_sync` | Configure background sync mode | `background_sync_type`, `wallet_password`, `background_cache_password` | (empty) |
| `start_background_sync` | Start background syncing | (empty) | (empty) |
| `stop_background_sync` | Stop background syncing and restore full wallet | `wallet_password`, `seed` (optional), `seed_offset` (optional) | (empty) |

### Blockchain Sync

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `refresh` | Manually trigger a blockchain sync | `start_height` (default: 0) | `blocks_fetched`, `received_money` |
| `auto_refresh` | Enable or disable automatic periodic refresh | `enable` (default: true), `period` (seconds, default: 20) | (empty) |
| `rescan_blockchain` | Rescan the blockchain from scratch | `hard` (default: false) | (empty) |
| `rescan_spent` | Re-check spent status of outputs with daemon | (empty) | (empty) |
| `scan_tx` | Scan specific transaction IDs | `txids[]` | (empty) |

### Mining

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `start_mining` | Start mining on the daemon (requires trusted daemon) | `threads_count`, `do_background_mining`, `ignore_battery` | (empty) |
| `stop_mining` | Stop mining on the daemon | (empty) | (empty) |

### Address Book

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `get_address_book` | Get address book entries | `entries[]` (optional index filter; all if empty) | `entries[]` (each: `index`, `address`, `description`) |
| `add_address_book` | Add an address book entry | `address`, `description` | `index` |
| `edit_address_book` | Edit an existing entry | `index`, `set_address`, `address`, `set_description`, `description` | (empty) |
| `delete_address_book` | Delete an entry by index | `index` | (empty) |

### Misc

| Method | Description | Key Request Parameters | Key Response Fields |
|--------|-------------|----------------------|-------------------|
| `set_tx_notes` | Set notes on transactions | `txids[]`, `notes[]` | (empty) |
| `set_attribute` | Set a wallet attribute (key-value) | `key`, `value` | (empty) |
| `make_uri` | Create a Monero payment URI | `address`, `payment_id`, `amount`, `tx_description`, `recipient_name` | `uri` |
| `parse_uri` | Parse a Monero payment URI | `uri` | `uri` (parsed spec), `unknown_parameters[]` |
| `get_languages` | List available mnemonic seed languages | (empty) | `languages[]`, `languages_local[]` |
| `set_log_level` | Set the log verbosity level (0-4) | `level` | (empty) |
| `set_log_categories` | Set log categories string | `categories` | `categories` (applied categories) |

## Internal Logic

### Server Lifecycle

1. **Startup** (`main()` at line 5113): Parses command-line arguments, creates a `t_daemon` which owns the `wallet_rpc_server`. Three modes are supported:
   - `--wallet-file=<file>`: Opens an existing wallet file, performs initial refresh (unless `--no-initial-sync`), then starts the RPC server.
   - `--generate-from-json=<file>`: Generates a wallet from a JSON description file.
   - `--wallet-dir=<directory>`: Starts with no wallet loaded; wallets are opened/created dynamically via `open_wallet`, `create_wallet`, `generate_from_keys`, or `restore_deterministic_wallet` RPC calls.

2. **Initialization** (`init()`): Processes RPC configuration including bind address/port, SSL options, CORS, connection limits, and authentication. If authentication is not disabled and no login is provided, a random password is generated, written to `monero-wallet-rpc.<port>.login`, and logged.

3. **Run loop** (`run()`): Runs a single-threaded HTTP server with two idle handlers:
   - **Auto-refresh handler** (200ms evaluation interval): Periodically calls `wallet2::refresh()` in chunks of 256 blocks. Implements throttling when catching up to the chain tip to allow other RPC requests to be served.
   - **Stop handler** (500ms interval): Checks the `m_stop` atomic flag and sends the stop signal when set.

4. **Shutdown** (`stop()`): Calls `wallet2::store()` and `wallet2::deinit()`, then deletes the wallet object.

### Request Dispatch

The server uses epee's HTTP server framework with macro-based routing:
- `CHAIN_HTTP_TO_MAP2` forwards all HTTP requests to a URI map.
- `BEGIN_JSON_RPC_MAP("/json_rpc")` / `END_JSON_RPC_MAP()` defines the JSON-RPC dispatch table.
- `MAP_JON_RPC_WE` maps a method string (e.g., `"get_balance"`) to a handler function and command struct type.
- Each handler has the signature: `bool on_<name>(const request&, response&, epee::json_rpc::error&, const connection_context*)`.

### Authentication and Restricted Mode

- HTTP digest authentication is supported (enabled by default with a random password unless `--disable-rpc-login` is specified).
- The `--restricted-rpc` flag sets `m_restricted = true`, blocking write operations (transfers, wallet creation, key queries, etc.). Restricted mode only allows read-only/view operations.
- Commands that modify wallet state check `m_restricted` and return `WALLET_RPC_ERROR_CODE_DENIED` if restricted.

### Guard Macros

Three preprocessor macros gate command execution:

- **`CHECK_IF_BACKGROUND_SYNCING()`**: Returns error if wallet is null, is a background wallet, or is currently background syncing.
- **`CHECK_MULTISIG_ENABLED()`**: Returns error if wallet is multisig but multisig experimental mode is not enabled.
- **`PRE_VALIDATE_BACKGROUND_SYNC()`**: Returns error if wallet is null, in restricted mode, using HW device, multisig, or watch-only. Used by the three background sync endpoints.

### Transfer Validation

`validate_transfer()` is called by all transfer endpoints. It:
1. Resolves each destination address (including OpenAlias DNS lookups).
2. Extracts integrated payment IDs from addresses and adds them to `tx_extra`.
3. Rejects standalone payment IDs (obsolete; subaddresses or integrated addresses must be used instead).
4. Enforces at most one payment ID per transaction.

### Response Population

The templated `fill_response()` method populates transfer response fields (tx hash, key, amount, fee, weight, key images) from a vector of `pending_tx` objects. It handles three scenarios:
- **Multisig wallets**: Serializes the pending transactions as a multisig tx set hex string.
- **Watch-only wallets**: Serializes as an unsigned tx set hex string.
- **Normal wallets**: Commits the transactions (unless `do_not_relay` is true) and returns tx hashes.

### Error Handling

`handle_rpc_exception()` maps C++ exception types to specific RPC error codes:
- `error::no_connection_to_daemon` -> `-38`
- `error::daemon_busy` -> `-3`
- `error::not_enough_money` -> `-17`
- `error::tx_not_possible` -> `-16`
- `error::file_exists` -> `-21`
- `error::invalid_password` -> `-22`
- And several more, with a catch-all for `std::exception` using a caller-specified default code.

### Auto-Refresh Mechanism

The auto-refresh idle handler (default period: 20 seconds) implements a two-state model:
- **At chain tip**: Refreshes every `m_auto_refresh_period` seconds.
- **Catching up**: Processes blocks in 256-block chunks continuously, but throttles by yielding ~200-300ms every `m_auto_refresh_period` to allow RPC requests to be served.

The `auto_refresh` RPC endpoint controls the period (set to 0 to disable).

### Wallet Switching

The `open_wallet`, `create_wallet`, `generate_from_keys`, and `restore_deterministic_wallet` endpoints can switch the active wallet at runtime. They:
1. Optionally store the current wallet (controlled by `autosave_current`).
2. Delete the old `m_wallet` pointer.
3. Set `m_wallet` to the new wallet instance.

Filenames are restricted to simple names (no `/`, `\`, or `:` characters) and are resolved relative to `m_wallet_dir`.

## Dependencies

### This Module Depends On

| Dependency | Purpose |
|------------|---------|
| `wallet2` (`src/wallet/wallet2.h`) | Core wallet logic: balance queries, transaction construction, refresh, key management, multisig, proofs |
| `epee::http_server_impl_base` | HTTP server framework with JSON-RPC support |
| `cryptonote_basic` | Address parsing (`get_account_address_from_str`), transaction format utilities |
| `cryptonote_config.h` | Constants (`CRYPTONOTE_MAX_BLOCK_NUMBER`, `DIFFICULTY_TARGET_V2`) |
| `multisig/multisig.h` | Multisig account status types |
| `mnemonics/electrum-words.h` | Mnemonic seed language lists and validation |
| `rpc/rpc_args.h` | Shared RPC argument processing (bind IP, SSL options, CORS) |
| `rpc/core_rpc_server_commands_defs.h` | Daemon RPC types for start/stop mining |
| `daemonizer/daemonizer.h` | Process daemonization |
| `wallet_args` | Wallet-specific command-line argument processing |
| `fee_priority.h` | Fee priority validation and conversion |
| `common/i18n.h` | Internationalization support |
| `common/command_line.h` | Command-line argument handling |

### What Depends On This Module

- **`monero-wallet-rpc` binary**: The `.cpp` file contains `main()` and is compiled as a standalone executable.
- **External applications**: Any application using the Monero wallet RPC API (exchanges, payment processors, GUI wallets, etc.) communicates through this server.

## Configuration

### Command-Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--rpc-bind-port` | Port for the RPC server to listen on | (required) |
| `--disable-rpc-login` | Disable HTTP digest authentication | false |
| `--restricted-rpc` | Restrict to view-only commands | false |
| `--wallet-dir` | Directory for dynamically created/opened wallets | (none) |
| `--wallet-file` | Path to wallet file to open at startup | (none) |
| `--generate-from-json` | Path to JSON file for wallet generation | (none) |
| `--prompt-for-password` | Prompt for password interactively when not provided | false |
| `--no-initial-sync` | Skip the initial blockchain refresh before accepting connections | false |
| `--rpc-max-connections-per-public-ip` | Max RPC connections per public IP | `DEFAULT_RPC_MAX_CONNECTIONS_PER_PUBLIC_IP` |
| `--rpc-max-connections-per-private-ip` | Max RPC connections per private/localhost IP | `DEFAULT_RPC_MAX_CONNECTIONS_PER_PRIVATE_IP` |
| `--rpc-max-connections` | Max total RPC connections | `DEFAULT_RPC_MAX_CONNECTIONS` |
| `--rpc-response-soft-limit` | Max response bytes queued before enforcement | `DEFAULT_RPC_SOFT_LIMIT_SIZE` |

Additionally, all standard `wallet2` options (daemon address, proxy, testnet/stagenet, etc.) and `rpc_args` options (bind IP, SSL certificates, CORS origins, etc.) are supported.

### Compile-Time Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `DEFAULT_AUTO_REFRESH_PERIOD` | 20 | Default auto-refresh interval in seconds |
| `REFRESH_INDICATIVE_BLOCK_CHUNK_SIZE` | 256 | Max blocks per auto-refresh cycle |
| `WALLET_RPC_VERSION_MAJOR` | 1 | RPC protocol major version |
| `WALLET_RPC_VERSION_MINOR` | 29 | RPC protocol minor version |
| `default_rpc_username` | "monero" | Default username when auto-generating login |

## Known Issues

| Location | Comment |
|----------|---------|
| `wallet_rpc_server.cpp:2159` | `// TODO - should the whole thing fail because of one bad id?` -- In `on_get_bulk_payments`, a single invalid payment ID causes the entire request to fail. The developer questioned whether partial results should be returned instead. |
| `wallet_rpc_server.cpp:280` | `//DO NOT START THIS SERVER IN MORE THEN 1 THREADS WITHOUT REFACTORING` -- The server is explicitly single-threaded. Running with more than 1 thread would require refactoring to add thread safety. |
| `wallet_rpc_server.cpp:4497-4500` | `on_finalize_multisig` is a NOP stub that always fails via `CHECK_MULTISIG_ENABLED()` then `return false`. The command struct in the defs file is marked `// NOP`. This endpoint is deprecated in favor of `exchange_multisig_keys`. |
| `wallet_rpc_server.cpp:2316` | `res.key = std::string(seed.data(), seed.size()); // send to the network, then wipe RAM :D` -- Sardonic comment acknowledging that the mnemonic seed is transmitted in plaintext over the RPC connection, undermining the security of the in-memory `wipeable_string`. |
