# Wallet Transfers

## Overview

The wallet transfer subsystem (`wallet2`) handles the complete lifecycle of constructing, signing, and submitting Monero transactions. It implements input selection algorithms that prioritize privacy (minimizing output relatedness), fee calculation that adapts across hard fork versions (per-KB through per-byte with quantization masks), change output handling with dummy addresses for zero-change RCT transactions, and a multi-pass transaction construction pipeline that builds candidate transactions with estimated fees, then iteratively refines until actual fees converge. The system supports standard transfers, sweep-all, sweep-single, sweep-unmixable, multisig signing, cold signing on hardware devices, and offline unsigned/signed transaction workflows.

## Key Files

| File | Description |
|------|-------------|
| `src/wallet/wallet2.h` | Core wallet class declaration: all transfer-related structs (`pending_tx`, `tx_construction_data`, `unsigned_tx_set`, `signed_tx_set`, `transfer_details`, `multisig_sig`), public transfer API, and private helpers. |
| `src/wallet/wallet2.cpp` | Implementation of all transfer logic: `create_transactions_2`, `create_transactions_all`, `create_transactions_from`, `create_transactions_single`, `create_unmixable_sweep_transactions`, `transfer_selected`, `transfer_selected_rct`, `commit_tx`, `sign_tx`, `get_outs`, `pick_preferred_rct_inputs`, `gamma_picker`, fee calculation, and sanity checks. |
| `src/wallet/fee_priority.h` | `fee_priority` enum (`Default`, `Unimportant`, `Normal`, `Elevated`, `Priority`) and utility functions for clamping, conversion, and string representation. |
| `src/wallet/fee_algorithm.h` | `fee_algorithm` enum (`PreHardforkV3`, `HardforkV3`, `HardforkV5`, `HardforkV8`) mapping hard fork versions to fee calculation strategies. |
| `src/cryptonote_core/cryptonote_tx_utils.h` | `tx_destination_entry` struct and `construct_tx_and_get_tx_key()` -- the core transaction construction function called by the wallet. |
| `src/cryptonote_config.h` | Protocol-level constants: `FEE_PER_KB`, `FEE_PER_BYTE`, `BULLETPROOF_MAX_OUTPUTS` (16), block reward zones, and dynamic fee base values. |

## Data Structures

### `cryptonote::tx_destination_entry`

Defined in `src/cryptonote_core/cryptonote_tx_utils.h:73`.

Represents a single output destination in a transaction.

| Field | Type | Description |
|-------|------|-------------|
| `original` | `std::string` | Original address string (for display). |
| `amount` | `uint64_t` | Amount in atomic units to send to this destination. |
| `addr` | `account_public_address` | Destination address (view + spend public keys). |
| `is_subaddress` | `bool` | Whether the destination is a subaddress. |
| `is_integrated` | `bool` | Whether the destination is an integrated address. |

### `wallet2::transfer_details`

Defined in `src/wallet/wallet2.h:333`.

Represents a single owned output (UTXO) in the wallet.

| Field | Type | Description |
|-------|------|-------------|
| `m_block_height` | `uint64_t` | Block height where the output was received. |
| `m_tx` | `transaction_prefix` | The transaction prefix containing this output. |
| `m_txid` | `crypto::hash` | Transaction hash. |
| `m_internal_output_index` | `uint64_t` | Index of this output within its transaction. |
| `m_global_output_index` | `uint64_t` | Global output index on the blockchain. |
| `m_spent` | `bool` | Whether the output has been spent. |
| `m_frozen` | `bool` | Whether the output is frozen (excluded from spending). |
| `m_spent_height` | `uint64_t` | Block height where the output was spent (0 if unspent). |
| `m_key_image` | `crypto::key_image` | Key image for this output. |
| `m_mask` | `rct::key` | Commitment mask for RCT outputs. |
| `m_amount` | `uint64_t` | Amount of this output. |
| `m_rct` | `bool` | Whether this is a RingCT output. |
| `m_key_image_known` | `bool` | Whether the key image has been computed. |
| `m_key_image_request` | `bool` | For view wallets: request key image; for cold wallets: was requested. |
| `m_pk_index` | `uint64_t` | Index of the tx public key used for this output. |
| `m_subaddr_index` | `subaddress_index` | Subaddress account (major) and index (minor). |
| `m_key_image_partial` | `bool` | True if key image is partial (multisig incomplete). |
| `m_multisig_k` | `std::vector<rct::key>` | Multisig nonce values. |
| `m_multisig_info` | `std::vector<multisig_info>` | Multisig info from other participants. |
| `m_uses` | `std::vector<std::pair<uint64_t, crypto::hash>>` | Known uses of this output (height, txid). |

Key methods: `is_rct()`, `amount()`, `get_public_key()`.

### `wallet2::tx_construction_data`

Defined in `src/wallet/wallet2.h:546`.

Captures all data needed to reconstruct a transaction (used for offline signing).

