# Key Derivation

## Overview

Monero uses a deterministic key derivation pipeline that produces all wallet keys from a single 32-byte random seed. The seed is encoded as a 25-word mnemonic phrase (24 data words + 1 checksum word) using an Electrum-compatible word list scheme supporting 12 languages. From the seed, the key chain derives a secret spend key and a deterministic secret view key, each with corresponding public keys. Subaddresses extend the key hierarchy to provide unlinkable receive addresses within a single wallet. Output scanning uses an ECDH shared secret combined with view tags (since HF15) to efficiently identify owned outputs. Pedersen commitments in RingCT use a second generator point H whose discrete logarithm with respect to G is unknown.

### Key Files

| File | Description |
|------|-------------|
| `src/mnemonics/electrum-words.cpp` | Mnemonic encoding/decoding: words-to-bytes and bytes-to-words conversion, checksum, language detection |
| `src/mnemonics/electrum-words.h` | Mnemonic constants: `seed_length = 24` |
| `src/mnemonics/language_base.h` | `Language::Base` class: `NWORDS = 1626`, word maps, prefix length, `utf8prefix()` |
| `src/mnemonics/*.h` | 13 language word lists (12 current + EnglishOld) |
| `src/crypto/crypto.cpp` | Core cryptographic operations: key generation, key derivation, view tag derivation |
| `src/crypto/crypto.h` | POD type definitions, `crypto_ops` class declaration, inline API wrappers |
| `src/crypto/generators.cpp` | Generator point constants G and H with reproducibility verification |
| `src/cryptonote_basic/account.cpp` | `account_base::generate()`: seed-to-key-pair pipeline |
| `src/device/device_default.cpp` | Subaddress derivation: `get_subaddress_secret_key()`, `get_subaddress_spend_public_key()`, `get_subaddress()` |
| `src/cryptonote_basic/cryptonote_format_utils.cpp` | Output ownership checking: `is_out_to_acc()`, `is_out_to_acc_precomp()`, `out_can_be_to_acc()` |
| `src/cryptonote_config.h` | Domain separator constants: `HASH_KEY_SUBADDRESS = "SubAddr"` |
| `src/ringct/rctTypes.h` | Pedersen generator constants: `H`, `H2[64]` |

## Mnemonic Seed Encoding

### Word List Properties

Each language has a word list of exactly 1626 words (`NWORDS = 1626` in `language_base.h:108-109`). The word list size is validated at construction time; a mismatch throws `std::runtime_error`.

Source: `src/mnemonics/language_base.h:124-125`

### Supported Languages

The following 12 languages are available for current wallets (listed in `get_language_list()` order). A 13th language, EnglishOld, is supported only for legacy seed decoding and is excluded from `get_language_list()` but included in `find_seed_language()`.

| Language | Native Name | Prefix Length |
|----------|------------|:------------:|
| German | Deutsch | 4 |
| English | English | 3 |
| Spanish | Espanol | 4 |
| French | Francais | 4 |
| Italian | Italiano | 4 |
| Dutch | Nederlands | 4 |
| Portuguese | Portugues | 4 |
| Russian | russkij jazyk | 4 |
| Japanese | nihongo | 3 |
| Chinese (simplified) | zhongwen | 1 |
| Esperanto | Esperanto | 4 |
| Lojban | Lojban | 4 |
| EnglishOld (legacy) | EnglishOld | 4 |

Source: Language constructors in `src/mnemonics/{english,german,spanish,french,italian,dutch,portuguese,russian,japanese,chinese_simplified,esperanto,lojban,english_old}.h`

The **unique prefix length** determines how many leading UTF-8 characters are used for word matching and checksum computation. Words are trimmed to this length via `Language::utf8prefix()`. Trimming is case-insensitive (via `tools::utf8canonical` with `std::towlower`).

Source: `src/mnemonics/language_base.h:59-75` (utf8prefix), `src/mnemonics/language_base.h:77-94` (WordHash, WordEqual)

### Seed Format

A standard seed is 24 data words + 1 checksum word = **25 words** total. The constant `seed_length = 24` is defined in `electrum-words.h:63`. The function `get_is_old_style_seed()` identifies seeds with a word count other than 25 as old-style.

