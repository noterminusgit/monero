# Proof of Work

## Overview

The proof-of-work (PoW) subsystem is responsible for computing and verifying the computationally expensive hash that secures each Monero block. Monero has used three generations of PoW algorithms on mainnet:

1. **CryptoNight (original, V0)** -- hard forks 1 through 6 (blocks 1 to 1,545,999). The original CryptoNote memory-hard hash using a 2 MB scratchpad.
2. **CryptoNight variants (V1, V2, V4/CryptoNight-R)** -- hard forks 7 through 11 (blocks 1,546,000 to 1,978,432). Successive anti-ASIC tweaks layered onto the CryptoNight base.
3. **RandomX** -- hard fork 12 onward (blocks 1,978,433+). A completely new PoW algorithm optimised for general-purpose CPUs, provided by an external library.

The PoW dispatch function `get_block_longhash()` in `cryptonote_format_utils.cpp` selects the correct algorithm and variant based on the block's `major_version` and height.

### Key Files

| File | Description |
|------|-------------|
| `src/crypto/slow-hash.c` | CryptoNight implementation (variants 0--4), AES-NI/NEON/software paths, ~1,887 lines |
| `src/crypto/rx-slow-hash.c` | RandomX wrapper: cache/dataset/VM management, seed hash lifecycle, ~525 lines |
| `src/crypto/hash-ops.h` | C header: `cn_slow_hash()`, `rx_slow_hash()`, `RX_BLOCK_VERSION` constant |
| `src/crypto/hash.h` | C++ wrappers: `cn_slow_hash()`, `cn_slow_hash_prehashed()`, `cn_variant1_check()` |
| `src/crypto/variant2_int_sqrt.h` | V2 integer square root: `integer_square_root_v2()`, SSE2/FP64/reference paths |
| `src/crypto/variant4_random_math.h` | V4/CryptoNight-R: random math program generation and interpreter |
| `src/crypto/CryptonightR_JIT.h` / `.c` | JIT compilation of CryptoNight-R random programs (x86-64 only) |
| `src/cryptonote_basic/cryptonote_format_utils.cpp` | `get_block_longhash()` -- PoW dispatch by block version/height |
| `src/cryptonote_basic/difficulty.cpp` | `check_hash()`, `check_hash_64()`, `check_hash_128()` -- PoW verification |
| `src/hardforks/hardforks.cpp` | Mainnet hard fork table with activation heights |

## CryptoNight Base Algorithm

CryptoNight is a memory-hard hash function designed to be efficient on consumer CPUs while resisting GPU/FPGA/ASIC acceleration. The algorithm operates in five sequential steps over a 2 MB scratchpad.

Source: `src/crypto/slow-hash.c`, lines 47--52, 874--1011.

### Constants

| Constant | Value | Definition |
|----------|-------|------------|
| `MEMORY` | `1 << 21` (2,097,152 bytes = 2 MB) | Scratchpad size |
| `ITER` | `1 << 20` (1,048,576) | Number of memory-hard loop iterations |
| `AES_BLOCK_SIZE` | 16 | Size of one AES block in bytes |
| `AES_KEY_SIZE` | 32 | AES-256 key size in bytes |
| `INIT_SIZE_BLK` | 8 | Number of AES blocks processed per scratchpad fill iteration |
| `INIT_SIZE_BYTE` | 128 | `INIT_SIZE_BLK * AES_BLOCK_SIZE` (8 * 16) |
| `TOTALBLOCKS` | 131,072 | `MEMORY / AES_BLOCK_SIZE` (2^21 / 16 = 2^17) |

Source: `src/crypto/slow-hash.c`, lines 47--52, 413.

### Step 1: Keccak Initialisation

The input data is hashed through Keccak-1600 to produce a 200-byte state (`hash_state`, a union of `uint8_t b[200]` and `uint64_t w[25]`). The first 64 bytes of this state are treated as two AES-256 keys (`k[0..31]` and `k[32..63]`), and the next 128 bytes (`init[0..127]`) become the initial `text` buffer.