| Field | Type | Description |
|-------|------|-------------|
| `sources` | `std::vector<tx_source_entry>` | Input sources with ring members and real output info. |
| `change_dts` | `tx_destination_entry` | Change output destination and amount. |
| `splitted_dsts` | `std::vector<tx_destination_entry>` | Final split destinations including change. |
| `selected_transfers` | `std::vector<size_t>` | Indices into `m_transfers` of selected inputs. |
| `extra` | `std::vector<uint8_t>` | Transaction extra field (payment IDs, etc.). |
| `unlock_time` | `uint64_t` | Unlock time (always 0 for normal transactions). |
| `use_rct` | `bool` | Whether RingCT is used. |
| `rct_config` | `rct::RCTConfig` | RCT configuration (range proof type and version). |
| `use_view_tags` | `bool` | Whether view tags are used (post-HF). |
| `dests` | `std::vector<tx_destination_entry>` | Original destinations (without change). |
| `subaddr_account` | `uint32_t` | Subaddress account used for inputs. |
| `subaddr_indices` | `std::set<uint32_t>` | Subaddress minor indices used as inputs. |
| `construction_flags` | `uint8_t` | Bitfield for serialization of bool flags. |

### `wallet2::pending_tx`

Defined in `src/wallet/wallet2.h:641`.

Represents a fully constructed transaction ready for submission.

| Field | Type | Description |
|-------|------|-------------|
| `tx` | `cryptonote::transaction` | The constructed transaction. |
| `dust` | `uint64_t` | Dust amount. |
| `fee` | `uint64_t` | Transaction fee. |
| `dust_added_to_fee` | `bool` | Whether dust was added to the fee. |
| `change_dts` | `tx_destination_entry` | Change destination and amount. |
| `selected_transfers` | `std::vector<size_t>` | Indices of inputs used. |
| `key_images` | `std::string` | Space-separated key image strings. |
| `tx_key` | `crypto::secret_key` | Transaction secret key. |
| `additional_tx_keys` | `std::vector<crypto::secret_key>` | Additional tx keys (for subaddress destinations). |
| `dests` | `std::vector<tx_destination_entry>` | Original destinations (without change). |
| `multisig_sigs` | `std::vector<multisig_sig>` | Multisig partial signatures (one per signer combination). |
| `multisig_tx_key_entropy` | `crypto::secret_key` | Entropy for multisig tx key generation. |
| `construction_data` | `tx_construction_data` | Full construction data for offline signing. |

### `wallet2::unsigned_tx_set`

Defined in `src/wallet/wallet2.h:682`.

Serializable set of unsigned transactions for offline signing workflows.

| Field | Type | Description |
|-------|------|-------------|
| `txes` | `std::vector<tx_construction_data>` | Transaction construction data for each transaction. |
| `transfers` | `tuple<uint64_t, uint64_t, transfer_container>` | Legacy format: (start, end, transfers). |
| `new_transfers` | `tuple<uint64_t, uint64_t, vector<exported_transfer_details>>` | Current format: compact export of transfer details. |

Supports versioned serialization (v0 legacy, v1 compact, v2 current).

### `wallet2::signed_tx_set`

Defined in `src/wallet/wallet2.h:714`.

Serializable set of signed transactions ready for submission.

| Field | Type | Description |
|-------|------|-------------|
| `ptx` | `std::vector<pending_tx>` | Signed pending transactions. |
| `key_images` | `std::vector<crypto::key_image>` | Key images for all wallet transfers. |
| `tx_key_images` | `unordered_map<public_key, key_image>` | Mapping of output public keys to key images for change outputs. |

### `wallet2::multisig_tx_set`

Defined in `src/wallet/wallet2.h:728`.

| Field | Type | Description |
|-------|------|-------------|
| `m_ptx` | `std::vector<pending_tx>` | Partially-signed transactions. |
| `m_signers` | `std::unordered_set<crypto::public_key>` | Public keys of signers who have signed. |

### `wallet2::multisig_sig`

Defined in `src/wallet/wallet2.h:609`.

Stores a single multisig signing attempt.

| Field | Type | Description |
|-------|------|-------------|
| `sigs` | `rct::rctSig` | The RingCT signature data. |
| `ignore` | `std::unordered_set<crypto::public_key>` | Signers excluded from this attempt. |
| `used_L` | `std::unordered_set<rct::key>` | Public L nonces used by this attempt. |
| `signing_keys` | `std::unordered_set<crypto::public_key>` | Public keys of signers who contributed. |
| `total_alpha_G` | `rct::keyM` | Aggregate public nonces (G component) per input per alpha component. |
| `total_alpha_H` | `rct::keyM` | Aggregate public nonces (H component) per input per alpha component. |
| `c_0` | `rct::keyV` | Challenge values per input. |
| `s` | `rct::keyV` | Partial signature values per input. |

### `tx_dust_policy`

Defined in `src/wallet/wallet2.h:163`.

| Field | Type | Description |
|-------|------|-------------|
| `dust_threshold` | `uint64_t` | Amount below which outputs are considered dust. |
| `add_to_fee` | `bool` | If true, dust is added to the fee instead of creating a dust output. |
| `addr_for_dust` | `account_public_address` | Address to send dust to if not added to fee. |

### `gamma_picker`

Defined in `src/wallet/wallet2.h:90`.

Picks random output indices from a gamma distribution for decoy selection.

| Field | Type | Description |
|-------|------|-------------|
| `gamma` | `std::gamma_distribution<double>` | Gamma distribution with shape=19.28, scale=1/1.61. |
| `rct_offsets` | `const std::vector<uint64_t>&` | Cumulative RCT output counts per block. |
| `num_rct_outputs` | `uint64_t` | Total number of spendable RCT outputs. |
| `average_output_time` | `double` | Average seconds between outputs (estimated from recent year). |

### Other Type Aliases

- `get_outs_entry` = `std::tuple<uint64_t, crypto::public_key, rct::key>` -- (global_output_index, output_public_key, commitment_mask) for ring members.
- `unique_index_container` = `std::set<uint32_t>` -- used for `subtract_fee_from_outputs` parameter.
- `transfer_container` = `std::vector<transfer_details>` -- the wallet's UTXO set.

