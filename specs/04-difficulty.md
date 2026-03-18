# Difficulty Calculation

## Overview

The difficulty module implements Monero's proof-of-work difficulty adjustment algorithm and hash-versus-difficulty validation. It determines whether a mined block's hash meets the required difficulty target and computes the next block's difficulty based on recent block timestamps and cumulative difficulties. The module provides both 64-bit and 128-bit arithmetic paths: a fast 64-bit path using native or emulated 128-bit multiplication for the common case where difficulty fits in 64 bits, and a full 128-bit path using Boost.Multiprecision for higher difficulty values. The `Blockchain` class in `cryptonote_core` is the primary consumer, calling `next_difficulty()` to set each block's target and `check_hash()` to validate proof-of-work during block verification.

## Key Files

| File | Lines | Description |
|------|------:|-------------|
| `src/cryptonote_basic/difficulty.h` | 63 | Public header declaring `difficulty_type`, hash-check functions, difficulty calculation functions, and a hex formatting utility. |
| `src/cryptonote_basic/difficulty.cpp` | 257 | Implementation of 64-bit and 128-bit hash checking, the v1 difficulty adjustment algorithm, and portable 128-bit multiplication. |
| `src/serialization/difficulty_type.h` | 63 | Serialization/deserialization support for `difficulty_type` (128-bit) values as two varints (high 64 bits, low 64 bits). |
| `src/cryptonote_config.h` | (partial) | Defines `DIFFICULTY_WINDOW` (720), `DIFFICULTY_CUT` (60), `DIFFICULTY_LAG` (15), `DIFFICULTY_TARGET_V1` (60s), `DIFFICULTY_TARGET_V2` (120s). |

## Data Structures

### `difficulty_type`

```cpp
typedef boost::multiprecision::uint128_t difficulty_type;
```

A 128-bit unsigned integer type used throughout the codebase to represent difficulty values. Defined in the `cryptonote` namespace. This type supports the full range of difficulty values that Monero may encounter as network hashrate grows. Serialized as a pair of `uint64_t` varints (high word first, then low word) via `src/serialization/difficulty_type.h`.

### Module-level constants (in `difficulty.cpp`)

| Constant | Type | Value | Purpose |
|----------|------|-------|---------|
| `max64bit` | `difficulty_type` | `2^64 - 1` | Threshold to decide whether to use 64-bit or 128-bit hash check path. |
| `max128bit` | `boost::multiprecision::uint256_t` | `2^128 - 1` | Upper bound for difficulty result overflow detection in `next_difficulty()`. |
| `max256bit` | `boost::multiprecision::uint512_t` | `2^256 - 1` | The target ceiling used in `check_hash_128()`: `hash * difficulty` must be `<= 2^256 - 1`. |

### Configuration constants (from `cryptonote_config.h`)

| Constant | Value | Description |
|----------|------:|-------------|
| `DIFFICULTY_WINDOW` | 720 | Maximum number of recent blocks used for difficulty calculation. |
| `DIFFICULTY_CUT` | 60 | Number of outlier timestamps trimmed from each end after sorting. |
| `DIFFICULTY_LAG` | 15 | Additional blocks fetched beyond the window (total fetched = `DIFFICULTY_WINDOW + DIFFICULTY_LAG = 735`). |
| `DIFFICULTY_BLOCKS_COUNT` | 735 | `DIFFICULTY_WINDOW + DIFFICULTY_LAG`. The number of recent blocks the `Blockchain` class collects before passing them to `next_difficulty()`. |
| `DIFFICULTY_TARGET_V1` | 60 | Target block time in seconds before hard fork v2. |
| `DIFFICULTY_TARGET_V2` | 120 | Target block time in seconds from hard fork v2 onward. |

## Public API

### Hash Validation

#### `bool check_hash(const crypto::hash &hash, difficulty_type difficulty)`

Dispatching function that validates whether a proof-of-work hash meets the given difficulty. If `difficulty` fits in 64 bits (`<= 2^64 - 1`), delegates to `check_hash_64()` for performance; otherwise delegates to `check_hash_128()`.

- **Returns:** `true` if the hash satisfies the difficulty, `false` otherwise.
- **Preconditions:** `hash` must be a valid 256-bit hash. `difficulty` must be > 0 (a zero difficulty would make the check trivially pass).
- **Error behavior:** None (pure computation, no exceptions).

#### `bool check_hash_64(const crypto::hash &hash, uint64_t difficulty)`

Performs the proof-of-work check using 64-bit arithmetic. Computes `hash * difficulty` as a 320-bit product (256-bit hash times 64-bit difficulty) and checks that the result fits within 256 bits (i.e., no overflow into the upper 64 bits).