```
if (prehashed)
    memcpy(&state.hs, data, length);
else
    hash_process(&state.hs, data, length);
memcpy(text, state.init, INIT_SIZE_BYTE);
```

Source: `src/crypto/slow-hash.c`, lines 903--909.

### Step 2: Scratchpad Initialisation (AES Encryption Fill)

The 128-byte `text` buffer is repeatedly encrypted using 10 rounds of AES (using the first 32 bytes of the Keccak state as the key) and the result is written sequentially to fill the entire 2 MB scratchpad. There are `MEMORY / INIT_SIZE_BYTE = 16,384` iterations, each writing 128 bytes (8 AES blocks).

When hardware AES-NI is available, `aes_pseudo_round()` is used (10 rounds of `_mm_aesenc_si128` without the standard final round or initial `AddRoundKey`). Without AES-NI, the software fallback `aesb_pseudo_round()` is used instead.

Source: `src/crypto/slow-hash.c`, lines 915--939.

### Step 3: Memory-Hard Loop

This is the core of CryptoNight's ASIC resistance. Two 128-bit values `a` and `b` are initialised from the Keccak state:

```
a[0] = k[0..7]   XOR k[32..39]
a[1] = k[8..15]  XOR k[40..47]
b[0] = k[16..23] XOR k[48..55]
b[1] = k[24..31] XOR k[56..63]
```

The loop runs `ITER / 2 = 524,288` iterations. Each iteration performs two dependent read-modify-write operations on pseudo-random scratchpad locations:

1. **Read** a 128-bit value from the scratchpad at address derived from `a` (address = `(a[0] >> 4) & (TOTALBLOCKS - 1) << 4`).
2. **AES encrypt** the read value using `a` as the round key (single AES round via `_mm_aesenc_si128` or `aesb_single_round`).
3. **XOR with `b`** and write the result back to the same scratchpad location.
4. **Read** again from a new address derived from the AES result.
5. **64-bit multiply** the AES result with the second read value (`c[0] * b[0]` producing 128-bit `hi:lo`).
6. **Add** the multiply result into `a` and write `a` to the second scratchpad location.
7. **XOR** the second read value into `a` to prepare for the next iteration.

Source: `src/crypto/slow-hash.c`, lines 941--972 (SSE2 path), 1821--1854 (portable path).

### Step 4: Scratchpad Extraction (AES Decryption)

The 128-byte `text` buffer is reinitialised from `state.init`, then sequentially XORed with each 128-byte chunk of the scratchpad and encrypted using the second AES key (`k[32..63]`). This folds the scratchpad contents back into the state.

Source: `src/crypto/slow-hash.c`, lines 974--999.

### Step 5: Final Hash Selection

The Keccak permutation is applied to the state one more time. The first byte of the result (`state.hs.b[0] & 3`) selects one of four finaliser hash functions:

| `state.hs.b[0] & 3` | Hash Function | Output |
|----------------------|---------------|--------|
| 0 | Blake-256 | 32 bytes |
| 1 | Groestl-256 | 32 bytes |
| 2 | JH-256 | 32 bytes |
| 3 | Skein-256 | 32 bytes |

The selected hash is applied to the full 200-byte Keccak state to produce the final 32-byte PoW hash.

```c
static void (*const extra_hashes[4])(const void *, size_t, char *) = {
    hash_extra_blake, hash_extra_groestl, hash_extra_jh, hash_extra_skein
};
// ...
extra_hashes[state.hs.b[0] & 3](&state, 200, hash);
```

Source: `src/crypto/slow-hash.c`, lines 891--894, 1009--1011.

## CryptoNight Variant Table

Each CryptoNight variant adds mutations to the memory-hard loop (Step 3) to break ASIC implementations developed for the previous variant. All variants share the same base algorithm structure and constants.

### V0: Original CryptoNight (Hard Forks 1--6)

The unmodified CryptoNight algorithm as described above. No variant-specific mutations.