## Public API

### `create_transactions_2`

```cpp
std::vector<pending_tx> create_transactions_2(
    std::vector<tx_destination_entry> dsts,
    const size_t fake_outs_count,
    fee_priority priority,
    const std::vector<uint8_t>& extra,
    uint32_t subaddr_account,
    std::set<uint32_t> subaddr_indices,
    const unique_index_container& subtract_fee_from_outputs = {});
```

Primary transaction creation method for standard transfers. Constructs one or more transactions to pay the specified destinations.

**Preconditions:**
- `dsts` must not be empty and all amounts must be non-zero.
- If `subtract_fee_from_outputs` is non-empty, its indices must be valid into `dsts`, and the total number of destinations must fit in one transaction (`<= BULLETPROOF_MAX_OUTPUTS - 1 = 15`).
- Sufficient unlocked balance must exist in the specified subaddress account/indices.

**Behavior:**
- Gathers all unspent, unfrozen, unlocked outputs from the specified subaddress account and indices, splitting them into dust and non-dust categories based on a fractional threshold.
- For RCT, attempts to find 1 or 2 preferred inputs that can cover the full amount plus estimated fee (`pick_preferred_rct_inputs`).
- Iteratively selects inputs, fills destinations, and constructs transactions. When a transaction reaches the weight target (2/3 of upper limit), it is finalized and a new transaction begins.
- Performs a two-phase construction: first pass with `TRANSACTION_CREATE_FAKE` device mode, then a final pass with `TRANSACTION_CREATE_REAL` mode.
- Fee is iteratively refined: estimate, construct trial transaction, recalculate fee from actual weight, repeat until convergence (up to 10 attempts).
- Runs `sanity_check()` on the result, verifying that all destinations receive at least the expected amounts via tx proofs.
- Supports `subtract_fee_from_outputs`: fee is distributed equally across designated destination outputs instead of being paid from change.

### `create_transactions_all`

```cpp
std::vector<pending_tx> create_transactions_all(
    uint64_t below,
    const cryptonote::account_public_address& address,
    bool is_subaddress,
    const size_t outputs,
    const size_t fake_outs_count,
    fee_priority priority,
    const std::vector<uint8_t>& extra,
    uint32_t subaddr_account,
    std::set<uint32_t> subaddr_indices);
```

Sweep all (or outputs below a threshold) to a single address. Used by the `sweep_all` command.

**Preconditions:**
- Must have unlocked balance in the specified subaddress account.
- If `below` is non-zero, only outputs with amounts less than `below` are selected.

**Behavior:**
- Gathers all eligible outputs (unspent, unfrozen, unlocked, not partial key image, matching subaddress).
- If no subaddress indices specified, picks a random non-empty subaddress (preferring non-zero indices).
- Delegates to `create_transactions_from()`.
- The `outputs` parameter controls how many destination outputs to split the transfer into.

### `create_transactions_single`

```cpp
std::vector<pending_tx> create_transactions_single(
    const crypto::key_image& ki,
    const cryptonote::account_public_address& address,
    bool is_subaddress,
    const size_t outputs,
    const size_t fake_outs_count,
    fee_priority priority,
    const std::vector<uint8_t>& extra);
```

Sweep a single output identified by its key image.

**Preconditions:**
- The key image must correspond to an unspent, unfrozen, unlocked output in the wallet.

**Behavior:**
- Finds the output matching the key image.
- Delegates to `create_transactions_from()`.

### `create_transactions_from`

```cpp
std::vector<pending_tx> create_transactions_from(
    const cryptonote::account_public_address& address,
    bool is_subaddress,
    const size_t outputs,
    std::vector<size_t> unused_transfers_indices,
    std::vector<size_t> unused_dust_indices,
    const size_t fake_outs_count,
    fee_priority priority,
    const std::vector<uint8_t>& extra);
```

Low-level transaction creation from pre-selected input sets. Used by `create_transactions_all`, `create_transactions_single`, and `create_unmixable_sweep_transactions`.

**Behavior:**
- Alternates between dust and non-dust inputs to ensure transactions can always pay their own fees.
- Accumulates inputs until the transaction weight target is reached or all inputs are consumed.
- Creates `outputs` destination entries, distributing the total transferred amount (input sum minus fee) equally among them, with residue distributed as 1 atomic unit per output.
- Fee convergence loop: iteratively adjusts fee and destination amounts until `needed_fee <= test_ptx.fee`.
- Performs the two-phase (fake then real) construction like `create_transactions_2`.

### `create_unmixable_sweep_transactions`

```cpp
std::vector<pending_tx> create_unmixable_sweep_transactions();
```

Sweeps all unmixable (pre-RCT, non-standard-denomination) outputs back to the wallet's main address.

**Behavior:**
- Selects unmixable outputs via `select_available_unmixable_outputs()`.
- Splits them into dust (below base fee) and non-dust.
- Delegates to `create_transactions_from()` with `fake_outs_count = 0` (no ring members needed) and `fee_priority::Unimportant`.

### `transfer_selected`

```cpp
template<typename T>
void transfer_selected(
    const std::vector<tx_destination_entry>& dsts,
    const std::vector<size_t>& selected_transfers,
    size_t fake_outputs_count,
    std::vector<std::vector<get_outs_entry>>& outs,
    std::unordered_set<crypto::public_key>& valid_public_keys_cache,
    uint64_t fee,
    const std::vector<uint8_t>& extra,
    T destination_split_strategy,
    const tx_dust_policy& dust_policy,
    cryptonote::transaction& tx,
    pending_tx& ptx,
    bool use_view_tags);
```

