# Consensus Rules

## Overview

The Monero consensus rules govern block and transaction validity across the entire network. They are enforced primarily in `Blockchain::handle_block_to_main_chain()` and related validation functions within `blockchain.cpp`. The rules have evolved over 16 hardfork versions, each introducing changes to block version requirements, proof-of-work algorithms, ring signature schemes, fee structures, block weight limits, and transaction format constraints. The hardfork mechanism itself is managed by the `HardFork` class, which uses a rolling-window vote-counting scheme (though all mainnet forks use a threshold of 0, meaning they activate at their scheduled heights unconditionally). The block reward follows a smooth emission curve defined by a right-shift formula, with a perpetual tail emission of 0.6 XMR per block once the main emission is exhausted.

## Key Files

| File | Lines | Description |
|------|------:|-------------|
| `src/cryptonote_core/blockchain.cpp` | 5607 | Core consensus enforcement: block/tx validation, reward calculation, weight limits, fee checks, PoW verification |
| `src/cryptonote_core/blockchain.h` | 1664 | `Blockchain` class declaration with all validation method signatures and member state |
| `src/cryptonote_basic/hardfork.h` | 273 | `HardFork` class: version voting, fork state management, block version checks |
| `src/cryptonote_basic/hardfork.cpp` | 424 | `HardFork` implementation: voting window, fork index tracking, reorganization |
| `src/hardforks/hardforks.h` | 52 | `hardfork_t` struct definition; extern declarations for per-network fork tables |
| `src/hardforks/hardforks.cpp` | 128 | Mainnet, testnet, and stagenet hardfork schedule tables |
| `src/cryptonote_config.h` | 369 | All consensus constants: emission params, block weight zones, fee constants, HF version macros |
| `src/cryptonote_basic/cryptonote_basic_impl.h` | 153 | Helper function declarations: `get_block_reward()`, `get_min_block_weight()`, `get_max_tx_size()` |
| `src/cryptonote_basic/cryptonote_basic_impl.cpp` | 377 | Implementation of block reward formula, address parsing, min block weight by version |
| `src/cryptonote_basic/difficulty.cpp` | 257 | Difficulty calculation algorithm: `next_difficulty()`, `check_hash()` |
| `src/cryptonote_basic/cryptonote_format_utils.cpp` | (partial) | `check_output_types()`: view tag enforcement by hardfork version |

## Data Structures

### `hardfork_t` (src/hardforks/hardforks.h:34)

Represents a single hardfork entry in the schedule.

```
struct hardfork_t {
    uint8_t  version;    // Major block version required from this fork onward
    uint64_t height;     // Block height at which this fork activates
    uint8_t  threshold;  // Voting threshold percentage (0-100); 0 = unconditional activation
    time_t   time;       // Approximate epoch timestamp of the fork (used for state warnings)
};
```

**Invariants:** Entries must be added in strictly increasing order of `version`, `height`, and `time`. Version must be non-zero. Threshold must be 0-100.

### `HardFork` (src/cryptonote_basic/hardfork.h:39)

Manages the hardfork state machine. Core members:

| Field | Type | Description |
|-------|------|-------------|
| `db` | `BlockchainDB&` | Reference to the backing blockchain database |
| `heights` | `vector<hardfork_t>` | Ordered list of registered hardfork entries |
| `versions` | `deque<uint8_t>` | Rolling window of last N blocks' voting versions |
| `last_versions[256]` | `unsigned int[]` | Count of each version in the rolling window |
| `current_fork_index` | `uint32_t` | Index into `heights` of the currently active fork |
| `window_size` | `uint64_t` | Size of the voting window (default: 10080 blocks, ~1 week) |
| `default_threshold_percent` | `uint8_t` | Default voting threshold (default: 80%) |
| `original_version` | `uint8_t` | Block version for pre-fork blocks (always 1) |
| `original_version_till_height` | `uint64_t` | Last height that uses `original_version` (mainnet: 1009826) |

**States** (`HardFork::State` enum):
- `Ready` -- software is up to date
- `UpdateNeeded` -- a fork was scheduled more than ~6 months ago and we have not reached it
- `LikelyForked` -- a fork was scheduled more than ~1 year ago and we have not reached it

