# Crypto Primitives

## Overview

The `src/crypto/` module provides the foundational cryptographic primitives for the Monero protocol. It implements Ed25519 elliptic curve operations (key generation, Diffie-Hellman key derivation, Schnorr signatures, ring signatures, key images), multiple hash functions (Keccak-256 as "CryptoNight fast hash", CryptoNight/RandomX slow hashes for proof-of-work, and auxiliary hashes Blake, Groestl, JH, Skein), a Keccak-based CSPRNG, ChaCha stream ciphers for symmetric encryption, Merkle tree hashing for transaction sets, and HMAC-Keccak for message authentication. The module is written as a mix of C and C++, with low-level curve and field arithmetic in C and a C++ wrapper class (`crypto_ops`) that exposes the public API with proper type safety via POD wrapper types.

## Key Files

| File | Lines | Description |
|------|------:|-------------|
| `crypto.h` | 345 | Main C++ header: POD type definitions (`ec_point`, `ec_scalar`, `public_key`, `secret_key`, `key_derivation`, `key_image`, `signature`, `view_tag`), `crypto_ops` class declaration, inline public API wrappers, random utilities |
| `crypto.cpp` | 776 | C++ implementation of `crypto_ops`: key generation, key derivation, Schnorr signatures, tx proofs (v1/v2), ring signatures, key images, view tags, thread-safe RNG wrapper |
| `crypto-ops.h` | 169 | C header for low-level Ed25519 group/field/scalar operations: group element types (`ge_p2`, `ge_p3`, `ge_p1p1`, `ge_cached`, `ge_precomp`), scalar arithmetic, point serialization |
| `crypto-ops.c` | 3,879 | C implementation of Ed25519 field element, group element, and scalar arithmetic (derived from SUPERCOP/ref10) |
| `crypto-ops-data.c` | 879 | Precomputed tables for `ge_scalarmult_base` (base point multiples) and curve constants |
| `hash.h` | 107 | C++ header: `hash` (32-byte) and `hash8` (8-byte) POD types, inline wrappers for `cn_fast_hash`, `cn_slow_hash`, `tree_hash` |
| `hash-ops.h` | 110 | C header: `hash_state` union (200-byte Keccak state), `cn_fast_hash`, `cn_slow_hash`, `tree_hash` / `tree_branch` / `tree_path` declarations, RandomX interface |
| `hash.c` | 57 | C implementation of `cn_fast_hash` (Keccak-256), `hash_permutation`, `hash_process` |
| `random.h` | 36 | C header: `generate_random_bytes_not_thread_safe`, `add_extra_entropy_not_thread_safe` |
| `random.c` | 163 | C implementation of CSPRNG: reads `/dev/urandom` (or `CryptGenRandom` on Windows) as seed, uses Keccak sponge for stream generation |
| `generators.h` | 47 | C++ header: accessors for Ed25519 generator points G and H in various representations |
| `generators.cpp` | 186 | C++ implementation: constexpr definitions of G and H, lazy initialization with `std::call_once`, reproducibility verification |
| `chacha.h` | 96 | C/C++ header: ChaCha8/ChaCha20 cipher declarations, `chacha_key` (mlocked 32-byte), `chacha_iv` (8-byte), `generate_chacha_key` KDF |
| `chacha.c` | 182 | C implementation of ChaCha stream cipher (D.J. Bernstein's chacha-merged.c) |
| `keccak.h` | 40 | C header: Keccak context, `keccak()`, `keccakf()`, `keccak1600()`, incremental API |
| `keccak.c` | 239 | C implementation of Keccak permutation and hashing |
| `generic-ops.h` | 87 | C++ macros: `CRYPTO_MAKE_COMPARABLE`, `CRYPTO_MAKE_HASHABLE`, `CRYPTO_MAKE_COMPARABLE_CONSTANT_TIME` for POD crypto types |
| `tree-hash.c` | 257 | C implementation of Merkle tree hash, branch extraction, and verification for transaction sets |
| `slow-hash.c` | 1,887 | C implementation of CryptoNight proof-of-work hash (variants 0-4) with AES-NI, NEON, and software fallbacks |
| `rx-slow-hash.c` | 524 | C wrapper for RandomX proof-of-work hash (block version >= 12) |
| `hmac-keccak.h` | 59 | C header: HMAC-Keccak (RFC 2104 with Keccak-256 base) |
| `hmac-keccak.c` | 81 | C implementation of HMAC-Keccak |
| `blake256.h` / `.c` | 73 / 356 | BLAKE-256 hash (extra hash function for CryptoNight selection) |
| `blake2b.h` / `.c` | 116 / 563 | BLAKE2b hash |
| `groestl.h` / `.c` | 88 / 367 | Groestl-256 hash (extra hash function for CryptoNight selection) |
| `skein.h` / `.c` | 47 / 2,036 | Skein-512-256 hash (extra hash function for CryptoNight selection) |
| `jh.h` / `.c` | 21 / 376 | JH-256 hash (extra hash function for CryptoNight selection) |
| `oaes_lib.h` / `.c` | 140 / 283 | AES implementation for CryptoNight slow hash |
| `aesb.c` | 186 | AES single-round / pseudo-round operations for CryptoNight |
| `initializer.h` | 63 | Portable `INITIALIZER` / `FINALIZER` macros for static initialization (GCC constructor/destructor, MSVC `.CRT$XCT`) |
| `duration.h` | 71 | `random_poisson_duration` template for Poisson-distributed random time intervals |
| `variant2_int_sqrt.h` | 164 | Integer square root for CryptoNight variant 2 |
| `variant4_random_math.h` | 448 | Random math instruction generation for CryptoNight variant 4 (CryptoNight-R) |
| `CryptonightR_JIT.h` / `.c` | 25 / 123 | JIT compilation of CryptoNight-R random programs (x86_64) |
| `c_threads.h` | 70 | Cross-platform thread primitives (pthreads / Windows) for slow hash |

## Data Structures

### Core Scalar and Point Types

All core types are declared `#pragma pack(push, 1)` as POD classes (via the `POD_CLASS` macro).

**`ec_point`** (32 bytes)
- Raw 32-byte compressed Ed25519 curve point representation.
- Fields: `char data[32]`.
- Base type for `public_key`, `key_derivation`, and `key_image`.

**`ec_scalar`** (32 bytes)
- Raw 32-byte Ed25519 scalar (integer mod l, where l = 2^252 + 27742317777372353535851937790883648493).
- Fields: `char data[32]`.
- Base type for `secret_key`.

**`public_key`** (32 bytes)
- Inherits `ec_point`. Represents a compressed Ed25519 public key.
- `crypto_ops` is a friend class (grants access to private constructors).
- Comparison operators `<` and `>` are defined via `memcmp`.
- Made hashable (non-constant-time equality via `CRYPTO_MAKE_HASHABLE`).

**`public_key_memsafe`** (32 bytes)
- Wraps `public_key` in `epee::mlocked<tools::scrubbed<>>` for secure memory (locked pages, automatic zeroing on destruction).
- Made hashable with constant-time comparison (`CRYPTO_MAKE_HASHABLE_CONSTANT_TIME`).

**`secret_key`** (32 bytes)
- Type alias: `epee::mlocked<tools::scrubbed<ec_scalar>>`.
- Stored in locked memory pages that are scrubbed on destruction.
- Constant-time equality comparison (`CRYPTO_MAKE_HASHABLE_CONSTANT_TIME`).
- Deliberately has no `operator<<` overload to prevent accidental logging. The `secret_key_explicit_print_ref` wrapper struct must be used explicitly to print.

**`key_derivation`** (32 bytes)
- Inherits `ec_point`. Represents a Diffie-Hellman shared secret point (8 * r * A or 8 * a * R).

**`key_image`** (32 bytes)
- Inherits `ec_point`. Represents a key image I = x * Hp(P), used for double-spend detection.
- Comparison operators `<` and `>` are defined via `memcmp`.

**`signature`** (64 bytes)
- Contains two `ec_scalar` fields: `c` (challenge) and `r` (response).
- Used for Schnorr signatures and as elements in ring signatures.

**`view_tag`** (1 byte)
- Contains a single `char data` field.
- 1-byte tag derived from sender-receiver shared secret for fast output scanning (99.6% filtering rate).

**`hash`** (32 bytes)
- POD class with `char data[HASH_SIZE]` where `HASH_SIZE = 32`.
- Used throughout the system for transaction hashes, block hashes, and as intermediate values in cryptographic operations.

**`hash8`** (8 bytes)
- POD class with `char data[8]`. Truncated hash used for payment IDs.

A compile-time `static_assert` verifies all structure sizes: `ec_point` = 32, `ec_scalar` = 32, `public_key` = 32, `public_key_memsafe` = 32, `secret_key` = 32, `key_derivation` = 32, `key_image` = 32, `signature` = 64, `view_tag` = 1.

### Ed25519 Group Element Representations

Defined in `crypto-ops.h` as C structs. Field elements (`fe`) are `int32_t[10]` in radix-2^25.5 representation.

**`ge_p2`** -- Extended coordinates (X:Y:Z), satisfying x = X/Z, y = Y/Z on the curve.
- Fields: `fe X, Y, Z`.

**`ge_p3`** -- Extended coordinates (X:Y:Z:T), satisfying x = X/Z, y = Y/Z, T = XY/Z.
- Fields: `fe X, Y, Z, T`.
- Most general internal representation; used for point addition and verification.

**`ge_p1p1`** -- Completed coordinates (X:Y:Z:T), satisfying x = X/T, y = Y/Z. Intermediate form from additions/doublings.
- Fields: `fe X, Y, Z, T`.

**`ge_precomp`** -- Precomputed point for fixed-base scalar multiplication.
- Fields: `fe yplusx, yminusx, xy2d`.
- Used in `ge_scalarmult_base` lookup tables.

**`ge_cached`** -- Cached representation for variable-base addition.
- Fields: `fe YplusX, YminusX, Z, T2d`.
- Produced by `ge_p3_to_cached`, consumed by `ge_add` and `ge_sub`.

**`ge_dsmp`** -- Double-scalar multiplication precomputed table: `ge_cached[8]`.

### Internal Signature Structures (crypto.cpp)

**`s_comm`** -- Challenge input for standard Schnorr signatures: `{ hash h; ec_point key; ec_point comm; }`.

**`s_comm_2_v1`** -- Challenge input for v1 tx proofs: `{ hash msg; ec_point D, X, Y; }`.

**`s_comm_2`** -- Challenge input for v2 tx proofs: `{ hash msg; ec_point D, X, Y; hash sep; ec_point R, A, B; }`. The `sep` field is the domain separator `cn_fast_hash("SubAddr")`.

**`ec_point_pair`** / **`rs_comm`** -- Ring signature challenge: `{ hash h; ec_point_pair ab[]; }` where each `ab` pair holds (a, b) commitments for each ring member.

### Keccak State

**`hash_state`** (200 bytes) -- Union of `uint8_t b[200]` and `uint64_t w[25]`, representing the 1600-bit Keccak permutation state.

**`KECCAK_CTX`** -- Incremental hashing context: `uint64_t hash[25]` (state), `uint64_t message[17]` (136-byte block buffer for Keccak-256 rate), `size_t rest` (buffered byte count).

### HMAC-Keccak State

**`hmac_keccak_state`** -- Contains two `KECCAK_CTX` instances: `inner` and `outer`, implementing HMAC per RFC 2104 with Keccak-256 as the base hash (block size B = 136 bytes).

### ChaCha Types

**`chacha_key`** -- `epee::mlocked<tools::scrubbed_arr<uint8_t, 32>>`. 32-byte key stored in locked, auto-scrubbed memory.

**`chacha_iv`** -- Struct with `uint8_t data[8]`. 8-byte initialization vector (nonce).

### Generator Points

**`G`** -- Standard Ed25519 base point, `{x, 4/5}` (compressed: `0x5866666666...66`). Defined as a `constexpr public_key`.

**`H`** -- Pedersen commitment generator, computed as `8 * toPoint(keccak(G))` (compressed: `0x8b6559701537...`). Defined as a `constexpr public_key`. Used in RingCT for amount commitments.

## Public API

### Key Generation

**`secret_key generate_keys(public_key &pub, secret_key &sec, const secret_key &recovery_key = secret_key(), bool recover = false)`**
- Generates a new Ed25519 key pair. If `recover` is true, uses `recovery_key` as the secret key (for wallet recovery) instead of generating a random scalar.
- Computes `pub = sec * G` via `ge_scalarmult_base`.
- Returns the raw random scalar used (before reduction), enabling deterministic recovery.
- The secret key is reduced mod l via `sc_reduce32`.

**`bool secret_key_to_public_key(const secret_key &sec, public_key &pub)`**
- Computes `pub = sec * G`.
- Precondition: `sec` must pass `sc_check` (valid scalar mod l).
- Returns `false` if `sec` fails `sc_check`.

**`bool check_key(const public_key &key)`**
- Validates that `key` represents a valid point on the Ed25519 curve by attempting `ge_frombytes_vartime`.
- Returns `true` if decompression succeeds, `false` otherwise.
- Note: uses variable-time decompression (acceptable for public keys).

### Key Derivation (Stealth Addresses)

The key derivation protocol enables Monero's stealth address system:
1. Sender generates a transaction key pair (r, R = r*G).
2. Both parties compute the shared secret derivation D = 8*r*A (sender) or D = 8*a*R (receiver), where A/a is the receiver's view key pair.
3. Derived keys are computed from D and the output index.

**`bool generate_key_derivation(const public_key &key1, const secret_key &key2, key_derivation &derivation)`**
- Computes `derivation = 8 * key2 * key1` (ECDH with cofactor clearing).
- The cofactor multiplication by 8 (`ge_mul8`) ensures the result is in the prime-order subgroup.
- Returns `false` if `key1` is not a valid curve point.
- Precondition (asserted): `key2` passes `sc_check`.

**`void derivation_to_scalar(const key_derivation &derivation, size_t output_index, ec_scalar &res)`**
- Computes `res = Hs(derivation || varint(output_index))` where `Hs` is hash-to-scalar.
- The output index is encoded as a varint (variable-length integer) to handle any output position.

**`bool derive_public_key(const key_derivation &derivation, size_t output_index, const public_key &base, public_key &derived_key)`**
- Computes `derived_key = Hs(derivation || output_index) * G + base`.
- Used by sender to compute the one-time public key for an output.
- Returns `false` if `base` is not a valid curve point.

**`void derive_secret_key(const key_derivation &derivation, size_t output_index, const secret_key &base, secret_key &derived_key)`**
- Computes `derived_key = Hs(derivation || output_index) + base` (scalar addition).
- Used by receiver to compute the one-time private key for spending.
- Precondition (asserted): `base` passes `sc_check`.

**`bool derive_subaddress_public_key(const public_key &out_key, const key_derivation &derivation, size_t output_index, public_key &result)`**
- Computes `result = out_key - Hs(derivation || output_index) * G`.
- Inverse of `derive_public_key`; used to check if an output belongs to a subaddress.
- Returns `false` if `out_key` is not a valid curve point.

### Schnorr Signatures

**`void generate_signature(const hash &prefix_hash, const public_key &pub, const secret_key &sec, signature &sig)`**
- Generates a standard Schnorr signature proving knowledge of `sec` such that `pub = sec * G`.
- Algorithm: pick random k, compute `R = k*G`, `c = Hs(prefix_hash || pub || R)`, `r = k - c*sec`.
- Retries if either `c` or `r` is zero.
- Wipes nonce `k` from memory after use.
- Debug mode asserts that `pub == sec * G`.

**`bool check_signature(const hash &prefix_hash, const public_key &pub, const signature &sig)`**
- Verifies: recompute `R' = sig.c * pub + sig.r * G`, then check `Hs(prefix_hash || pub || R') == sig.c`.
- Returns `false` if: `pub` is invalid, `sig.c` or `sig.r` fail `sc_check`, `sig.c` is zero, or the recomputed point equals the identity point (infinity check).
- Uses `ge_double_scalarmult_base_vartime` for efficient verification.

### Transaction Proofs

**`void generate_tx_proof(const hash &prefix_hash, const public_key &R, const public_key &A, const boost::optional<public_key> &B, const public_key &D, const secret_key &r, signature &sig)`**
- Generates a v2 Schnorr proof of knowledge of `r` such that `R = r*G` (or `R = r*B` for subaddresses) and `D = r*A`.
- V2 challenge includes domain separation: `c = Hs(msg || D || X || Y || sep || R || A || B)` where `sep = cn_fast_hash("SubAddr")`.
- If `B` is present (subaddress): `X = k*B`, otherwise `X = k*G`. Always `Y = k*A`.
- Throws `std::runtime_error` if any input point is invalid.

**`void generate_tx_proof_v1(const hash &prefix_hash, const public_key &R, const public_key &A, const boost::optional<public_key> &B, const public_key &D, const secret_key &r, signature &sig)`**
- Generates a v1 Schnorr proof (same as v2 but without domain separation).
- V1 challenge: `c = Hs(msg || D || X || Y)`.
- Marked as "for TESTING ONLY" in the source.

**`bool check_tx_proof(const hash &prefix_hash, const public_key &R, const public_key &A, const boost::optional<public_key> &B, const public_key &D, const signature &sig, const int version)`**
- Verifies a tx proof for version 1 or 2.
- Recomputes `X = sig.c*R + sig.r*G` (or `sig.c*R + sig.r*B` for subaddresses) and `Y = sig.c*D + sig.r*A`.
- For version 1, hashes the smaller v1 buffer; for version 2, hashes the full buffer with domain separator.
- Returns `false` for any invalid input point, invalid scalar, or if `version` is not 1 or 2.

### Key Images and Ring Signatures

**`void generate_key_image(const public_key &pub, const secret_key &sec, key_image &image)`**
- Computes `image = sec * Hp(pub)` where `Hp` is hash-to-point (hash the public key, interpret as curve point, multiply by cofactor 8).
- Key images are linkable across transactions to detect double-spending.
- Precondition (asserted): `sec` passes `sc_check`.

**`void generate_ring_signature(const hash &prefix_hash, const key_image &image, const public_key *const *pubs, size_t pubs_count, const secret_key &sec, size_t sec_index, signature *sig)`**
- Generates a ring signature proving the signer knows one of the secret keys corresponding to `pubs[0..pubs_count-1]`, linked to `image`.
- Implements the original CryptoNote ring signature scheme (back's linkable ring signature).
- The signer's position is `sec_index`; for all other positions, random `c` and `r` values are chosen.
- The challenge is computed as `Hs(prefix_hash || {a_i, b_i})` where `a_i = c_i*P_i + r_i*G` and `b_i = r_i*Hp(P_i) + c_i*I`.
- Allocates the commitment buffer dynamically (`malloc`); calls `local_abort` on allocation failure.
- An overload accepting `std::vector<const public_key *>` is provided for convenience.

**`bool check_ring_signature(const hash &prefix_hash, const key_image &image, const public_key *const *pubs, size_t pubs_count, const signature *sig)`**
- Verifies a ring signature: recomputes all `(a_i, b_i)` pairs, checks that `sum(c_i) == Hs(prefix_hash || {a_i, b_i})`.
- Returns `false` if: `image` is not a valid point, any `pubs[i]` is invalid, any `sig[i].c` or `sig[i].r` fails `sc_check`, or the ring equation does not close.
- An overload accepting `std::vector<const public_key *>` is provided.

### View Tags

**`void derive_view_tag(const key_derivation &derivation, size_t output_index, view_tag &vt)`**
- Computes a 1-byte view tag: `vt = cn_fast_hash("view_tag" || derivation || varint(output_index))[0]`.
- Used during wallet scanning to quickly reject outputs not addressed to the wallet (expected 99.6% rejection rate = 1 - 1/256).
- The domain separator `"view_tag"` (8 bytes, no null terminator) prevents collision with other hash uses.

### Hashing Functions

**`void cn_fast_hash(const void *data, size_t length, char *hash)` / `hash cn_fast_hash(const void *data, size_t length)`**
- Keccak-256 hash (NOT SHA3-256; uses the original Keccak padding).
- Implementation: calls `keccak1600` via `hash_process`, copies first 32 bytes of the 200-byte state.
- HASH_DATA_AREA = 136 bytes (rate for Keccak-256).

**`void cn_slow_hash(const void *data, size_t length, char *hash, int variant, int prehashed, uint64_t height)`**
- CryptoNight proof-of-work hash. Uses a 2 MB scratchpad (MEMORY = 2^21), 2^20 iterations.
- `variant` selects the CryptoNight variant (0 = original, 1-4 = subsequent hard-fork variants).
- `prehashed`: if 1, input is treated as a pre-hashed Keccak state.
- `height`: block height, used for CryptoNight-R (variant 4) random program generation.
- Has hardware-optimized paths for x86_64 (AES-NI) and AArch64 (NEON), with software fallback.

**`void rx_slow_hash(const char *seedhash, const void *data, size_t length, char *result_hash)`**
- RandomX proof-of-work hash for blocks at version >= 12 (RX_BLOCK_VERSION).
- Requires prior initialization via `rx_set_main_seedhash`.

**`void hash_to_scalar(const void *data, size_t length, ec_scalar &res)`**
- Computes `res = cn_fast_hash(data) mod l` (Keccak-256 followed by `sc_reduce32`).
- Used throughout for deriving scalars from arbitrary data.

**`void tree_hash(const char (*hashes)[HASH_SIZE], size_t count, char *root_hash)`**
- Merkle tree hash over a set of 32-byte hashes.
- count = 1: identity. count = 2: `cn_fast_hash(h1 || h2)`. count >= 3: binary tree using `tree_hash_cnt` to find the largest power of 2 less than count.
- Historical note: contained a bug affecting block 202612 (514 transactions); now fixed.

**`bool tree_branch(...)` / `bool tree_branch_hash(...)` / `bool is_branch_in_tree(...)`**
- Extract Merkle branch proofs, reconstruct roots from branches, and verify membership.

**`void hash_extra_blake(...)` / `void hash_extra_groestl(...)` / `void hash_extra_jh(...)` / `void hash_extra_skein(...)`**
- Auxiliary 256-bit hash functions used in the CryptoNight slow hash's hash selection round. One of these four is selected based on bits from the Keccak state to hash the final result.

**`void hmac_keccak_hash(uint8_t *out, const uint8_t *key, size_t keylen, const uint8_t *in, size_t inlen)`**
- One-shot HMAC-Keccak-256 (RFC 2104 construction). Block size B = 136.
- Incremental API: `hmac_keccak_init`, `hmac_keccak_update`, `hmac_keccak_finish`.

### Random Number Generation

**`void generate_random_bytes_not_thread_safe(size_t n, void *result)`**
- Core RNG: produces `n` bytes from a Keccak sponge state.
- Seeded at program start from `/dev/urandom` (Linux) or `CryptGenRandom` (Windows) with 32 bytes.
- Each call: permutes the state (`keccakf`), copies up to HASH_DATA_AREA (136) bytes per permutation.
- Debug mode uses a `curstate` flag to detect concurrent access (not truly thread-safe).

**`void add_extra_entropy_not_thread_safe(const void *ptr, size_t bytes)`**
- XORs additional entropy into the Keccak sponge state (after permutation).
- Processes up to HASH_DATA_AREA bytes per permutation round.

**`void generate_random_bytes_thread_safe(size_t N, uint8_t *bytes)`**
- Thread-safe wrapper: acquires a `boost::mutex` before calling `generate_random_bytes_not_thread_safe`.

**`void add_extra_entropy_thread_safe(const void *ptr, size_t bytes)`**
- Thread-safe wrapper for `add_extra_entropy_not_thread_safe`.

**`void rand(size_t N, uint8_t *bytes)`**
- Inline shortcut for `generate_random_bytes_thread_safe`.

**`template<typename T> T rand()`**
- Generates a random value of any standard-layout, trivially-copyable type.

**`void random32_unbiased(unsigned char *bytes)`**
- Generates a uniformly distributed random scalar in `[1, l)` (non-zero, reduced mod l).
- Rejection sampling: generates 32 random bytes, rejects if >= 15*l (the highest multiple of l that fits in 32 bytes), then reduces via `sc_reduce32` and retries if zero.

**`crypto::random_device`**
- Satisfies the C++ UniformRandomBitGenerator concept, returning `crypto::rand<uint64_t>()`.
- Used with standard distributions (e.g., `std::uniform_int_distribution`, `std::poisson_distribution`).

**`template<typename T> T rand_range(T range_min, T range_max)`**
- Returns a uniformly distributed random integer in `[range_min, range_max]`.

**`template<typename T> T rand_idx(T sz)`**
- Returns a uniformly distributed random index in `[0, sz-1]`.

### Symmetric Encryption (ChaCha)

**`void chacha8(const void *data, size_t length, const uint8_t *key, const uint8_t *iv, char *cipher)`**
- ChaCha stream cipher with 8 rounds. XORs data with the keystream (encryption = decryption).
- 32-byte key, 8-byte IV, 64-byte counter (starts at 0).

**`void chacha20(const void *data, size_t length, const uint8_t *key, const uint8_t *iv, char *cipher)`**
- ChaCha stream cipher with 20 rounds.

**`void generate_chacha_key(const void *data, size_t size, chacha_key &key, uint64_t kdf_rounds)`**
- Key derivation function: applies `cn_slow_hash` (CryptoNight) `kdf_rounds` times to derive a 32-byte ChaCha key.
- The intermediate hash is stored in mlocked, scrubbed memory.
- Used for wallet file encryption.

**`void generate_chacha_key_prehashed(const void *data, size_t size, chacha_key &key, uint64_t kdf_rounds)`**
- Same as above but the first round uses `prehashed = 1` mode.

### Generator Point Accessors

**`public_key get_G()` / `public_key get_H()`**
- Return the generator constants G and H as `public_key` values. G is returned directly (constexpr). H is returned directly (constexpr).

**`ge_p3 get_G_p3()` / `ge_p3 get_H_p3()`**
- Return deserialized `ge_p3` representations. Lazy-initialized via `std::call_once`.

**`ge_cached get_G_cached()` / `ge_cached get_H_cached()`**
- Return `ge_cached` representations (for efficient point addition). Lazy-initialized.

### Type Conversion Utilities

**`unsigned char* to_bytes(crypto::ec_scalar &scalar)` / `const unsigned char* to_bytes(const crypto::ec_scalar &scalar)`**
- Reinterpret-cast a scalar to a byte pointer for interfacing with C functions like `sc_add`.

**`unsigned char* to_bytes(crypto::ec_point &point)` / `const unsigned char* to_bytes(const crypto::ec_point &point)`**
- Reinterpret-cast a point to a byte pointer.

### Output Streaming

- `operator<<` is defined for `public_key`, `key_derivation`, `key_image`, `signature`, `view_tag`, `hash`, and `hash8`, formatting as hex via `epee::to_hex::formatted`.
- `operator<<` is deliberately **not** defined for `secret_key` or `ec_scalar` to prevent accidental logging of secret material. The `secret_key_explicit_print_ref` wrapper must be used explicitly.

### Poisson Duration Generator

**`template<typename D> struct random_poisson_duration`**
- Generates Poisson-distributed random durations using `crypto::random_device`.
- Type aliases: `random_poisson_seconds` (1-second precision), `random_poisson_subseconds` (1/4-second precision).

## Internal Logic

### Ed25519 Curve Operations (crypto-ops.c)

The implementation is derived from the SUPERCOP/ref10 Ed25519 code. The curve is the twisted Edwards curve `-x^2 + y^2 = 1 + d*x^2*y^2` over GF(2^255 - 19), with the base point G having y-coordinate 4/5.

**Field arithmetic** uses a radix-2^25.5 representation (10 limbs of `int32_t`), allowing multiplication without overflow in 64-bit intermediates. Key operations:
- `fe_add`, `fe_sub`, `fe_mul`, `fe_sq`: basic field element arithmetic.
- `fe_invert`: computes multiplicative inverse via exponentiation (Fermat's little theorem, p-2).
- `fe_tobytes` / `fe_frombytes`: serialization (canonical reduction mod 2^255-19).
- `fe_pow22523`: computes `z^((2^252-3))` used in square root calculations.

**Group operations** convert between the five point representations:
- `ge_p3_to_p2`, `ge_p1p1_to_p2`, `ge_p1p1_to_p3`, `ge_p3_to_cached`: representation conversions.
- `ge_add(p1p1, p3, cached)` / `ge_sub(p1p1, p3, cached)`: point addition/subtraction.
- `ge_p2_dbl(p1p1, p2)` / `ge_p3_dbl(p1p1, p3)`: point doubling.
- `ge_madd` / `ge_msub`: mixed addition with `ge_precomp`.

**Scalar multiplication:**
- `ge_scalarmult_base(p3, scalar)`: fixed-base multiplication using precomputed tables (`ge_base[32][8]`). Converts scalar to signed radix-16 digits, performs 64 doublings with 32 additions.
- `ge_scalarmult(p2, scalar, point)`: variable-base scalar multiplication using a sliding window method.
- `ge_scalarmult_p3`: same as `ge_scalarmult` but returns `ge_p3`.
- `ge_double_scalarmult_base_vartime(p2, a, A, b)`: computes `a*A + b*G` using Straus/Shamir interleaving.
- `ge_double_scalarmult_precomp_vartime(p2, a, A, b, Bi)`: computes `a*A + b*B` with precomputed `Bi` table.
- `ge_triple_scalarmult_base_vartime`, `ge_triple_scalarmult_precomp_vartime`: three-scalar variants.

**Scalar arithmetic** (all scalars mod l):
- `sc_reduce(s)`: reduces a 64-byte scalar mod l to 32 bytes.
- `sc_reduce32(s)`: reduces a 32-byte scalar mod l in place.
- `sc_add(s, a, b)`: `s = a + b mod l`.
- `sc_sub(s, a, b)`: `s = a - b mod l`.
- `sc_mul(s, a, b)`: `s = a * b mod l`.
- `sc_muladd(s, a, b, c)`: `s = a*b + c mod l`.
- `sc_mulsub(s, a, b, c)`: `s = c - a*b mod l`.
- `sc_check(s)`: returns 0 if `s` is a canonical scalar (in `[0, l)`), non-zero otherwise.
- `sc_isnonzero(s)`: returns 1 if `s != 0`, 0 otherwise. Not constant-time.
- `sc_0(s)`: sets `s = 0`.

**Special operations:**
- `ge_mul8(p1p1, p2)`: multiplies by cofactor 8 (three doublings), used in key derivation.
- `ge_fromfe_frombytes_vartime(p2, bytes)`: converts a 32-byte field element to a curve point using the Elligator-like mapping; used in hash-to-point.
- `ge_frombytes_vartime(p3, bytes)`: decompresses a 32-byte compressed point (variable-time).
- `ge_tobytes` / `ge_p3_tobytes`: compresses a point to 32 bytes.
- `ge_p3_is_point_at_infinity_vartime(p)`: checks if a point is the identity element (X = T = 0, Y = Z, Y nonzero).
- `ge_dsm_precomp(r, s)`: precomputes a table for double-scalar multiplication.

### Key Derivation Protocol

1. **Shared secret**: `D = 8 * secretkey * publickey` (cofactor Diffie-Hellman).
2. **Scalar derivation**: `s = Hs(D || varint(output_index))`.
3. **Public key derivation**: `P' = s*G + B` (where B is the base spend public key).
4. **Secret key derivation**: `x' = s + b` (where b is the base spend secret key).
5. **Subaddress derivation**: `B = P' - s*G` (inverse operation to find the base key from an output key).

### Hash-to-Point (hash_to_ec)

The `hash_to_ec` function in `crypto.cpp` maps a public key to a curve point:
1. Compute `h = cn_fast_hash(public_key)` (Keccak-256).
2. Interpret h as a field element and map to a curve point via `ge_fromfe_frombytes_vartime`.
3. Multiply by cofactor 8 (`ge_mul8`) to ensure the result is in the prime-order subgroup.

This is used to generate the point `Hp(P)` in key image computation `I = x * Hp(P)`.

### Random Number Generation

The CSPRNG uses a Keccak sponge construction:
1. **Initialization**: at program startup (via `INITIALIZER`), 32 bytes from `/dev/urandom` (or `CryptGenRandom` on Windows) are read into the 200-byte Keccak state.
2. **Generation**: each request permutes the state via `keccakf` (24 rounds), then copies up to 136 bytes (the Keccak-256 rate) per permutation.
3. **Extra entropy**: can be mixed in via XOR after a permutation, in 136-byte blocks.
4. **Finalization**: the state is zeroed at program exit (via `FINALIZER`).

The `random32_unbiased` function ensures uniform distribution in `[1, l)` by rejection sampling: it rejects random values >= `15 * l` (the 32-byte constant `0xf0000...01390...e3`), then reduces mod l, and retries if the result is zero.

### CryptoNight Slow Hash

The CryptoNight PoW hash (pre-RandomX):
1. Keccak-1600 the input to produce a 200-byte state.
2. Use AES to expand 8 blocks (128 bytes) from the state into a 2 MB scratchpad.
3. Perform 2^20 iterations of memory-hard mixing: read from scratchpad, AES-round, XOR, write back.
4. Compress the scratchpad back into the Keccak state.
5. Select one of four hash functions (Blake-256, Groestl-256, JH-256, Skein-256) based on state bits for final output.

Variants 1-4 introduce modifications to the mixing step to resist ASIC optimization. Variant 4 (CryptoNight-R) uses per-block random math programs, with optional JIT compilation on x86_64.

### RandomX

For blocks at version >= 12 (RX_BLOCK_VERSION), the PoW uses RandomX instead of CryptoNight. The `rx-slow-hash.c` file provides the interface (`rx_slow_hash`), managing dataset initialization and seed hash changes. Thread coordination uses `rx_set_miner_thread` / `rx_get_miner_thread`.

## Dependencies

### What This Module Depends On

| Dependency | Usage |
|-----------|-------|
| `common/pod-class.h` | `POD_CLASS` macro for crypto type definitions |
| `common/varint.h` | `tools::write_varint` for encoding output indices |
| `epee` (mlocked, scrubbed, hex, span) | Memory-locking (`epee::mlocked`), secure erasure (`tools::scrubbed`), hex formatting, byte span utilities |
| `memwipe.h` | Secure memory erasure after use of sensitive values |
| `mlocker.h` | Memory page locking to prevent swapping of secrets |
| `warnings.h` | Compiler warning suppression macros |
| `boost::thread` | Mutex for thread-safe RNG access |
| `boost::optional` | Optional parameters in tx proof functions |
| `boost::shared_ptr` | RAII for malloc'd ring signature buffers |
| `libsodium` | `crypto_verify_32` for constant-time comparison of 32-byte values |
| `randomx` | RandomX PoW hash library |
| `cryptonote_config.h` | `config::HASH_KEY_TXPROOF_V2` domain separator string |

### What Depends On This Module

The crypto module is a foundational dependency used pervasively:

- **`ringct/`** -- RingCT operations (`rctOps`, `rctSigs`, `multiexp`) for confidential transactions, Pedersen commitments, Bulletproofs, and CLSAG ring signatures.
- **`cryptonote_basic/`** -- Transaction and block structures, format utilities, difficulty calculation, mining.
- **`cryptonote_core/`** -- Blockchain validation, transaction pool, tx utilities.
- **`wallet/`** -- Wallet key management, transaction construction, RPC server.
- **`multisig/`** -- Multisignature key exchange, signing contexts.
- **`serialization/`** -- Serialization of crypto types.
- **`rpc/`** -- RPC data structures, payment signatures.
- **`p2p/`** -- Peer list management (key-based identification).
- **`net/`** -- Dandelion++ implementation (random timing).
- **`simplewallet/`** -- CLI wallet implementation.
- **`seraphis_crypto/`** -- Seraphis transcript system.

## Known Issues

| Location | Comment |
|----------|---------|
| `crypto.cpp:150` | `TODO: allow specifying random value (for wallet recovery)` -- This TODO appears to be stale, as the `recovery_key` parameter and `recover` flag were already added to `generate_keys`, implementing the requested functionality. |