Constructs a non-RCT transaction from pre-selected inputs. Legacy path used before RingCT.

**Preconditions:**
- Not available for multisig wallets (throws error).
- All selected transfers must be from the same subaddress account.
- `found_money >= needed_money` (sum of destinations + fee).

**Behavior:**
- Fetches decoy outputs via `get_outs()` if not already provided.
- Builds `tx_source_entry` for each input with ring members.
- Computes change as `found_money - needed_money`.
- Applies `destination_split_strategy` and `dust_policy` to split destinations.
- Calls `construct_tx_and_get_tx_key()` with `rct = false`.
- Populates the `pending_tx` and `tx_construction_data` fields.

### `transfer_selected_rct`

```cpp
void transfer_selected_rct(
    std::vector<tx_destination_entry> dsts,
    const std::vector<size_t>& selected_transfers,
    size_t fake_outputs_count,
    std::vector<std::vector<get_outs_entry>>& outs,
    std::unordered_set<crypto::public_key>& valid_public_keys_cache,
    uint64_t fee,
    const std::vector<uint8_t>& extra,
    cryptonote::transaction& tx,
    pending_tx& ptx,
    const rct::RCTConfig& rct_config,
    bool use_view_tags);
```

Constructs a RingCT transaction from pre-selected inputs. This is the primary construction path for modern Monero transactions.

**Preconditions:**
- All selected transfers must be from the same subaddress account.
- `found_money >= needed_money`.

**Behavior:**
- Handles multisig wallet case: determines available signers, creates multiple signing attempts for different signer combinations using `multisig_tx_builder`.
- Fetches decoy outputs via `get_outs()` if not already provided.
- For zero change with a single destination: generates a dummy random change address to prevent the recipient from determining the real input via ring analysis.
- For non-zero change: sends change to `subaddress{account, 0}`.
- Calls `construct_tx_and_get_tx_key()` with `rct = true` and the specified `rct_config`.
- Tracks source permutation (the core TX constructor may reorder inputs) and applies it to `selected_transfers`.
- For multisig with threshold > 1: creates `C(n-1, n-threshold)` signing attempts, each with a different "ignore set" of excluded signers.

### `commit_tx`

```cpp
void commit_tx(pending_tx& ptx);
void commit_tx(std::vector<pending_tx>& ptx_vector);
```

Submits a transaction to the daemon.

**Behavior:**
- Serializes the transaction to hex and calls `/sendrawtransaction` RPC with `do_sanity_checks = true`.
- Checks for duplicate processing (already in transfers, unconfirmed, or confirmed).
- Records payment ID, stores as unconfirmed transaction.
- Saves tx keys if `store_tx_info()` is true.
- Marks all selected transfers as spent.
- Wipes multisig nonce data from used transfers.

### `sign_tx` (file-based, CLI)

```cpp
bool sign_tx(
    const std::string& unsigned_filename,
    const std::string& signed_filename,
    std::vector<pending_tx>& ptx,
    std::function<bool(const unsigned_tx_set&)> accept_func = NULL,
    bool export_raw = false);
```

Loads an unsigned transaction file, optionally presents to user via callback, signs it, and saves the signed result. Used by CLI wallet for offline signing.

### `sign_tx` (in-memory, GUI)

```cpp
bool sign_tx(unsigned_tx_set& exported_txs, std::vector<pending_tx>& ptx, signed_tx_set& signed_txs);
```

Signs an unsigned transaction set in memory. Core signing logic shared by all `sign_tx` overloads.

**Behavior:**
- Imports outputs from the unsigned transaction set (either legacy `transfers` or compact `new_transfers` format).
- For each transaction: validates sources are non-empty and unlock_time is zero.
- Calls `construct_tx_and_get_tx_key()` to re-sign the transaction with the signing wallet's keys.
- Saves tx keys locally (since the submitting wallet should not see them).
- Computes fee as `sum(source amounts) - sum(destination amounts)`.
- Generates key images for any change outputs back to this wallet.
- Exports all wallet key images in the signed transaction set.

### `sign_tx` (file output)

```cpp
bool sign_tx(unsigned_tx_set& exported_txs, const std::string& signed_filename,
             std::vector<pending_tx>& txs, bool export_raw = false);
```

Signs and saves to file. Optionally exports raw hex for each transaction.

### `save_tx / dump_tx_to_str`

```cpp
bool save_tx(const std::vector<pending_tx>& ptx_vector, const std::string& filename) const;
std::string dump_tx_to_str(const std::vector<pending_tx>& ptx_vector) const;
```

Serializes pending transactions as an `unsigned_tx_set` (with decrypted short payment IDs and exported outputs), then encrypts and saves to file or returns as string.

### `cold_sign_tx`

```cpp
void cold_sign_tx(
    const std::vector<pending_tx>& ptx_vector,
    signed_tx_set& exported_txs,
    std::vector<cryptonote::address_parse_info>& dsts_info,
    std::vector<std::string>& tx_device_aux);
```

Signs transactions on a hardware wallet (cold signing protocol). Requires the device to implement `hw::device_cold` interface.

### `sanity_check`

```cpp
bool sanity_check(
    const std::vector<pending_tx>& ptx_vector,
    const std::vector<tx_destination_entry>& dsts,
    const unique_index_container& subtract_fee_from_outputs = {}) const;
```