- **Dispatch:** `variant = 0` when `major_version < HF_VERSION_CRYPTONIGHT_VARIANT_1` (i.e., `major_version < 7`).
- **Block range (mainnet):** Block 1 to 1,545,999.

### V1: CryptoNight V1 (Hard Fork 7)

Adds two mutations to the memory-hard loop that depend on the input data, making the hash function non-fungible across different input lengths.

- **Input length requirement:** The input must be at least 43 bytes. If `length < 43`, the function aborts. This is enforced by the `VARIANT1_CHECK()` macro and the C++ wrapper `cn_variant1_check()` in `hash.h`.
- **Tweak byte derivation:** An 8-byte tweak `tweak1_2` is computed as `state.hs.w[24] XOR *(uint64_t*)(input + 35)`. The "nonce pointer" at byte offset 35 into the original input is XORed with bytes 192--199 of the Keccak state.
- **VARIANT1_1 mutation:** Applied after writing the AES result to the scratchpad. Modifies byte 11 of the written value using a table lookup: `table = 0x75310`, `index = (((byte >> 3) & 6) | (byte & 1)) << 1`, then `byte ^= (table >> index) & 0x30`.
- **VARIANT1_2 mutation:** Applied after the multiply-accumulate step. XORs the `tweak1_2` value into the second scratchpad write.
- **Dispatch:** `variant = 1` when `major_version == 7`.
- **Block range (mainnet):** Block 1,546,000 to 1,685,554.

Source: `src/crypto/slow-hash.c`, lines 120--158; `src/crypto/hash.h`, lines 74--79.

### V2: CryptoNight V2 (Hard Forks 8--9)

Adds integer math (division and square root) and a shuffle-add operation to the memory-hard loop, dramatically increasing the computational complexity per iteration.

- **Integer math injection (`VARIANT2_INTEGER_MATH_DIVISION_STEP`):** At each iteration, performs a 64-bit division of a scratchpad value by a modified 32-bit divisor (ORed with `0x80000001` to prevent division by zero and ensure odd divisor). The quotient and remainder are packed into `division_result`. An integer square root of `scratchpad_value + division_result` is also computed. The XOR of `division_result ^ (sqrt_result << 32)` is injected into the loop state.
- **Integer square root (`integer_square_root_v2`):** Computes the integer part of `sqrt(2^64 + n) * 2 - 2^33` using only 64-bit unsigned arithmetic. Three implementation paths exist: SSE2 (using `_mm_sqrt_sd`), FP64 (using `sqrt()` with fixup for platforms with >= 50 mantissa bits), and a pure-integer reference implementation using a convergent bit-by-bit algorithm. All paths apply `VARIANT2_INTEGER_MATH_SQRT_FIXUP` to correct off-by-one errors.
- **Shuffle-add (`VARIANT2_SHUFFLE_ADD`):** Three additional 128-bit scratchpad reads from addresses at offsets `j ^ 0x10`, `j ^ 0x20`, `j ^ 0x30`. The values are shuffled and added with registers `a`, `b`, `b1`, then written back.
- **Additional XOR (`VARIANT2_2`):** After the multiply step, `hi:lo` are XORed into scratchpad locations at `j ^ 0x10` and `j ^ 0x20`.
- **Dispatch:** `variant = 2` when `major_version == 8`; `variant = 3` when `major_version == 9`.
- **Block range (mainnet):** Block 1,685,555 to 1,787,999.

Source: `src/crypto/slow-hash.c`, lines 160--308; `src/crypto/variant2_int_sqrt.h`.

### V4: CryptoNight-R (Hard Forks 10--11)

Adds a per-block random math program to the memory-hard loop, making it impossible to build fixed-function ASICs because the computation changes at every block height.