### `Blockchain::block_extended_info` (src/cryptonote_core/blockchain.h:108)

Metadata carried with a block during chain operations:

| Field | Type | Description |
|-------|------|-------------|
| `bl` | `block` | The block itself |
| `height` | `uint64_t` | Height in the blockchain |
| `block_cumulative_weight` | `uint64_t` | Cumulative weight of the block |
| `cumulative_difficulty` | `difficulty_type` | Accumulated difficulty through this block |
| `already_generated_coins` | `uint64_t` | Total coins minted through this block |

### Key Blockchain Members for Consensus

| Member | Type | Description |
|--------|------|-------------|
| `m_hardfork` | `HardFork*` | Hardfork state manager |
| `m_current_block_cumul_weight_limit` | `uint64_t` | Current maximum block weight (2x effective median) |
| `m_current_block_cumul_weight_median` | `uint64_t` | Effective median block weight for reward calculations |
| `m_long_term_effective_median_block_weight` | `uint64_t` | Long-term effective median (from HF v10+) |
| `m_long_term_block_weights_window` | `uint64_t` | Window size for long-term median (default: 100000) |

## Hardfork Schedule

### Mainnet (`mainnet_hard_forks[]` in src/hardforks/hardforks.cpp)

All mainnet forks use threshold 0 (unconditional activation at scheduled height).

| Version | Height | Approx. Date | Key Changes |
|--------:|-------:|--------------|-------------|
| 1 | 1 | 2012-07-04 | Genesis; CryptoNight PoW; 60s block target; 20 KB full reward zone |
| 2 | 1009827 | 2016-03-20 | 120s block target; 60 KB full reward zone; dust/compound output ban; minimum mixin 2; partial coinbase allowed |
| 3 | 1141317 | 2016-09-24 | RCT v2 tx outputs must be zero-amount |
| 4 | 1220516 | 2017-01-05 | Dynamic fees (`HF_VERSION_DYNAMIC_FEE`); max tx version 2 |
| 5 | 1288616 | 2017-04-15 | 300 KB full reward zone (`CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5`) |
| 6 | 1400000 | 2017-09-16 | Enforce RCT (`HF_VERSION_ENFORCE_RCT`); minimum mixin 4 |
| 7 | 1546000 | 2018-04-06 | Minimum mixin 6; sorted inputs required; CryptoNight variant 1 |
| 8 | 1685555 | 2018-10-18 | Minimum mixin 10 (exact); per-byte fees; bulletproofs allowed |
| 9 | 1686275 | 2018-10-19 | Borromean range proofs forbidden (bulletproofs only) |
| 10 | 1788000 | 2019-03-09 | Bulletproof v2 (`RCTTypeBulletproof2`) allowed; long-term block weight (`HF_VERSION_LONG_TERM_BLOCK_WEIGHT`); smaller bulletproofs |
| 11 | 1788720 | 2019-03-10 | Bulletproof v1 forbidden (v2 only) |
| 12 | 1978433 | 2019-11-30 | RandomX PoW (from `RX_BLOCK_VERSION`=12); minimum 2 outputs; v2 coinbase required; same mixin enforced; reject sigs in coinbase; enforce min output age (10 blocks); effective short-term median used in penalty |
| 13 | 2210000 | 2020-08-23 | Exact coinbase required; CLSAG allowed; deterministic unlock time |
| 14 | 2210720 | 2020-08-24 | Non-CLSAG (MLSAG) ring signatures forbidden (2 grandfathered txs excepted) |
| 15 | 2688888 | 2022-07-01 | Bulletproofs+ allowed; view tags required (`HF_VERSION_VIEW_TAGS`); 2021 scaling rules; minimum mixin 15 (grace period allows 10) |
| 16 | 2689608 | 2022-07-02 | Bulletproof (non-plus) range proofs forbidden; view tags mandatory (txout_to_tagged_key only) |

**Version 1 range:** `mainnet_hard_fork_version_1_till = 1009826` (blocks 0-1009826 use version 1).

### Testnet

Follows the same version progression at different heights. `testnet_hard_fork_version_1_till = 624633`.