The 24 data words encode a 32-byte (256-bit) secret key. Each group of 3 words encodes 4 bytes, yielding 8 groups for 32 bytes.

Source: `src/mnemonics/electrum-words.h:63`, `src/mnemonics/electrum-words.cpp:479`

### Encoding (bytes-to-words)

The function `bytes_to_words()` converts a 32-byte secret key to a mnemonic phrase:

1. For each 4-byte chunk `i` (0 through 7):
   - Read 4 bytes as a little-endian uint32: `w[0] = SWAP32LE(*(uint32_t*)(src + i*4))`
   - Compute three word indices (N = 1626):
     ```
     w[1] = w[0] % N
     w[2] = ((w[0] / N) + w[1]) % N
     w[3] = (((w[0] / N) / N) + w[2]) % N
     ```
   - Emit `word_list[w[1]]`, `word_list[w[2]]`, `word_list[w[3]]`

2. Append the checksum word (the 25th word).

Source: `src/mnemonics/electrum-words.cpp:381-429`

### Decoding (words-to-bytes)

The function `words_to_bytes()` converts a mnemonic phrase back to the 32-byte secret key:

1. If the seed has 25 words, the last word is a checksum (verified then removed).
2. For each group of 3 consecutive word indices `(w[1], w[2], w[3])` from `matched_indices`:
   ```
   w[0] = w[1] + N * ((N - w[1] + w[2]) % N) + N^2 * ((N - w[2] + w[3]) % N)
   ```
   where N = 1626 (the word list length).
3. Verification: `w[0] % N == w[1]`. If this fails, the seed is invalid.
4. Store as little-endian: `w[0] = SWAP32LE(w[0])`, then append 4 bytes to output.
5. For 12-word Electrum seeds (half-length), the output is duplicated to reach 32 bytes.

Source: `src/mnemonics/electrum-words.cpp:264-347`

### Checksum

The checksum word provides integrity verification for the mnemonic phrase:

1. Concatenate the prefix-trimmed forms of all 24 data words into a single string (each word trimmed to the language's `unique_prefix_length` using the canonical trimmed word from the trimmed word map).
2. Compute CRC-32 (Boost `crc_32_type`) of the concatenated trimmed string.
3. The checksum index is: `crc32_result % 24` (i.e., modulo the number of data words).
4. The checksum word is `seed[checksum_index]` -- a copy of one of the existing data words.
5. Verification: the prefix-trimmed form of the last word (the 25th) must match the prefix-trimmed form of `seed[checksum_index]`.

Source: `src/mnemonics/electrum-words.cpp:192-238`

### Language Detection

The function `find_seed_language()` iterates over all 13 language instances and attempts to match every word in the seed against each language's word map:

- If the seed has a checksum (25 words), matching uses the **trimmed word map** (prefix-trimmed keys).
- If the seed has no checksum, matching uses the **full word map** (exact full words).
- After a full match on a checksummed seed, the checksum is verified. If the checksum fails, the language is saved as a fallback (returned if no other language matches, to handle typographical errors).

Source: `src/mnemonics/electrum-words.cpp:91-184`

## Key Derivation Chain

### Seed to Secret Spend Key

The mnemonic phrase decodes to a 32-byte value. This value is used as the `recovery_key` in `account_base::generate()`, which calls `generate_keys()` with `recover = true`:

1. `sec = recovery_key` (copy the 32 raw bytes)
2. `sc_reduce32(&sec)` -- reduce the 32-byte value modulo the Ed25519 group order `l = 2^252 + 27742317777372353535851937790883648493`
3. Compute `pub = sec * G` via `ge_scalarmult_base`

The result after `sc_reduce32` is the **secret spend key** (`m_spend_secret_key`), and the corresponding point is the **public spend key** (`m_account_address.m_spend_public_key`).

Source: `src/crypto/crypto.cpp:153-173` (generate_keys), `src/cryptonote_basic/account.cpp:166-168`

### Secret Spend Key to Secret View Key

The secret view key is derived deterministically from the secret spend key:

1. Compute `second = Keccak-256(secret_spend_key)` -- hash the 32-byte spend secret key using the raw Keccak permutation (not SHA3).
2. Pass `second` as the recovery key to `generate_keys()` with `recover = true`.
3. `sc_reduce32(&second)` produces the **secret view key** (`m_view_secret_key`).
4. Compute `pub = second * G` to get the **public view key** (`m_account_address.m_view_public_key`).

This means only the secret spend key (or equivalently, the mnemonic seed) is needed to recover the entire wallet. The view key is always deterministically reproducible.

Source: `src/cryptonote_basic/account.cpp:170-174`

### Summary

```
seed_bytes (32 bytes, from mnemonic)
  |
  +-- sc_reduce32(seed_bytes) --> secret_spend_key (b)
  |     |
  |     +-- b * G --> public_spend_key (B)
  |
  +-- sc_reduce32(Keccak-256(b)) --> secret_view_key (a)
        |
        +-- a * G --> public_view_key (A)
```

### Public Key Computation

`secret_key_to_public_key()` computes the public key from a secret key:

1. Validate: `sc_check(&sec) == 0` (the secret key is a canonical scalar in `[0, l)`). Returns `false` on failure.
2. Compute `ge_scalarmult_base(&point, &sec)` -- scalar multiplication of the base point G.
3. Compress: `ge_p3_tobytes(&pub, &point)`.

Source: `src/crypto/crypto.cpp:180-188`

## Subaddress Derivation

Subaddresses allow a wallet to generate an unlimited number of unlinkable receive addresses from a single key pair, organized by account (major index) and address (minor index). The standard address corresponds to `(major=0, minor=0)`.

### Subaddress Secret Key

The function `get_subaddress_secret_key()` computes the subaddress scalar `m`:

```
m = Hs("SubAddr\0" || a || LE32(major) || LE32(minor))
```

Where:
- `"SubAddr\0"` is the 8-byte domain separator `config::HASH_KEY_SUBADDRESS` (7 ASCII chars + null terminator, copied via `sizeof` which includes the null byte).
- `a` is the 32-byte secret view key.
- `LE32(major)` and `LE32(minor)` are the account and address indices as little-endian 32-bit unsigned integers (`SWAP32LE`).
- `Hs` is `hash_to_scalar`: Keccak-256 followed by `sc_reduce32`.

The total input is 8 + 32 + 4 + 4 = **48 bytes**.

Note: the subaddress derivation uses fixed-width `uint32_t` indices (not varint), unlike the output index in key derivation.

Source: `src/device/device_default.cpp:197-208`, `src/cryptonote_config.h:248`

### Subaddress Public Spend Key

The function `get_subaddress_spend_public_key()` computes the subaddress spend public key `D`:

1. If `index.is_zero()` (major=0, minor=0), return the standard spend public key `B` directly.
2. Compute `m = get_subaddress_secret_key(a, index)`.
3. Compute `M = m * G` via `secret_key_to_public_key`.
4. Compute `D = B + M` (point addition via `rct::addKeys`).

Source: `src/device/device_default.cpp:127-141`

### Subaddress View Public Key

The function `get_subaddress()` computes the full subaddress (spend key D, view key C):

1. If `index.is_zero()`, return the standard address `(A, B)` directly.
2. Compute `D = get_subaddress_spend_public_key(keys, index)`.
3. Compute `C = a * D` (scalar multiplication of the view secret key by the subaddress spend public key, via `rct::scalarmultKey`).
4. The subaddress is `(C, D)` where `C` is the view public key and `D` is the spend public key.

Note: `C = a * D = a * (B + m*G) = a*B + a*m*G`. This is NOT equal to the standard view public key `A = a*G`. Each subaddress has a unique view public key.

Source: `src/device/device_default.cpp:181-195`

### Batch Computation

`get_subaddress_spend_public_keys()` efficiently computes spend public keys for a range of minor indices within a single major account. It caches the `ge_cached` form of the base spend public key `B` and performs point additions in-place, avoiding repeated decompression.

Source: `src/device/device_default.cpp:143-179`

## Output Key Derivation (Scanning)

### Transaction Key and Shared Secret

For each transaction, the sender generates a random transaction key pair `(r, R = r*G)`. The public transaction key `R` is included in the transaction's `extra` field.

The ECDH shared secret (key derivation) is:
```
D = 8 * a * R    (receiver computes, using view secret key a)
D = 8 * r * A    (sender computes, using tx secret key r and receiver's view public key A)
```

The cofactor multiplication by 8 (`ge_mul8`) ensures the result is in the prime-order subgroup.

Source: `src/crypto/crypto.cpp:190-203` (generate_key_derivation)

### One-Time Output Public Key

For output at index `idx`, the sender derives the one-time public key:

```
P_out = Hs(D || varint(idx)) * G + B
```

Where:
- `Hs(D || varint(idx))` is `derivation_to_scalar`: Keccak-256 of the derivation concatenated with the varint-encoded output index, reduced mod l.
- `B` is the receiver's spend public key (or subaddress spend public key `D_sub` for subaddresses).
- The operation is point addition: `derive_public_key` computes `scalar*G + base`.

Source: `src/crypto/crypto.cpp:205-235` (derivation_to_scalar, derive_public_key)

### Output Ownership Check

The receiver checks whether an output belongs to their wallet:

**Standard address** (`is_out_to_acc`):
1. Compute `D = 8 * a * R` (shared secret from tx public key and view secret key).
2. If view tag is present, check `out_can_be_to_acc()` first (see View Tag section below).
3. Compute `P' = Hs(D || varint(idx)) * G + B`.
4. If `P' == P_out`, the output belongs to this wallet.

**Subaddress** (`is_out_to_acc_precomp`):
1. Compute `D = 8 * a * R`.
2. If view tag is present, check it first.
3. Compute `B' = P_out - Hs(D || varint(idx)) * G` via `derive_subaddress_public_key`.
4. Look up `B'` in the precomputed subaddress table. If found, the output belongs to the wallet at the corresponding subaddress index.

Source: `src/cryptonote_basic/cryptonote_format_utils.cpp:993-1073`, `src/crypto/crypto.cpp:245-262` (derive_subaddress_public_key)

### Spending (Secret Key Derivation)

To spend an output, the receiver derives the one-time secret key:

```
x = Hs(D || varint(idx)) + b
```

Where `b` is the secret spend key. The operation is scalar addition via `derive_secret_key`.

Source: `src/crypto/crypto.cpp:237-243`

### Key Image

The key image for an output with one-time public key `P` and one-time secret key `x` is:

```
I = x * Hp(P)
```

Where `Hp(P)` is the hash-to-point function:
1. `h = cn_fast_hash(P)` (Keccak-256 of the 32-byte compressed public key).
2. Interpret `h` as a field element, map to a curve point via `ge_fromfe_frombytes_vartime`.
3. Multiply by cofactor 8 (`ge_mul8`) to ensure the result is in the prime-order subgroup.

Key images are deterministic: the same output always produces the same key image, enabling double-spend detection without revealing which output was spent.

Source: `src/crypto/crypto.cpp:611-628` (hash_to_ec, generate_key_image)

## View Tag

### Purpose

View tags are a 1-byte optimization for wallet scanning introduced at hard fork version 15 (`HF_VERSION_VIEW_TAGS = 15` in `cryptonote_config.h:197`). Before view tags, the wallet had to perform an expensive elliptic curve operation (`derive_public_key`) for every output in every transaction to check ownership. With view tags, 255 out of 256 non-owned outputs can be rejected with a single byte comparison, avoiding the EC operation entirely.

### Derivation

The function `derive_view_tag()` computes the view tag:

```
view_tag = cn_fast_hash("view_tag" || D || varint(idx))[0]
```

Where:
- `"view_tag"` is an 8-byte domain separator salt (ASCII bytes, no null terminator, copied via `memcpy(buf.salt, "view_tag", 8)`).
- `D` is the 32-byte key derivation (ECDH shared secret).
- `varint(idx)` is the varint-encoded output index.
- `[0]` means only the first byte of the 32-byte Keccak-256 hash is used.

The structure is packed (`#pragma pack(push, 1)`) to ensure no padding between fields:
```c
struct {
    char salt[8];
    key_derivation derivation;
    char output_index[(sizeof(size_t) * 8 + 6) / 7];
} buf;
```

Source: `src/crypto/crypto.cpp:753-775`

### Properties

- **Size**: 1 byte (`sizeof(crypto::view_tag) == 1`, verified by `static_assert` in `crypto.h:98`).
- **False positive rate**: 1/256 = 0.39%. When scanning an output not addressed to the wallet, there is a 1/256 chance the view tag matches by coincidence, requiring the full EC derivation to definitively reject it. The expected rejection rate without EC operations is 255/256 = 99.6%.
- **Required since HF15**: Outputs in transactions at or after hard fork version 15 include a view tag. The `out_can_be_to_acc()` function checks the view tag before proceeding to derive the output public key. If no view tag is present (pre-HF15 transactions), the function returns `true` (cannot filter, must proceed to full derivation).

Source: `src/crypto/crypto.h:87-89` (view_tag type), `src/crypto/crypto.h:292-298` (derive_view_tag doc comment), `src/cryptonote_basic/cryptonote_format_utils.cpp:993-1016` (out_can_be_to_acc)

## Pedersen H Generator

### H = toPoint(cn_fast_hash(G))

The Pedersen commitment generator `H` is the second base point used in RingCT amount commitments (`C = x*G + a*H` where `x` is the blinding factor and `a` is the amount). Its derivation ensures that the discrete logarithm of H with respect to G is unknown (nothing-up-my-sleeve construction).

**Derivation** (from `generators.cpp:99-121`):
1. Start with G (the standard Ed25519 base point, compressed: `0x5866666666...66`).
2. Hash: `h = cn_fast_hash(G)` (Keccak-256 of the 32-byte compressed G).
3. Decompress: interpret `h` as a compressed Ed25519 point via `ge_frombytes_vartime`. (This step can fail for arbitrary inputs but is known to succeed for the hash of G.)
4. Multiply by cofactor: `H = 8 * point` via `ge_p3_to_p2` then `ge_mul8`, ensuring `H` is in the prime-order subgroup.
5. Compress: `ge_p3_tobytes` produces the final 32-byte representation.

**Hardcoded constant** (hex, little-endian byte order):
```
8b655970153799af2aeadc9ff1add0ea6c7251d54154cfa92c173a0dd39c1f94
```

This value is defined as a `constexpr` in both `src/crypto/generators.cpp:69-70` and `src/ringct/rctTypes.h:634`. Debug builds verify reproducibility by re-deriving H from G and asserting equality.

Source: `src/crypto/generators.cpp:64-70` (constexpr definition), `src/crypto/generators.cpp:99-121` (reproduce_generator_H), `src/ringct/rctTypes.h:633-634`

### H2 Array (Borromean Range Proofs)

The `H2` array contains 64 precomputed points used in the original Borromean range proofs (pre-Bulletproofs):

```
H2[i] = 2^i * H    for i = 0, 1, ..., 63
```

This means `H2[0] = H`, `H2[1] = 2*H`, `H2[2] = 4*H`, and so on. The type is `key64` (an array of 64 `key` values, each 32 bytes). The values are hardcoded as a static constant in `rctTypes.h`.

The first entry `H2[0]` has the same bytes as `H`:
```
8b655970153799af2aeadc9ff1add0ea6c7251d54154cfa92c173a0dd39c1f94
```

The comment notes: "You can regenerate this by running `python2 Test.py HPow2` in the MiniNero repo."

Source: `src/ringct/rctTypes.h:636-639`

## Known Issues

| Location | Comment |
|----------|---------|
| `src/crypto/crypto.cpp:150` | `TODO: allow specifying random value (for wallet recovery)` -- This TODO is stale; the `recovery_key` parameter already implements wallet recovery. |
| `src/device/device_default.cpp:197-208` | Subaddress derivation uses fixed-width `uint32_t` for major/minor indices, limiting the subaddress space to 2^32 accounts with 2^32 addresses each. |