- **Random program generation (`v4_random_math_init`):** A deterministic random math program is generated from the block height. The height is written into a 32-byte buffer with `data[20] = -38` as a seed modifier, then iteratively hashed with Blake-256 to produce random bytes. These bytes encode 60--70 instructions selected from: `MUL` (3-cycle latency), `ADD` (2-cycle, with 32-bit random constant), `SUB` (1-cycle), `ROR` (2-cycle), `ROL` (2-cycle), `XOR` (1-cycle). The program targets a total latency of `TOTAL_LATENCY = 45` cycles (equivalent to 15 multiplications). The generator ensures ASIC resistance by also targeting `TOTAL_LATENCY` for a theoretical ASIC with unlimited ALUs.
- **Registers:** 4 variable registers (`r[0]`--`r[3]`) initialised from the Keccak state, and 5 constant registers (`r[4]`--`r[8]`) loaded from loop variables `a`, `b`, `b1` at each iteration.
- **Program execution:** At each loop iteration, the random math program is executed on the 9 registers. The results from `r[0]`--`r[3]` are XORed into the loop state `a` and `b`. Two execution paths are available:
  - **Interpreter (`v4_random_math`):** A fully unrolled switch-case interpreter (70 iterations of `V4_EXEC` macro) with 100% branch prediction on CPUs.
  - **JIT (`v4_generate_JIT_code`):** Generates native x86-64 machine code from the instruction sequence. Enabled by default on x86-64 (controllable via `MONERO_USE_CNV4_JIT` environment variable). Returns `-1` if the provided 4096-byte buffer is too small.
- **Additional V4 shuffle XOR:** When `variant >= 4`, the shuffle-add operation also XORs `chunk1 ^ chunk2` and `chunk3` into the output, adding more data dependency.
- **Dispatch:** `variant = 4` when `major_version == 10`; `variant = 5` when `major_version == 11`.
- **Block range (mainnet):** Block 1,788,000 to 1,978,432.

Source: `src/crypto/variant4_random_math.h`; `src/crypto/CryptonightR_JIT.h`; `src/crypto/slow-hash.c`, lines 319--370.

### Variant Settings Summary

| Variant | Name | Hard Fork(s) | Mainnet Block Range | Key Change |
|---------|------|-------------|---------------------|------------|
| 0 | CryptoNight | 1--6 | 1 -- 1,545,999 | Original algorithm |
| 1 | CryptoNight V1 | 7 | 1,546,000 -- 1,685,554 | Tweak byte XOR, table mutation, min 43-byte input |
| 2 | CryptoNight V2 | 8 | 1,685,555 -- 1,686,274 | Integer sqrt/division, shuffle-add |
| 3 | CryptoNight V2 | 9 | 1,686,275 -- 1,787,999 | Same as variant 2 (different major_version) |
| 4 | CryptoNight-R | 10 | 1,788,000 -- 1,788,719 | Per-block random math program, optional JIT |
| 5 | CryptoNight-R | 11 | 1,788,720 -- 1,978,432 | Same as variant 4 (different major_version) |

### Variant Dispatch Formula

The variant number is computed from the block's `major_version` in `get_block_longhash()`:

```cpp
const int pow_variant = major_version >= HF_VERSION_CRYPTONIGHT_VARIANT_1
    ? major_version - (HF_VERSION_CRYPTONIGHT_VARIANT_1 - 1) : 0;
```

Since `HF_VERSION_CRYPTONIGHT_VARIANT_1 = 7`, this simplifies to:

| `major_version` | `pow_variant` |
|-----------------|---------------|
| 1--6 | 0 |
| 7 | 1 |
| 8 | 2 |
| 9 | 3 |
| 10 | 4 |
| 11 | 5 |

Source: `src/cryptonote_basic/cryptonote_format_utils.cpp`, lines 1629--1632; `src/cryptonote_config.h`, line 180.

## RandomX

RandomX is a proof-of-work algorithm designed specifically for general-purpose CPUs. It replaces CryptoNight starting at hard fork 12 (block 1,978,433 on mainnet). RandomX is implemented as an external library (`external/randomx/`); Monero wraps it via `rx-slow-hash.c`.

Source: `src/crypto/hash-ops.h`, line 100: `#define RX_BLOCK_VERSION 12`.