### Stagenet

Follows the same version progression at different heights. No special version 1 cutoff (starts at 0).

## Block Reward Rules

### Emission Curve (src/cryptonote_basic/cryptonote_basic_impl.cpp:83-128)

The base reward formula is:

```
emission_speed_factor = EMISSION_SPEED_FACTOR_PER_MINUTE - (target_minutes - 1)
                      = 20 - (target_minutes - 1)

  For v1 (60s target):  emission_speed_factor = 20
  For v2+ (120s target): emission_speed_factor = 19

base_reward = (MONEY_SUPPLY - already_generated_coins) >> emission_speed_factor
```

Where:
- `MONEY_SUPPLY` = `(uint64_t)(-1)` = 2^64 - 1 = 18,446,744,073,709,551,615 atomic units
- `EMISSION_SPEED_FACTOR_PER_MINUTE` = 20
- `DIFFICULTY_TARGET_V1` = 60 seconds
- `DIFFICULTY_TARGET_V2` = 120 seconds
- 1 XMR = 10^12 atomic units (`COIN`)

### Tail Emission

When `base_reward` drops below the tail emission floor:

```
tail_emission = FINAL_SUBSIDY_PER_MINUTE * target_minutes
              = 300,000,000,000 * 2
              = 600,000,000,000 atomic units per block
              = 0.6 XMR per block
```

This ensures perpetual minimum block reward. The constant `FINAL_SUBSIDY_PER_MINUTE` = 3 * 10^11.

### Overflow Protection (blockchain.cpp:4277-4281)

When cumulative coins exceed `MONEY_SUPPLY`, `already_generated_coins` is capped at `MONEY_SUPPLY` to prevent uint64 overflow. Since `MONEY_SUPPLY - MONEY_SUPPLY = 0`, the base formula yields 0, and the tail emission floor kicks in.

### Block Reward Penalty

If a block's weight exceeds the effective median weight, the reward is penalized quadratically (src/cryptonote_basic/cryptonote_basic_impl.cpp:102-128):

```
if current_block_weight <= median_weight:
    reward = base_reward   (no penalty)

if current_block_weight > 2 * median_weight:
    reward = 0             (block is invalid -- returns false)

otherwise:
    penalty_multiplier = (2 * median_weight - current_block_weight) * current_block_weight
    reward = base_reward * penalty_multiplier / median_weight^2
```

The computation uses 128-bit arithmetic (`mul128`/`div128_64`) to avoid overflow. The median_weight floor is `get_min_block_weight(version)` -- the "full reward zone" for the current hardfork version.

### Miner Transaction Validation (blockchain.cpp:1379-1438)

- **v1 and v13+**: Coinbase must claim exactly `base_reward + fees` (exact coinbase).
- **v2 through v12**: Miners may claim less than `base_reward + fees` (partial reward allowed), which slightly modifies the emission curve by deferring unclaimed coins.
- **v3**: Miner tx outputs must be valid decomposed amounts.
- **v12+**: Miner tx must be version 2; RCT signatures in coinbase must be `RCTTypeNull`.
- **All versions**: `unlock_time` must equal `height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW` (60 blocks).

### Median Weight for Penalty Calculation

- **Before v12**: Penalty median is the simple median of the last 100 blocks' weights.
- **v12+** (`HF_VERSION_EFFECTIVE_SHORT_TERM_MEDIAN_IN_PENALTY`): Penalty uses the effective short-term median `m_current_block_cumul_weight_median`, which accounts for the long-term weight system.

## Block Size/Weight Rules

### Full Reward Zones (src/cryptonote_config.h + cryptonote_basic_impl.cpp:69-76)

| HF Version | Zone (bytes) | Constant |
|-----------:|-------------:|----------|
| < 2 | 20,000 | `CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1` |
| 2-4 | 60,000 | `CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2` |
| >= 5 | 300,000 | `CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5` |

Blocks up to the full reward zone weight receive no penalty.

### Dynamic Block Weight (Pre-v10)

Before HF v10, the block weight limit is determined by the median of the last 100 blocks' weights (`CRYPTONOTE_REWARD_BLOCKS_WINDOW`):
- The effective median is at least the full reward zone.
- The block weight limit = 2 * effective_median.
- Blocks exceeding this limit are invalid.