Validates that constructed transactions correctly pay all destinations. Uses `get_tx_proof` / `check_tx_proof` to verify each destination receives at least the expected amount. When `subtract_fee_from_outputs` is active, accounts for the fee deduction tolerance (fee / num_subtractable + 1). Verifies change goes back to the sender's wallet.

## Internal Logic

### Input Selection

#### `pick_preferred_rct_inputs`

Location: `wallet2.cpp:10216`

For RCT transactions, attempts to find a minimal set of inputs (1 or 2) that cover the needed amount. This is the first step in `create_transactions_2`.

**Algorithm:**
1. **Single input pass:** Scans all transfers for a single unspent, unfrozen, RCT, unlocked output in the specified subaddress that covers `needed_money`. Returns immediately if found. Respects `m_ignore_outputs_above` and `m_ignore_outputs_below` bounds.
2. **Two input pass:** Scans all pairs `(i, j)` where `i < j`, both are eligible, and their sum covers `needed_money`. Tracks the pair with lowest "relatedness" (see below). Returns immediately if an unrelated pair (relatedness == 0.0) is found; otherwise returns the best pair found.

#### Output Relatedness (`get_output_relatedness`)

Location: `wallet2.cpp:7419`

Heuristic scoring of how related two outputs appear to an observer. Used to select inputs that minimize privacy leakage.

| Condition | Relatedness Score |
|-----------|------------------|
| Same transaction (`m_txid` match) | 1.0 |
| Same block height | 0.9 |
| Adjacent blocks (height difference = 1) | 0.8 |
| Within 10 blocks | 0.2 |
| More than 10 blocks apart | 0.0 |

#### `pop_best_value_from`

Location: `wallet2.cpp:7446`

Selects the next input to use from a set of unused indices. Minimizes relatedness to already-selected transfers.

**Algorithm:**
1. For each candidate, compute max relatedness to any already-selected transfer.
2. Collect all candidates with the lowest max relatedness.
3. If `smallest = true`, pick the smallest amount among candidates. Otherwise, pick randomly.

#### `should_pick_a_second_output`

Location: `wallet2.cpp:10291`

Returns true if the transaction currently has exactly 1 input, RCT is in use, and there are remaining RCT outputs available. This ensures most RCT transactions have 2 inputs, making them look uniform (preventing 1-input transactions from being identifiable).

#### Dust vs Non-Dust Classification

Outputs are classified based on two criteria:
- **Fractional threshold:** `(base_fee * tx_weight_per_ring) / (use_per_byte_fee ? 1 : 1024)`. Outputs below this are ignored entirely if `m_ignore_fractional_outputs` is set.
- **Decomposed amount:** Non-RCT outputs with valid decomposed amounts go to the non-dust list; others go to the dust list. RCT outputs always go to the non-dust list.

#### Second Output Selection Guard

When adding a second output (the "make RCT txes 2/2" case), the code applies two guards:
1. **Minimum output count/value:** Will not consume the output if it is above `DEFAULT_MIN_OUTPUT_VALUE` (2 XMR) and there are fewer than `DEFAULT_MIN_OUTPUT_COUNT` (5) outputs of that size remaining.
2. **Relatedness threshold:** Will not add if relatedness to the first input exceeds `SECOND_OUTPUT_RELATEDNESS_THRESHOLD` (0.0 -- i.e., only completely unrelated outputs are added).

### Fee Estimation and Calculation

#### Fee Algorithm Selection (`get_fee_algorithm`)

Location: `wallet2.cpp:8562`

Determined by current hard fork version:

| Hard Fork | Fee Algorithm |
|-----------|---------------|
| < v3 | `PreHardforkV3` |
| >= v3 | `HardforkV3` |
| >= v5 | `HardforkV5` |
| >= v8 (HF_VERSION_PER_BYTE_FEE) | `HardforkV8` |

#### Fee Multipliers (`get_fee_multiplier`)

Location: `wallet2.cpp:8446`

Each fee algorithm has a lookup table of multipliers indexed by priority:

| Algorithm | Max Priority | Multipliers [Unimportant, Normal, Elevated, Priority] |
|-----------|-------------|-------------------------------------------------------|
| `PreHardforkV3` | Elevated | [1, 2, 3] |
| `HardforkV3` | Elevated | [1, 20, 166] |
| `HardforkV5` | Priority | [1, 4, 20, 166] |
| `HardforkV8` | Priority | [1, 5, 25, 1000] |

Default priority maps to `Normal` for `HardforkV5+`, `Unimportant` otherwise.

#### Base Fee Calculation (`get_base_fee`)

Location: `wallet2.cpp:8502`

- **Pre-dynamic-fee:** Returns `FEE_PER_KB` (2000000000 atomic units = 0.002 XMR/KB).
- **Dynamic fee (pre-2021-scaling):** Queries daemon via `get_dynamic_base_fee_estimate`, then multiplies by `get_fee_multiplier(priority)`.
- **2021-scaling:** Queries daemon for per-priority base fees via `get_dynamic_base_fee_estimate_2021_scaling`, returns the fee for the requested priority directly (no multiplier needed).
- **Fallback:** `FEE_PER_BYTE` (300000 atomic units) if daemon query fails.

#### Fee Quantization Mask (`get_fee_quantization_mask`)

Location: `wallet2.cpp:8549`

For per-byte-fee hard forks, queries the daemon for the quantization mask. Fee is rounded up to the nearest multiple of this mask. Returns 1 (no quantization) for pre-per-byte-fee or if the query fails.