- **Algorithm detail:** Processes the hash as four little-endian 64-bit words. Checks the highest word first (most likely to cause overflow for a random hash, providing an early-exit fast path). Multiplies each word by `difficulty`, accumulating carries through `cadd()`/`cadc()` helper functions.
- **Returns:** `true` if `hash * difficulty < 2^256`, `false` otherwise.
- **Preconditions:** `difficulty` is a 64-bit unsigned integer.
- **Error behavior:** None.

#### `bool check_hash_128(const crypto::hash &hash, difficulty_type difficulty)`

Performs the proof-of-work check using Boost.Multiprecision 512-bit arithmetic for difficulties that exceed 64 bits. Reconstructs the hash as a `uint512_t` value from its four 64-bit words (little-endian byte order, big-endian word order), multiplies by `difficulty`, and checks the result against `2^256 - 1`.

- **Compile-time behavior:** The `FORCE_FULL_128_BITS` macro is unconditionally defined, which means all four 64-bit words of the hash are always included in the computation. Without this macro, a fast-path optimization would skip the lowest word and reject hashes where the highest word is nonzero.
- **Returns:** `true` if `hash * difficulty <= 2^256 - 1`, `false` otherwise.
- **Preconditions:** `difficulty` is a 128-bit value.
- **Error behavior:** None.

### Difficulty Calculation

#### `difficulty_type next_difficulty(std::vector<uint64_t> timestamps, std::vector<difficulty_type> cumulative_difficulties, size_t target_seconds)`

Computes the next block's difficulty using the "v1" algorithm (simple windowed average with timestamp trimming). This is the 128-bit variant used throughout the main codebase.

- **Parameters:**
  - `timestamps` -- Block timestamps for recent blocks (up to `DIFFICULTY_BLOCKS_COUNT` entries, truncated to `DIFFICULTY_WINDOW` internally).
  - `cumulative_difficulties` -- Corresponding cumulative difficulty values.
  - `target_seconds` -- Desired block interval (60s for v1, 120s for v2).
- **Returns:** The computed difficulty for the next block. Returns `1` if fewer than 2 data points are provided. Returns `0` on overflow (result exceeds 128 bits).
- **Preconditions:** `timestamps.size() == cumulative_difficulties.size()`. Both vectors are passed by value (copied).
- **Error behavior:** Uses `assert()` for internal invariant checks (debug builds only). Returns `0` on overflow.

#### `uint64_t next_difficulty_64(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulative_difficulties, size_t target_seconds)`

Identical algorithm to `next_difficulty()` but operates entirely in 64-bit arithmetic. Provided for use cases where difficulty values are known to fit in 64 bits.

- **Returns:** The computed difficulty. Returns `1` if fewer than 2 data points. Returns `0` on overflow (when `total_work * target_seconds` does not fit in 64 bits).
- **Error behavior:** Same as `next_difficulty()`.

### Utility

#### `std::string hex(difficulty_type v)`

Converts a 128-bit difficulty value to a hexadecimal string prefixed with `"0x"`.

- **Returns:** Hex string (e.g., `"0x0"`, `"0x1a2b3c"`). Lowercase hex digits.
- **Error behavior:** None. Returns `"0x0"` for zero input.

## Internal Logic

### Hash Check Algorithm (64-bit path)

The core proof-of-work validation tests whether `hash * difficulty < 2^256`. The 256-bit hash is treated as four 64-bit little-endian words `w[0..3]` (where `w[0]` is the least significant). The algorithm:

1. **Early rejection:** Multiply the most significant word `w[3]` by `difficulty`. If the high 64 bits of this 128-bit product are nonzero, the overall 320-bit result will overflow 256 bits. Return `false`.
2. **Full multiplication:** Multiply each word `w[0]`, `w[1]`, `w[2]` by `difficulty`, chaining carries through the intermediate results using `cadd()` (carry-add) and `cadc()` (carry-add-with-carry) helpers.
3. **Overflow check:** If any carry propagates beyond the 256th bit, return `false`; otherwise return `true`.

The `mul()` static helper performs 64x64 -> 128-bit multiplication. On x86_64, it delegates to `mul128()` from `int-util.h` (which uses compiler intrinsics or inline assembly). On other architectures, a portable implementation splits each operand into 32-bit halves and performs four 32x32 multiplications with manual carry propagation.

### Hash Check Algorithm (128-bit path)

Uses Boost.Multiprecision `uint512_t` for the multiplication:

1. Reconstruct the hash as a `uint512_t` by iterating over its four 64-bit words in big-endian word order, shifting and OR-ing.
2. Multiply by `difficulty` (128-bit), yielding up to a 640-bit product.
3. Compare against `max256bit` (= `2^256 - 1`). Return `true` if `<=`.

The `FORCE_FULL_128_BITS` macro (always defined via `#define`) ensures all four hash words are processed, avoiding a potential shortcut that could produce incorrect results for certain hash/difficulty combinations.

### Difficulty Adjustment Algorithm (v1 -- Windowed Average with Trimmed Timestamps)

Both `next_difficulty()` and `next_difficulty_64()` implement the same algorithm:

1. **Window truncation:** If more than `DIFFICULTY_WINDOW` (720) entries are provided, truncate both vectors to that length. (The caller typically provides up to `DIFFICULTY_BLOCKS_COUNT` = 735 entries; the extra `DIFFICULTY_LAG` = 15 entries are discarded here.)

2. **Minimum data check:** If 0 or 1 data points remain, return difficulty `1` (bootstrapping case).

3. **Timestamp sorting:** Sort the timestamps in ascending order. This is critical because block timestamps are not guaranteed to be monotonically increasing in the blockchain (miners can set timestamps within a tolerance window). Sorting ensures the time span calculation is meaningful.

4. **Outlier trimming:** Determine a trimmed sub-range `[cut_begin, cut_end)` of the sorted timestamps:
   - If `length <= DIFFICULTY_WINDOW - 2 * DIFFICULTY_CUT` (i.e., `<= 600`), use the full range (no trimming needed because the dataset is small).
   - Otherwise, trim symmetrically: `cut_begin = (length - 600 + 1) / 2`, `cut_end = cut_begin + 600`. This removes the `DIFFICULTY_CUT` (60) most extreme timestamps from each end, eliminating outliers that could skew the time span.

5. **Time span calculation:** `time_span = timestamps[cut_end - 1] - timestamps[cut_begin]`. If zero (all trimmed timestamps are identical), force to `1` to prevent division by zero.

6. **Total work calculation:** `total_work = cumulative_difficulties[cut_end - 1] - cumulative_difficulties[cut_begin]`. This represents the amount of work done over the trimmed window.

7. **Difficulty computation:** `next_difficulty = ceil(total_work * target_seconds / time_span)`. The ceiling is implemented as `(total_work * target_seconds + time_span - 1) / time_span`. This scales the observed hash rate to produce the desired block interval.

8. **Overflow handling:** If the multiplication overflows the result type (64-bit or 128-bit), return `0`. The blockchain layer treats a returned difficulty of `0` as an error ("difficulty overhead").

### Dispatch Logic in `check_hash()`

`check_hash()` provides a unified entry point that selects the optimal code path:

- If `difficulty <= max64bit` (fits in 64 bits): convert to `uint64_t` and call `check_hash_64()`, which uses fast native/portable 128-bit multiplication.
- Otherwise: call `check_hash_128()`, which uses Boost.Multiprecision 512-bit arithmetic (significantly slower but handles the full 128-bit difficulty range).

### Integration with Blockchain

The `Blockchain` class in `src/cryptonote_core/blockchain.cpp` is the primary consumer:

- **`Blockchain::get_difficulty_for_next_block()`** (line 903): Collects up to `DIFFICULTY_BLOCKS_COUNT` (735) recent timestamps and cumulative difficulties from the database, then calls `next_difficulty()`. Includes a caching optimization: if only one new block has been added since the last call, it pushes the new entry and pops the oldest rather than re-reading all 735 entries from the database.

- **`Blockchain::get_next_difficulty_for_alternative_chain()`** (line 1271): Similar but for alternative (fork) chains. Combines blocks from the alt chain with blocks from the main chain to fill the difficulty window, then calls `next_difficulty()`.

- **Block validation** (lines 4003, 2018): Calls `check_hash(proof_of_work, current_diffic)` to verify that a block's PoW hash meets its required difficulty. Failure sets `bvc.m_verifivation_failed = true` and `bvc.m_bad_pow = true`.

- **Mining** (`src/cryptonote_basic/miner.cpp`, lines 484, 589): The miner calls `check_hash()` to test candidate hashes against the current difficulty target.

- **RPC payment** (`src/rpc/rpc_payment.cpp`, line 247): Uses `check_hash()` to validate RPC payment proof-of-work.

## Dependencies

### This module depends on