### Long-Term Block Weight System (v10+, `HF_VERSION_LONG_TERM_BLOCK_WEIGHT`)

Introduced to prevent sustained block size inflation attacks (blockchain.cpp:4410-4494):

**Long-term block weight** for each block:
- **v10-v14**: `long_term_block_weight = min(block_weight, long_term_median * 1.4)`
  - Bounded to range `[0, long_term_effective_median * 1.4]`
- **v15+** (`HF_VERSION_2021_SCALING`): `long_term_block_weight = clamp(block_weight, long_term_median / 1.7, long_term_median * 1.7)`
  - Bounded to range `[long_term_effective_median * 10/17, long_term_effective_median * 17/10]`

**Long-term effective median**: median of long-term block weights over the last 100,000 blocks (`CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE`), floored at 300,000 bytes.

**Effective median block weight** (used for reward penalty):
- **v10-v14**: `effective_median = clamp(short_term_median, full_reward_zone, 50 * long_term_effective_median)`
- **v15+**: `effective_median = clamp(short_term_median, long_term_effective_median, 50 * long_term_effective_median)`

The short-term median is the median of the last 100 blocks' actual weights. The surge factor (`CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR`) is 50.

**Block weight limit**: Always `2 * effective_median`.

### Maximum Transaction Size

`CRYPTONOTE_MAX_TX_SIZE` = 1,000,000 bytes (returned by `get_max_tx_size()`).

### Maximum Transaction Extra Size

`MAX_TX_EXTRA_SIZE` = 1060 bytes. Enforced at the mempool level (tx_pool.cpp:186) for non-block transactions.

## Transaction Validation Rules

### Ring Size / Mixin Rules (blockchain.cpp:3341-3418)

The minimum ring size has increased over hardforks:

| HF Version | Min Mixin (ring size - 1) | Notes |
|-----------:|--------------------------:|-------|
| 2-5 | 2 | Unmixable outputs exempted |
| 6 | 4 | `HF_VERSION_MIN_MIXIN_4` |
| 7 | 6 | `HF_VERSION_MIN_MIXIN_6` |
| 8-9 | 10 (exact) | `HF_VERSION_MIN_MIXIN_10`; ring size must be exactly 11 |
| 10-14 | 10 (max also 10) | Ring size locked to exactly 11 |
| 15 | 15 (exact) | `HF_VERSION_MIN_MIXIN_15`; grace period allows ring size 11 at this version only |
| 16+ | 15 (exact) | Ring size locked to exactly 16 |

**From v12** (`HF_VERSION_SAME_MIXIN`): All inputs within a transaction must have the same ring size.

### Transaction Version Rules (blockchain.cpp:3420-3435)

| HF Version | Min TX Version | Max TX Version |
|-----------:|:--------------:|:--------------:|
| 1 | 1 | 1 |
| 2-3 | 1 | 1 |
| 4+ | 1 (or 2 if all mixable) | 2 |
| 6+ | 2 (if all inputs mixable) | 2 |

From `HF_VERSION_ENFORCE_RCT` (v6), version 2 (RingCT) transactions are mandatory for mixable inputs.

### Input Ordering (blockchain.cpp:3437-3455)

**From v7**: Transaction inputs must be sorted by key image in ascending lexicographic order.

### Output Rules (blockchain.cpp:3047-3196)

| HF Version | Rule |
|-----------:|------|
| >= 2 | v1 tx outputs must be valid decomposed amounts (no dust/compound) |
| >= 3 | v2 tx outputs must have amount = 0 (confidential amounts) |
| < 8 | Bulletproofs forbidden |
| 8 | Bulletproofs allowed |
| 9+ | Borromean range proofs forbidden |
| 10 | `RCTTypeBulletproof2` allowed |
| 11+ | `RCTTypeBulletproof` (v1) forbidden |
| 13 | CLSAG (`RCTTypeCLSAG`) allowed |
| 14+ | MLSAG forbidden (2 grandfathered txs: `c5151944...` and `6f2f117c...`) |
| 15 | Bulletproofs+ allowed; view tags allowed (grace period: `txout_to_key` or `txout_to_tagged_key`, but must be uniform) |
| 16+ | Bulletproofs (non-plus) forbidden; view tags mandatory (`txout_to_tagged_key` only) |