### Activation

RandomX activates when `major_version >= RX_BLOCK_VERSION` (12). On mainnet, this corresponds to block 1,978,433 (approximately November 30, 2019).

Source: `src/hardforks/hardforks.cpp`, lines 68--69; `src/cryptonote_basic/cryptonote_format_utils.cpp`, line 1623.

### Seed Hash

RandomX uses a "seed hash" to initialise its dataset and cache. The seed hash is the block hash of a prior block, computed by `rx_seedheight()`:

```c
#define SEEDHASH_EPOCH_BLOCKS  2048
#define SEEDHASH_EPOCH_LAG     64

uint64_t rx_seedheight(const uint64_t height) {
    const uint64_t seedhash_epoch_lag = get_seedhash_epoch_lag();
    const uint64_t seedhash_epoch_blocks = get_seedhash_epoch_blocks();
    uint64_t s_height = (height <= seedhash_epoch_blocks + seedhash_epoch_lag) ? 0 :
                        (height - seedhash_epoch_lag - 1) & ~(seedhash_epoch_blocks - 1);
    return s_height;
}
```

The seed hash changes every `SEEDHASH_EPOCH_BLOCKS` (2048) blocks with a lag of `SEEDHASH_EPOCH_LAG` (64) blocks. Both values are overridable via environment variables (`SEEDHASH_EPOCH_BLOCKS`, `SEEDHASH_EPOCH_LAG`) but are constrained: epoch blocks must be a power of 2 between 2 and 2048; lag must be a power of 2 at most 64.

The function `rx_seedheights()` also computes `nextheight`, the seed height for the upcoming epoch, allowing the daemon to pre-initialise the RandomX cache before the epoch transition.

Source: `src/crypto/rx-slow-hash.c`, lines 136--190.

### Cache and Dataset Management

RandomX operates in two modes with different performance characteristics:

- **Light mode:** Uses a ~256 MB cache. Verification takes 10--15 ms per hash. Multiple threads can verify in parallel.
- **Full mode:** Uses a ~2 GB dataset (initialised from the cache). Mining takes 1--2 ms per hash. Enabled via the `MONERO_RANDOMX_FULL_MEM` environment variable.

The wrapper maintains global state protected by read-write locks:

| Global | Type | Purpose |
|--------|------|---------|
| `main_cache` | `randomx_cache*` | Primary cache for the current seed hash |
| `main_dataset` | `randomx_dataset*` | Full dataset for mining (optional) |
| `secondary_cache` | `randomx_cache*` | Secondary cache for verifying blocks with a different seed hash |
| `main_seedhash` | `char[32]` | Current primary seed hash |
| `secondary_seedhash` | `char[32]` | Current secondary seed hash |

Thread-local VMs are allocated per thread:

| Thread-local | Type | Purpose |
|-------------|------|---------|
| `main_vm_full` | `randomx_vm*` | Full-dataset VM for the main seed hash |
| `main_vm_light` | `randomx_vm*` | Light VM for the main seed hash |
| `secondary_vm_light` | `randomx_vm*` | Light VM for the secondary seed hash |

Source: `src/crypto/rx-slow-hash.c`, lines 49--73.

### `rx_slow_hash()`

```c
void rx_slow_hash(const char *seedhash, const void *data, size_t length, char *result_hash);
```

The main entry point for RandomX hashing. It selects one of three paths based on seed hash matching:

1. **Fast path** (`seedhash == main_seedhash`): Uses `main_vm_full` (if dataset allocated and lock acquired) or `main_vm_light`. Multiple threads run in parallel.
2. **Slow path** (`seedhash == secondary_seedhash`): Uses `secondary_vm_light` in light mode. Multiple threads run in parallel.
3. **Slowest path** (neither match): Acquires a write lock, reinitialises `secondary_cache` with the new seed hash, then computes in light mode. Only one thread at a time, 200--500 ms per hash.

Source: `src/crypto/rx-slow-hash.c`, lines 408--488.