| Dependency | Purpose |
|------------|---------|
| `crypto/hash.h` | Provides the `crypto::hash` type (256-bit hash). |
| `boost/multiprecision/cpp_int.hpp` | Provides `uint128_t`, `uint256_t`, `uint512_t` for arbitrary-precision arithmetic. |
| `contrib/epee/include/int-util.h` | Provides `mul128()` (hardware-accelerated 64x64->128-bit multiplication on x86_64) and `swap64le()` (byte-order conversion). |
| `cryptonote_config.h` | Provides `DIFFICULTY_WINDOW`, `DIFFICULTY_CUT`, `DIFFICULTY_LAG` constants. |
| `<algorithm>`, `<vector>`, `<cstdint>`, `<cassert>`, `<string>` | Standard library. |

### Depends on this module

| Consumer | Purpose |
|----------|---------|
| `src/cryptonote_core/blockchain.cpp` | Calls `next_difficulty()` for block difficulty calculation and `check_hash()` for PoW validation. |
| `src/cryptonote_basic/miner.cpp` | Calls `check_hash()` to test mined hashes. |
| `src/rpc/rpc_payment.cpp` | Calls `check_hash()` for RPC payment PoW validation. |
| `src/rpc/core_rpc_server.cpp` | Uses `check_hash()` in RPC processing. |
| `src/serialization/difficulty_type.h` | Serialization support for `difficulty_type`. |
| `src/rpc/zmq_pub.h`, `src/rpc/message_data_structs.h`, `src/rpc/core_rpc_server_commands_defs.h` | Use `difficulty_type` in RPC data structures. |
| `src/blockchain_db/blockchain_db.h` | Uses `difficulty_type` in the database abstraction layer. |
| `src/checkpoints/checkpoints.h` | Uses `difficulty_type` for difficulty checkpoint data. |
| `tests/unit_tests/difficulty.cpp`, `tests/difficulty/difficulty.cpp` | Unit and functional tests for difficulty calculation. |

## Configuration

### Compile-time constants (cryptonote_config.h)

| Constant | Value | Description |
|----------|------:|-------------|
| `DIFFICULTY_WINDOW` | 720 | Number of blocks in the difficulty calculation window. |
| `DIFFICULTY_CUT` | 60 | Number of extreme timestamps trimmed from each end after sorting. |
| `DIFFICULTY_LAG` | 15 | Extra blocks fetched beyond the window for the blockchain layer. |
| `DIFFICULTY_TARGET_V1` | 60 | Target block time (seconds) before hard fork v2. |
| `DIFFICULTY_TARGET_V2` | 120 | Target block time (seconds) from hard fork v2 onward. |

### Compile-time macros (difficulty.cpp)

| Macro | Default | Description |
|-------|---------|-------------|
| `FORCE_FULL_128_BITS` | Defined | Forces `check_hash_128()` to process all four 64-bit hash words. When not defined, a fast-path optimization skips the lowest word and uses an early-exit check on the highest word. Currently always defined (unconditional `#define`). |

### Architecture-dependent code paths

The `mul()` helper in `difficulty.cpp` has two implementations selected at compile time:
- **x86_64** (`__x86_64__` defined): Uses `mul128()` from `int-util.h`, which typically compiles to a single `mul` or `imul` instruction.
- **Other architectures**: Uses a portable implementation that decomposes each 64-bit operand into two 32-bit halves and performs four 32-bit multiplications with manual carry propagation.

### MSVC workaround

On MSVC (`_MSC_VER` defined), the `max` macro is explicitly `#undef`-ed (line 166-168) to prevent conflicts with `std::numeric_limits::max()`, which Windows headers may shadow via a `max` macro.

### Runtime configuration

The `Blockchain` class supports a `m_fixed_difficulty` member that, when nonzero, bypasses `next_difficulty()` entirely and returns the fixed value. This is used for testing and regtest modes. It is not part of the difficulty module itself but affects how the module is invoked.

## Known Issues

| Location | Comment |
|----------|---------|
| `src/cryptonote_basic/difficulty.cpp:158` | `// TODO: consider throwing an exception instead` -- In `next_difficulty_64()`, when `total_work * target_seconds` overflows 64 bits, the function returns `0` rather than throwing. The blockchain layer interprets a zero difficulty as a "difficulty overhead" error. The TODO suggests this should be an exception for clearer error handling. |
| `src/cryptonote_basic/difficulty.cpp:238` | `return 0; // to behave like previous implementation, may be better return max128bit?` -- In `next_difficulty()`, when the computed difficulty exceeds 128 bits, the function returns `0` instead of clamping to the maximum 128-bit value. The inline comment questions whether returning `max128bit` would be more appropriate. |