**Minimum outputs** (`HF_VERSION_MIN_2_OUTPUTS`, v12): v2 transactions must have at least 2 outputs.

**Maximum bulletproof outputs**: Both `BULLETPROOF_MAX_OUTPUTS` and `BULLETPROOF_PLUS_MAX_OUTPUTS` = 16.

### Output Age / Spendability (blockchain.cpp:3501-3506)

**From v12** (`HF_VERSION_ENFORCE_MIN_AGE`): The highest block referenced by any input must be at least `CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE` (10) blocks behind the current chain height.

### Unlock Time Rules (blockchain.cpp:3685-3706)

- If `unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER` (500,000,000): interpreted as a block height. Output is spendable when chain height >= `unlock_time`.
- Otherwise: interpreted as a Unix timestamp. Output is spendable when current time >= `unlock_time`.
- **From v13** (`HF_VERSION_DETERMINISTIC_UNLOCK_TIME`): time-based unlock uses `get_adjusted_time()` (median of recent block timestamps) instead of system clock, making unlock deterministic.

### Double-Spend Prevention

Every `txin_to_key` input contains a `key_image`. The blockchain maintains a set of all spent key images. If any input's key image already exists in this set, the transaction is rejected (blockchain.cpp:3472-3477).

### Fee Rules (blockchain.cpp:3550-3604)

The dynamic fee is calculated per byte of transaction weight:

```
fee_per_byte = 0.95 * block_reward * DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT / median^2
```

Where:
- `DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT` = 3000 bytes
- `median` = `min(current_weight_median, long_term_effective_median)`
- The 0.95 factor is implemented as `lo -= lo / 20`
- Minimum `fee_per_byte` is 1

The required fee = `tx_weight * fee_per_byte`, quantized up to `PER_KB_FEE_QUANTIZATION_DECIMALS` (8) decimal places. A 2% buffer is allowed on acceptance (`fee >= needed_fee - needed_fee / 50`).

**2021 Scaling Fee Estimates** (v15+, blockchain.cpp:3607-3680): Four fee tiers (low, normal, medium, high) based on the ArticMine scaling paper, using penalty-free zone estimates with grace blocks.

## Block Timestamp Rules (blockchain.cpp:3807-3856)

1. **Future limit**: Block timestamp must not exceed `time(NULL) + CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT` (2 hours = 7200 seconds).
2. **Median check**: Block timestamp must be >= median of the last `BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW` (60) blocks' timestamps. If fewer than 60 blocks exist, the check is skipped.

## Proof-of-Work

### Difficulty Calculation (difficulty.cpp:203-240)

Uses a window of `DIFFICULTY_WINDOW` (720) blocks + `DIFFICULTY_LAG` (15) = 735 total (`DIFFICULTY_BLOCKS_COUNT`). The algorithm:

1. Take timestamps and cumulative difficulties for the window.
2. Sort timestamps; cut the outer `DIFFICULTY_CUT` (60) values from each side.
3. Compute: `difficulty = total_work * target_seconds / time_span`

The target seconds depend on hardfork version:
- v1: 60 seconds (`DIFFICULTY_TARGET_V1`)
- v2+: 120 seconds (`DIFFICULTY_TARGET_V2`)

### PoW Algorithms by Hardfork

| Version Range | Algorithm |
|--------------:|-----------|
| 1-6 | CryptoNight (original) |
| 7 | CryptoNight variant 1 (`HF_VERSION_CRYPTONIGHT_VARIANT_1`) |
| 8-9 | CryptoNight variant 2 |
| 10-11 | CryptoNight/R |
| 12+ | RandomX (`RX_BLOCK_VERSION` = 12) |

## Dependencies

### What This Module Depends On