### Dataset Initialisation

When `rx_set_main_seedhash()` is called with a new seed hash, it spawns a background thread that:

1. Acquires write locks on `main_dataset_lock` and `main_cache_lock`.
2. Allocates and initialises `main_cache` from the seed hash.
3. Releases `main_cache_lock` (allowing light-mode verification to proceed immediately).
4. Initialises `main_dataset` using `num_threads - 2` worker threads (leaving 2 CPU cores free).
5. Releases `main_dataset_lock`.

Source: `src/crypto/rx-slow-hash.c`, lines 301--406.

### Hardware Flags

RandomX auto-detects CPU features via `randomx_get_flags()` and uses them to select optimal code paths (hardware AES, large pages, JIT compilation). Two environment variables control behaviour:

- `MONERO_RANDOMX_UMASK`: Bitmask of `randomx_flags` to forcibly disable.
- `MONERO_RANDOMX_FULL_MEM`: When set, enables full dataset allocation for mining.

The JIT flag includes `RANDOMX_FLAG_SECURE` for non-miner threads to map JIT memory as read-execute (rather than write-execute) for security.

Source: `src/crypto/rx-slow-hash.c`, lines 99--134, 240--262.

## PoW Dispatch

The function `get_block_longhash()` in `cryptonote_format_utils.cpp` is the single entry point for computing a block's PoW hash. It dispatches to the correct algorithm based on block height and major version.

```cpp
crypto::hash get_block_longhash(const blobdata_ref block_hashing_blob,
    const uint64_t height,
    const uint8_t major_version,
    const crypto::hash &seed_hash)
{
    crypto::hash res;

    if (height == 202612) // block 202612 bug workaround
    {
        static const std::string longhash_202612 =
            "84f64766475d51837ac9efbef1926486e58563c95a19fef4aec3254f03000000";
        epee::string_tools::hex_to_pod(longhash_202612, res);
    }
    else if (major_version >= RX_BLOCK_VERSION) // RandomX
    {
        crypto::rx_slow_hash(seed_hash.data, block_hashing_blob.data(),
            block_hashing_blob.size(), res.data);
    }
    else // CryptoNight
    {
        static_assert(HF_VERSION_CRYPTONIGHT_VARIANT_1 >= 1);
        const int pow_variant = major_version >= HF_VERSION_CRYPTONIGHT_VARIANT_1
            ? major_version - (HF_VERSION_CRYPTONIGHT_VARIANT_1 - 1) : 0;
        crypto::cn_slow_hash(block_hashing_blob.data(),
            block_hashing_blob.size(), res, pow_variant, height);
    }

    return res;
}
```

Source: `src/cryptonote_basic/cryptonote_format_utils.cpp`, lines 1611--1636.

### Dispatch Rules

| Condition | Algorithm | Details |
|-----------|-----------|---------|
| `height == 202612` | Hardcoded hash | Returns `84f64766...03000000`. A historical bug workaround for a block that cannot be re-validated by the current code. |
| `major_version >= 12` | RandomX | Calls `rx_slow_hash()` with the provided `seed_hash`. |
| `major_version >= 7` | CryptoNight variant | `variant = major_version - 6`. Calls `cn_slow_hash()`. |
| `major_version < 7` | CryptoNight V0 | `variant = 0`. Calls `cn_slow_hash()`. |

### Block 202612 Bug Workaround

Block 202612 on mainnet has a PoW hash that was accepted by a previous version of the software but cannot be correctly recomputed by current code. Rather than breaking the chain, the hash is hardcoded as a special case. This block falls within the original CryptoNight V0 era (hard fork 1).

Source: `src/cryptonote_basic/cryptonote_format_utils.cpp`, lines 1618--1622.

## PoW Verification

After computing a block's PoW hash, the result is checked against the current difficulty target using `check_hash()` from `difficulty.cpp`.

### `check_hash()`

```cpp
bool check_hash(const crypto::hash &hash, difficulty_type difficulty);
```