#### `estimate_fee`

Location: `wallet2.cpp:8432`

Static method that estimates the fee for a transaction with given parameters.

- **Per-byte mode:** Estimates transaction weight via `estimate_tx_weight()`, then calls `calculate_fee_from_weight(base_fee, weight, quantization_mask)`.
- **Per-KB mode:** Estimates transaction size via `estimate_tx_size()`, then calls `calculate_fee(base_fee, size)` which rounds up to the nearest KB.

#### `calculate_fee_from_weight`

Location: `wallet2.cpp:305`

```
fee = weight * base_fee
fee = ceil(fee / quantization_mask) * quantization_mask
```

#### `calculate_fee` (per-KB)

Location: `wallet2.cpp:299`

```
kB = ceil(bytes / 1024)
fee = kB * fee_per_kb
```

#### Fee Convergence Loop

In `create_transactions_2` (line 10923), after the initial estimate, the actual transaction is constructed and its real fee computed from the serialized weight. If the needed fee exceeds the current fee, the transaction is reconstructed with the updated fee. This repeats up to 10 times.

In `create_transactions_from` (line 11350), a similar loop runs without a fixed iteration limit, repeating while `needed_fee > test_ptx.fee`.

### Change Output Logic

#### RCT Transactions (`transfer_selected_rct`)

Location: `wallet2.cpp:10015`