| Dependency | Description |
|------------|-------------|
| `BlockchainDB` (`blockchain_db/`) | Persistent storage for blocks, transactions, key images, outputs |
| `HardFork` (`cryptonote_basic/hardfork.h`) | Hardfork state machine; determines current version |
| `tx_memory_pool` (`tx_pool.h`) | Transaction mempool; provides txs for block validation |
| `checkpoints` (`checkpoints/checkpoints.h`) | Hardcoded and DNS-fetched checkpoint hashes |
| `crypto/` | Hash functions, key operations, RandomX bindings |
| `ringct/` | RingCT signature verification (MLSAG, CLSAG, bulletproofs, bulletproofs+) |
| `epee::` | Utility library (rolling median, critical sections, misc utilities) |

### What Depends On This Module

| Dependent | Description |
|-----------|-------------|
| `cryptonote_core` | Core daemon logic; delegates all validation to `Blockchain` |
| `cryptonote_protocol` | Network protocol handler; calls block/tx validation |
| `rpc/` | RPC server; queries blockchain state, difficulty, fee estimates |
| `wallet2` | Wallet; uses fee estimates, output selection, block scanning |
| `simplewallet` / `wallet_rpc_server` | User-facing tools that rely on consensus state queries |

## Hard Fork Detection and Activation

Source: `src/cryptonote_basic/hardfork.cpp`, `src/cryptonote_core/blockchain.cpp`.

### Fork Version Determination

The `HardFork` class determines the expected block version at any height:

- `get_ideal_hard_fork_version(height)`: Returns the expected major version at the given height based on the fork schedule table. Iterates `heights[]` to find the highest fork whose activation height ≤ the given height.
- `get_current_hard_fork_version()`: Returns the version of the most recently added block (tracks `current_fork_index`).

### Block Version Validation

In `HardFork::add()` (called when adding each block):