Dispatches to `check_hash_64()` when `difficulty <= 2^64 - 1`, otherwise to `check_hash_128()`.

The PoW check verifies that `hash * difficulty <= 2^256 - 1`, which is equivalent to checking that the hash, interpreted as a 256-bit little-endian integer, is below the target value `(2^256 - 1) / difficulty`.

Source: `src/cryptonote_basic/difficulty.cpp`, lines 196--201.

### `check_hash_64()`

Uses 64-bit arithmetic with manual carry propagation. The hash is treated as four little-endian 64-bit words. The highest word (`hash[3]`) is checked first as an early-exit optimisation (most random hashes will fail this check immediately). The function computes `hash * difficulty` as a 320-bit product using `mul()` (128-bit multiply) and `cadd()`/`cadc()` (carry-add) helpers, then checks that no carry overflows the 256-bit boundary.

Source: `src/cryptonote_basic/difficulty.cpp`, lines 105--120.

### `check_hash_128()`

Uses Boost.Multiprecision `uint512_t` arithmetic for difficulties exceeding 64 bits. Reconstructs the hash as a `uint512_t` from its four 64-bit words, multiplies by `difficulty`, and checks `hashVal * difficulty <= max256bit` where `max256bit = 2^256 - 1`.

Source: `src/cryptonote_basic/difficulty.cpp`, lines 177--194.

## Platform-Specific Implementations

CryptoNight's `cn_slow_hash()` has three complete implementations in `slow-hash.c`, selected at compile time:

| Platform | Guard | AES Implementation | Scratchpad Allocation |
|----------|-------|-------------------|----------------------|
| x86-64 (SSE2 + AES-NI) | `!defined NO_AES && (defined(__x86_64__) \|\| ...)` | Hardware `_mm_aesenc_si128` with software fallback via `force_software_aes()` env var | `mmap()` with huge pages, fallback to `malloc()` |
| ARM/AArch64 (NEON + Crypto Extensions) | `!defined NO_AES && (defined(__arm__) \|\| defined(__aarch64__))` | Hardware AES via NEON crypto intrinsics with software fallback | `mmap()` with huge pages (AArch64), stack/heap (32-bit ARM) |
| Portable (software only) | `#else` (fallback) | `oaes_lib` software AES via `aesb_pseudo_round()` / `aesb_single_round()` | Stack-allocated `uint8_t long_state[MEMORY]` or heap via `FORCE_USE_HEAP` |

The x86-64 path checks for AES-NI support at runtime via `cpuid` and can be forced to software AES via the `MONERO_USE_SOFTWARE_AES` environment variable. The CryptoNight-R JIT compiler (`v4_generate_JIT_code`) is only available on x86-64 and can be disabled via `MONERO_USE_CNV4_JIT=0`.

Source: `src/crypto/slow-hash.c`, lines 73--96, 373--403, 524--534.

### Thread-Local State

The scratchpad and JIT function pointer are thread-local to allow concurrent hashing from multiple threads:

```c
THREADV uint8_t *hp_state = NULL;       // 2 MB scratchpad
THREADV int hp_allocated = 0;
THREADV v4_random_math_JIT_func hp_jitfunc = NULL;
THREADV uint8_t *hp_jitfunc_memory = NULL;
THREADV int hp_jitfunc_allocated = 0;
```

The `cn_slow_hash_allocate_state()` and `cn_slow_hash_free_state()` functions manage this per-thread memory. On x86-64, the scratchpad is allocated via `mmap()` with `MAP_HUGETLB` for large page support, falling back to `posix_memalign()` or `malloc()`.

Source: `src/crypto/slow-hash.c`, lines 480--484.

## Hard Fork Activation Heights (Mainnet)

For reference, the complete hard fork table showing which PoW algorithm applies at each version:

| HF Version | Block Height | PoW Algorithm | Variant |
|-----------|-------------|---------------|---------|
| 1 | 1 | CryptoNight | 0 |
| 2 | 1,009,827 | CryptoNight | 0 |
| 3 | 1,141,317 | CryptoNight | 0 |
| 4 | 1,220,516 | CryptoNight | 0 |
| 5 | 1,288,616 | CryptoNight | 0 |
| 6 | 1,400,000 | CryptoNight | 0 |
| 7 | 1,546,000 | CryptoNight V1 | 1 |
| 8 | 1,685,555 | CryptoNight V2 | 2 |
| 9 | 1,686,275 | CryptoNight V2 | 3 |
| 10 | 1,788,000 | CryptoNight-R | 4 |
| 11 | 1,788,720 | CryptoNight-R | 5 |
| 12 | 1,978,433 | RandomX | N/A |
| 13 | 2,210,000 | RandomX | N/A |
| 14 | 2,210,720 | RandomX | N/A |
| 15 | 2,688,888 | RandomX | N/A |
| 16 | 2,689,608 | RandomX | N/A |

Source: `src/hardforks/hardforks.cpp`, lines 34--76.

## Rust Port Notes

### CryptoNight

- **AES dependency:** The primary x86-64 path uses AES-NI intrinsics (`_mm_aesenc_si128`, `_mm_aeskeygenassist_si128`). A Rust port should use the `aes` crate with hardware detection or the `aesni` feature, falling back to software AES. The CryptoNight AES usage is non-standard (10 rounds of `aesenc` without the final round or initial key addition), so standard AES library APIs are not sufficient; raw round functions are needed.
- **Scratchpad allocation:** The 2 MB scratchpad should use `mmap` (via `libc` crate) with huge page support for performance. Thread-local storage can be managed with `thread_local!` or stored in a struct.
- **Variant macros:** The C implementation uses extensive preprocessor macros (`VARIANT1_1`, `VARIANT2_SHUFFLE_ADD`, etc.) that inline variant-specific logic into the main loop. A Rust port should consider using `match` on the variant number or compile-time generics to avoid runtime branching overhead.
- **Integer square root:** The `integer_square_root_v2` reference implementation is pure integer arithmetic and translates directly to Rust. The SSE2/FP64 fast paths can use `std::arch::x86_64` intrinsics.
- **CryptoNight-R JIT:** The x86-64 JIT compiler generates raw machine code. A Rust port can either use an interpreter-only path (the `v4_random_math` function is already a portable interpreter) or implement JIT via `cranelift` or `dynasm-rs`. The interpreter path is simpler and sufficient for verification; JIT is only needed for competitive mining performance.
- **V4 random math program generation:** `v4_random_math_init` is self-contained and uses only integer arithmetic plus Blake-256 hashing. It translates directly to Rust.

### RandomX

- **FFI binding:** RandomX is a separate C/C++ library. The recommended approach is to create a Rust FFI binding crate wrapping the C API (`randomx_alloc_cache`, `randomx_init_cache`, `randomx_create_vm`, `randomx_calculate_hash`, etc.). A pure-Rust reimplementation is possible but would be a very large effort with marginal benefit.
- **Thread-local VMs:** The `rx-slow-hash.c` wrapper uses `__thread` (thread-local storage) for VM pointers. In Rust, use `thread_local!` with `RefCell<Option<...>>` or a dedicated struct.
- **Seed hash management:** The dual-cache (main + secondary) design with read-write locks maps naturally to `RwLock<CacheState>` in Rust.
- **Dataset initialisation:** The multi-threaded dataset init (spawning `num_threads - 2` workers) can use `std::thread::spawn` or `rayon`.

### PoW Dispatch

- **Version-to-variant mapping:** The dispatch logic in `get_block_longhash()` is straightforward arithmetic and can be a simple `match` expression.
- **Block 202612 special case:** Must be preserved as a hardcoded constant in the Rust port.
- **check_hash:** The 64-bit path uses manual carry propagation which maps to Rust's `u64::overflowing_mul` and `u64::overflowing_add`. The 128-bit path can use the `uint` crate or Rust's native `u128` type for intermediate computations.