- **Non-zero change:** Change is sent to `subaddress{account, 0}` (the account's primary subaddress). The change destination entry is appended to `splitted_dsts`.
- **Zero change, single destination:** A dummy change output is created with amount 0 to a freshly generated random address. This ensures the transaction always has 2 outputs, preventing the recipient from trivially determining which output is theirs.
- **Zero change, multiple destinations:** No dummy output is added (with 2+ destinations, output ambiguity already exists).

#### Non-RCT Transactions (`transfer_selected`)

Location: `wallet2.cpp:9787`

- Change is computed as `found_money - needed_money`.
- If non-zero, sent to `subaddress{account, 0}`.
- Change is passed through the `destination_split_strategy` and `dust_policy` to handle dust splitting.
- If `dust_policy.add_to_fee` is true, dust is absorbed into the fee. Otherwise, dust goes to `addr_for_dust`.

#### `get_num_outputs` Helper

Location: `wallet2.cpp:224`

Determines the output count for fee estimation:
- Starts with `dsts.size()`.
- Adds 1 for change if `found_money != needed_money`.
- Ensures minimum of 2 outputs (adds dummy if needed).

### Ring Member Selection (Decoy Selection)

#### Overview

Location: `wallet2.cpp:9052` (`get_outs`)

The decoy selection process builds rings for each input, selecting `fake_outputs_count` decoy outputs plus the real output.

#### RCT Outputs: Gamma Distribution

For RCT outputs (amount = 0), the `gamma_picker` is used:

1. **Parameters:** Shape = 19.28, Scale = 1/1.61 (as per Miller et al., "An Empirical Analysis of Traceability in the Monero Blockchain").
2. **Time-based selection:** The gamma distribution produces a time offset `x` (in seconds) from the current chain tip. This is converted to an output index using the `average_output_time` (estimated from the last year of blocks).
3. **Spendable age offset:** If `x > DEFAULT_UNLOCK_TIME` (10 blocks * 120s = 1200s), subtract the unlock time so the selection starts at the first unlocked output. If `x <= DEFAULT_UNLOCK_TIME`, select uniformly from `RECENT_SPEND_WINDOW` (15 blocks * 120s = 1800s).
4. **Block-level resolution:** The selected output index is mapped to a block via binary search in `rct_offsets`, then a random output within that block is picked.

#### Pre-RCT Outputs: Triangular Distribution

For outputs with explicit amounts, a triangular distribution is used:
- 50% (`RECENT_OUTPUT_RATIO`) of decoys are drawn from "recent" outputs.
- The triangular distribution is biased toward the most recent outputs.

#### Segregation Fork Handling

Around the segregation fork height, the selection can be split into pre-fork and post-fork proportions to handle key reuse mitigation:
- `m_segregate_pre_fork_outputs`: restricts decoys to pre-fork outputs when the real output is pre-fork.
- `m_key_reuse_mitigation2`: applies additional pre/post fork distribution ratios (33.4% pre-fork, 33.4% post-fork, remainder normal).

#### Ring Reuse

If a key image has a previously stored ring (from the ring database), that ring is reused. This ensures consistency when the same output is spent across different chains (e.g., after a fork). If the existing ring is larger than the current ring size, an error is thrown.

#### Output Request Batching

- The wallet requests `(fake_outputs_count + 1) * 1.5 + 1` outputs per input (plus `CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW - CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE` extra for RCT to account for locked coinbase outputs).
- Outputs are requested in a sorted order to prevent the daemon from learning which output is real based on request ordering. Internally, a `secret_picking_order` preserves the original selection priority.

#### Sanity Check on Decoys

After `get_outs`, `tx_sanity_check` verifies the selected decoys don't appear suspiciously concentrated. If the check fails, the rings are discarded and rebuilt (up to 3 attempts).

#### Blackball Handling

Outputs marked as "blackballed" (known spent) are excluded in a first pass. If insufficient non-blackballed outputs exist, a second pass allows them (since consensus doesn't enforce blackball lists).

### Transaction Weight and Splitting

- **Upper transaction weight limit:** `full_reward_zone / 2 - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE` (for fork >= 8), or `full_reward_zone - reserved` (for earlier forks). `full_reward_zone` is 300000 bytes for v5+.
- **Weight target:** `TX_WEIGHT_TARGET = upper_limit * 2/3`. Transactions are split when the estimated weight reaches this target, providing headroom for fee adjustment.
- **Maximum outputs per transaction:** `BULLETPROOF_MAX_OUTPUTS - 1 = 15` (one slot reserved for change).

### Two-Phase Construction

Both `create_transactions_2` and `create_transactions_from` use a two-phase approach:

1. **Phase 1 (Fake):** The hardware device is set to `TRANSACTION_CREATE_FAKE` mode. Transactions are constructed to determine the correct fee, input/output assignments, and ring members. This phase may involve multiple trial constructions.
2. **Phase 2 (Real):** The hardware device is set to `TRANSACTION_CREATE_REAL` mode. Each transaction is re-constructed with final parameters. This is the phase that produces the actual signatures.

### Fee Subtraction from Outputs

When `subtract_fee_from_outputs` is provided to `create_transactions_2`:
- The fee is not added to `total_needed_money` during initial balance checks.
- After fee determination, `TX::get_adjusted_dsts()` distributes the fee equally across designated outputs, reducing each by `fee / count` (rounded down), with residue of 1 atomic unit distributed round-robin.
- This feature does not support transaction splitting (throws if `dsts.size() > BULLETPROOF_MAX_OUTPUTS - 1`).

## Dependencies

### What This Module Depends On

| Dependency | Usage |
|-----------|-------|
| `cryptonote::construct_tx_and_get_tx_key()` | Core transaction construction and signing (ring signatures, RingCT proofs). |
| `hw::device` | Hardware wallet abstraction for key operations and signing modes (`TRANSACTION_CREATE_FAKE`, `TRANSACTION_CREATE_REAL`). |
| `hw::device_cold` | Cold signing protocol for hardware wallets (Trezor, etc.). |
| `multisig::signing::tx_builder_ringct_t` | Multisig RingCT transaction builder (partial signing, finalization). |
| `node_rpc_proxy` | Cached RPC calls to the daemon for fee estimates, output distribution, and output histogram. |
| Daemon RPC endpoints | `/sendrawtransaction` (submit), `get_output_histogram` (pre-RCT decoy counts), `get_output_distribution` (RCT output distribution and segregation fork data), `get_outs.bin` (output details for ring construction). |
| `ringdb` | Ring database for storing and retrieving previously used rings per key image. |
| `rct::*` | RingCT types and operations (`RCTConfig`, `rctSig`, commitments, range proofs). |

### What Depends On This Module

| Dependent | Usage |
|-----------|-------|
| `simplewallet` / CLI wallet | Calls `create_transactions_2`, `commit_tx`, `sign_tx`, etc. for user-initiated transfers. |
| `wallet_rpc_server` | Exposes transfer methods via JSON-RPC (`transfer`, `sweep_all`, `sweep_single`, `sign_transfer`, `submit_transfer`). |
| `wallet2_api.h` / GUI wallet interface | `PendingTransaction` wraps `pending_tx` vectors; calls `createTransaction`, `sweepAll`, etc. |
| Multisig workflows | `sign_multisig_tx`, `export_multisig`, `import_multisig` interact with transfer construction for collaborative signing. |

## Decoy Selection Algorithm (Gamma Picker)

### Distribution Parameters

The `gamma_picker` class (wallet2.h:90) implements decoy selection using a gamma distribution with the following parameters:

| Parameter | Value | Source |
|-----------|-------|--------|
| Shape (α) | 19.28 | `GAMMA_SHAPE`, derived from Miller et al. "An Empirical Analysis of Traceability in the Monero Blockchain" |
| Scale (β) | 1/1.61 ≈ 0.6211 | `GAMMA_SCALE` |
| Recent spend window | 1800 seconds (15 × 120s) | `RECENT_SPEND_WINDOW = 15 * DIFFICULTY_TARGET_V2` |
| Recent output ratio | 50% | `RECENT_OUTPUT_RATIO = 0.5` |
| Minimum spendable age | 10 blocks (1200 seconds) | `CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE` |

### Selection Algorithm

Source: `wallet2::get_outs()` at wallet2.cpp:9052 and `gamma_picker::pick()`.

1. **Initialization:** The gamma picker receives the cumulative RCT output distribution (`rct_offsets`) from `get_output_distribution` RPC. It computes `average_output_time` as the average seconds between outputs over the most recent year of blocks.

2. **Pick loop (for each decoy needed):**
   a. Draw a random value `x` from gamma(19.28, 1/1.61) representing output age in seconds.
   b. If `x > DEFAULT_UNLOCK_TIME` (10 blocks × 120s = 1200s): subtract the unlock time so selection starts at the first spendable output.
   c. If `x <= DEFAULT_UNLOCK_TIME`: select uniformly from the recent spend window (last 1800 seconds of outputs).
   d. Convert time offset to a block index using binary search in `rct_offsets`.
   e. Pick a random output within the selected block.
   f. If the picked output is the real output, or is not yet spendable (too young), retry from step (a).

3. **Pre-RCT outputs** (non-zero amounts): Use a triangular distribution instead of gamma, with 50% of picks from recent outputs (`RECENT_OUTPUT_RATIO`).

4. **Segregation fork handling:** When `m_segregate_pre_fork_outputs` or `m_key_reuse_mitigation2` is set, the pick distribution is split between pre-fork and post-fork output zones.

5. **Blackball filtering:** Known-spent outputs are excluded in a first pass. If insufficient non-blackballed outputs exist, a second pass allows them.

6. **Ring reuse:** If a key image has a previously stored ring (from the ring database), that ring is reused to maintain consistency across chains.

7. **Output batching:** The wallet requests `(fake_outputs_count + 1) × 1.5 + 1` outputs per input from the daemon, plus extra for locked coinbase outputs.

### Post-Selection Validation

After `get_outs()`, `tx_sanity_check()` verifies decoys are not suspiciously concentrated. If the check fails, rings are rebuilt (up to 3 attempts).

## Iterative Fee Refinement

Source: `wallet2::create_transactions_2()` at wallet2.cpp:10923 and `wallet2::create_transactions_from()` at wallet2.cpp:11350.

### Fee Estimation Pipeline

1. **Initial estimate:** Query daemon via `get_fee_estimate` RPC → obtain base fee per byte and quantization mask.
2. **Estimate fee:** `estimate_fee(n_inputs, mixin, n_outputs, extra_size, base_fee, quantization_mask)` → initial fee value.
3. **Construct trial transaction:** Build the transaction with the estimated fee.
4. **Measure actual weight:** Serialize the trial transaction → compute actual weight.
5. **Recalculate needed fee:** `calculate_fee_from_weight(base_fee, actual_weight, quantization_mask)`.
6. **Convergence check:** If `fee >= needed_fee - needed_fee/50` (within 2% tolerance), accept. Otherwise, reconstruct with the updated fee.
7. **Iteration limit:** Up to 10 attempts in `create_transactions_2`; unlimited in `create_transactions_from` (loops while `needed_fee > test_ptx.fee`).

### Fee Calculation Formula

For per-byte fee (HF v8+):
```
fee = weight × base_fee
fee = ceil(fee / quantization_mask) × quantization_mask
```

For per-KB fee (pre-HF v8):
```
kB = ceil(bytes / 1024)
fee = kB × fee_per_kb
```

## Input Selection Strategy

Source: `wallet2::create_transactions_2()`, `wallet2::pick_preferred_rct_inputs()` at wallet2.cpp:10216.

### Preferred Input Selection

1. **Single input pass:** Scan all unspent, unfrozen, RCT, unlocked outputs in the specified subaddress. If one output covers `needed_money`, use it. Respects `m_ignore_outputs_above` and `m_ignore_outputs_below`.
2. **Two input pass:** Scan all pairs `(i, j)` where `i < j`. Track the pair with lowest "relatedness" score. Return immediately if an unrelated pair (relatedness == 0.0) is found.
3. **Relatedness scoring** (`get_output_relatedness` at wallet2.cpp:7419):

| Condition | Score |
|-----------|-------|
| Same transaction | 1.0 |
| Same block height | 0.9 |
| Adjacent blocks (±1) | 0.8 |
| Within 10 blocks | 0.2 |
| More than 10 blocks apart | 0.0 |

### Fallback Input Selection

If preferred inputs don't cover the amount:
1. Add inputs one at a time using `pop_best_value_from()`, which minimizes relatedness to already-selected inputs.
2. Alternate between non-dust and dust inputs.
3. After selecting one input, `should_pick_a_second_output()` forces a second input for RCT transactions (making 2-input txs the norm, preventing 1-input txs from being fingerprintable).

### Frozen/Locked Output Exclusion

- **Frozen outputs** (`m_frozen == true`): Excluded from all selection.
- **Locked outputs** (`is_transfer_unlocked()` returns false): Excluded — must be past unlock time AND at least `CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE` (10 blocks) old.
- **Dust threshold:** Outputs below `calculate_fee()` for a 1-in/1-out transaction are classified as dust.

## Transaction Splitting

Source: `wallet2::create_transactions_2()` splitting logic.

### Split Trigger

A transaction is split when:
- Estimated weight exceeds `TX_WEIGHT_TARGET = get_upper_transaction_weight_limit() × 2/3`.
- The upper limit is `full_reward_zone / 2 - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE` for HF v8+, or `full_reward_zone - reserved` for earlier forks.
- Maximum outputs per transaction: `BULLETPROOF_MAX_OUTPUTS - 1 = 15` (one slot reserved for change).

### Splitting Behavior

1. When the weight target is reached, the current transaction is finalized.
2. A new transaction begins accumulating remaining destinations and inputs.
3. Each sub-transaction gets its fee calculated independently based on its own weight.
4. The change for each sub-transaction goes to `subaddress{account, 0}`.
5. The `subtract_fee_from_outputs` feature does NOT support splitting (throws if `dsts.size() > BULLETPROOF_MAX_OUTPUTS - 1`).

## Known Issues

The following TODO/FIXME/HACK/XXX comments were found in transfer-related or closely adjacent code:

| Location | Comment |
|----------|---------|
| `src/wallet/wallet2.h:343` | `//TODO: key_image stored twice :(` -- `transfer_details::m_key_image` duplicates data stored elsewhere. |
| `src/wallet/wallet2.h:1945` | `//TODO: auto-calc this value or request from daemon, now use some fixed value` -- `m_upper_transaction_weight_limit` should be dynamically determined. |
| `src/wallet/wallet2.cpp:1913` | `// TODO: handle this sweep case` -- incomplete sweep handling in a code path. |
| `src/wallet/wallet2.cpp:7352` | `// XXX: this needs to be fast, so we'd need to get the starting heights` -- performance concern in output scanning code adjacent to transfer logic. |