1. **Version check:** The block's `major_version` must be ≥ the ideal version at that height. Blocks with a version below the expected version are rejected outright — there is no grace period.
2. **Future version tolerance:** Blocks with `major_version` > ideal version are accepted if the version has been registered in the fork schedule (even if the activation height hasn't been reached). This allows the voting mechanism to work.
3. **Voting window:** A rolling window of `window_size` (default: 10080) blocks tracks how many blocks vote for each version via their `major_version` field.
4. **Threshold activation:** A fork activates when the percentage of blocks in the window with that version exceeds the fork's threshold. However, all mainnet forks use threshold=0, meaning they activate unconditionally at their scheduled height.

### Reorganization Handling

`HardFork::reorganize_from_block_height(height)`:
- Pops all block versions from the voting window above the given height.
- Recalculates `current_fork_index` based on the remaining window state.
- Called during blockchain reorgs.

## Difficulty Target Selection at Fork Boundaries

Source: `blockchain.cpp:1332`, Bug #7 in specs/bugs.md.

### Target Selection Rule

The difficulty target (time between blocks) is determined by the block's `major_version`, NOT by a height-based lookup:

```
if block.major_version < 2:
    target = DIFFICULTY_TARGET_V1 (60 seconds)
else:
    target = DIFFICULTY_TARGET_V2 (120 seconds)
```

This is significant because it means the target depends on the block being validated, not on the chain height. The code at `blockchain.cpp:1332` uses `block.major_version` directly.

### FIXME Note

The code comment at blockchain.cpp:1332 notes: "FIXME: This will fail if fork activation heights are subject to voting" — because the difficulty target changes based on block version, not on height-based fork schedule lookup. Since all mainnet forks use threshold=0 (unconditional activation), this is not a practical issue.

## Block Version Validation Rules

Source: `hardfork.cpp::add()`, `hardfork.cpp::check()`.

### Validation Matrix

| Condition | Result |
|-----------|--------|
| `block.major_version < ideal_version(height)` | **Rejected** — block version too old |
| `block.major_version == ideal_version(height)` | **Accepted** — expected version |
| `block.major_version > ideal_version(height)` AND version is in fork schedule | **Accepted** — future version vote |
| `block.major_version > ideal_version(height)` AND version is NOT in fork schedule | **Rejected** — unknown version |

### Key Properties

- **No grace period:** Once a fork height is reached, blocks with the old version are immediately rejected. The transition is a hard cutoff.
- **Original version range:** Blocks from height 0 to `original_version_till_height` (mainnet: 1009826) must have `major_version == 1`.
- **Version monotonicity:** Fork schedule entries must be in strictly increasing order of version and height.

## Known Issues

TODO/FIXME/HACK/XXX comments found in the consensus-related source files:

### blockchain.cpp

- `blockchain.cpp:72` -- TODO: Clean up code, possibly change how outputs are referred to/indexed
- `blockchain.cpp:151` -- TODO: Investigate if relative-to-absolute offset conversion is necessary
- `blockchain.cpp:278` -- FIXME: possibly move init logic into the constructor to avoid null pointer dereference
- `blockchain.cpp:341` -- TODO: add function to create and store genesis block taking testnet into account
- `blockchain.cpp:353` -- TODO: if blockchain load successful, verify blockchain against both hard-coded and runtime-loaded checkpoints
- `blockchain.cpp:488` -- TODO: make sure sync exceptions are not simply ignored higher up
- `blockchain.cpp:640` -- FIXME: HardFork (comment only, context unclear)
- `blockchain.cpp:767` -- TODO: this function was poorly written (get_short_chain_history)
- `blockchain.cpp:1332` -- FIXME: This will fail if fork activation heights are subject to voting
- `blockchain.cpp:1523` -- TODO: This function only needed minor modification to work with BlockchainDB
- `blockchain.cpp:1530` -- FIXME: this codebase references DEBUG_CREATE_BLOCK_TEMPLATE
- `blockchain.cpp:1594` -- TODO: (inline) From block not found error handling
- `blockchain.cpp:1643` -- FIXME: consider moving away from block_extended_info at some point
- `blockchain.cpp:1963` -- FIXME: consider moving away from block_extended_info at some point
- `blockchain.cpp:2033` -- FIXME: (context unclear, in handle_alternative_block)
- `blockchain.cpp:2139` -- FIXME: is it even possible for a checkpoint to show up not on the main chain?
- `blockchain.cpp:2225` -- TODO: This function looks like it won't need to be rewritten
- `blockchain.cpp:2229` -- FIXME: This function appears to want to return false if any transactions are missing
- `blockchain.cpp:2249` -- FIXME: s/rsp.missed_ids/missed_tx_id/?
- `blockchain.cpp:2512` -- TODO: return type should be void, throw on exception
- `blockchain.cpp:2598` -- TODO: return type should be void, throw on exception
- `blockchain.cpp:2785` -- FIXME: change argument to std::vector, low priority
- `blockchain.cpp:3003` -- FIXME: it seems this function is meant to be merely a wrapper
- `blockchain.cpp:3309` -- FIXME: consider moving functionality specific to one input into check_tx_input()
- `blockchain.cpp:3806` -- TODO: revisit, has changed a bit on upstream (check_block_timestamp)
- `blockchain.cpp:3948` -- FIXME: get_difficulty_for_next_block can also assert, look into changing to exceptions
- `blockchain.cpp:3966` -- FIXME: height parameter is not used
- `blockchain.cpp:4109` -- XXX: old code adds miner tx here
- `blockchain.cpp:4121` -- XXX: old code does not check whether tx exists
- `blockchain.cpp:4177` -- TODO: Move tx availability checking to right after PoW check
- `blockchain.cpp:4206` -- FIXME: the storage should not be responsible for validation
- `blockchain.cpp:4233` -- TODO: why is this done? make sure keeping invalid blocks makes sense
- `blockchain.cpp:4310` -- TODO: figure out the best way to deal with this failure
- `blockchain.cpp:4543` -- TODO: Refactor, consider returning a failure height
- `blockchain.cpp:5504` -- FIXME: clear tx_pool because the process might have been interrupted

### Other Files

- `src/cryptonote_basic/account.cpp:274` -- TODO: change this code into base 58
- `src/cryptonote_basic/account.cpp:280` -- TODO: change this code into base 58
- `src/cryptonote_basic/cryptonote_format_utils.cpp:238` -- TODO: validate tx
- `src/cryptonote_basic/difficulty.cpp:158` -- TODO: consider throwing an exception instead (on difficulty overflow)
