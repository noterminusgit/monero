# RingCT

## Overview

RingCT (Ring Confidential Transactions) is the cryptographic subsystem that hides transaction amounts while proving they are valid (non-negative and balanced). It combines Pedersen commitments with ring signatures to ensure that the sum of inputs equals the sum of outputs without revealing the actual values. The module implements the full lifecycle of confidential transactions: generation, verification, and decoding. Over successive hard forks, Monero has evolved through several RingCT types -- from the original Borromean range proofs and MLSAG ring signatures, through Bulletproofs, to the current Bulletproofs+ with CLSAG signatures.

## Key Files

| File | Lines | Description |
|------|------:|-------------|
| `src/ringct/rctTypes.h` | 832 | Core data structures: `key`, `ctkey`, `rctSig`, `Bulletproof`, `BulletproofPlus`, `clsag`, `mgSig`, RCT type enums, serialization logic, and type conversion utilities |
| `src/ringct/rctTypes.cpp` | (see dir) | Helper functions for bulletproof amount counting, type classification (`is_rct_simple`, `is_rct_bulletproof`, etc.), and data conversions (`d2h`, `h2d`, `d2b`) |
| `src/ringct/rctOps.h` | 192 | Declarations for elliptic curve operations: scalar/point arithmetic, key generation, hashing, Pedersen commitments, and ECDH encoding/decoding |
| `src/ringct/rctOps.cpp` | 727 | Implementations of curve operations, precomputed zero-commitment lookup table (for common amounts 0-10^19), ECDH encode/decode, and commitment construction |
| `src/ringct/rctSigs.h` | 143 | Public API declarations for all signature schemes: Borromean, MLSAG, CLSAG, range proofs, and the top-level `genRct`/`verRct`/`decodeRct` functions |
| `src/ringct/rctSigs.cpp` | 1614 | Full implementations of Borromean ring signatures, MLSAG, CLSAG, range proof generation/verification, RingCT transaction generation/verification/decoding, and the pre-MLSAG hash construction |
| `src/ringct/bulletproofs.h` | 51 | Public API for Bulletproof range proofs (prove and verify, single and batched) |
| `src/ringct/bulletproofs.cc` | 1098 | Bulletproof implementation: inner-product argument, prove/verify using Straus/Pippenger multi-exponentiation with cached Hi/Gi generators |
| `src/ringct/bulletproofs_plus.h` | 49 | Public API for Bulletproofs+ range proofs (prove and verify, single and batched) |
| `src/ringct/bulletproofs_plus.cc` | 1121 | Bulletproofs+ implementation: weighted inner-product argument with Fiat-Shamir transcript, prove/verify using cached generators |
| `src/ringct/multiexp.h` | (support) | Multi-exponentiation interface (Straus and Pippenger algorithms) |
| `src/ringct/multiexp.cc` | (support) | Multi-exponentiation implementations used by both bulletproof variants |
| `src/ringct/rctCryptoOps.h` | (support) | Low-level Ed25519 crypto operations (triple scalar multiplication, etc.) |
| `src/ringct/rctCryptoOps.c` | (support) | C implementation of custom Ed25519 operations needed by RingCT |

## Data Structures

### `rct::key` (`rctTypes.h:79`)

A 32-byte array representing either a scalar or a compressed Ed25519 curve point.

- **Fields**: `unsigned char bytes[32]`
- **Operators**: Indexing (`operator[]`), constant-time equality (`operator==` via `crypto_verify_32`)
- **Invariant**: When used as a scalar, must be reduced modulo the curve order `L`. When used as a curve point, must be a valid compressed Ed25519 point.

### `rct::ctkey` (`rctTypes.h:98`)

A pair of keys representing a confidential transaction key.

- **Fields**:
  - `key dest` -- Destination: the public address key (public) or spending secret key (private)
  - `key mask` -- Mask: the Pedersen commitment `C = aG + bH` (public) or the blinding factor `a` (private)
- **Invariant**: When public, `mask` is a valid curve point. When private, `mask` is a valid scalar.

### `rct::ecdhTuple` (`rctTypes.h:141`)

ECDH-encrypted data passed to the receiver for amount recovery.

- **Fields**:
  - `key mask` -- Blinding factor `a` (encrypted; zeroed in v2+ encoding)
  - `key amount` -- Amount `b` (encrypted; only 8 bytes used in v2+ encoding)
- **Invariant**: In v2+ (RCTTypeBulletproof2 and later), `mask` is not serialized and only the first 8 bytes of `amount` are stored on-chain.

### `rct::clsag` (`rctTypes.h:182`)

CLSAG (Compact Linkable Spontaneous Anonymous Group) signature structure.

- **Fields**:
  - `keyV s` -- Vector of scalars, one per ring member
  - `key c1` -- Initial challenge scalar
  - `key I` -- Signing key image (not serialized; reconstructed from transaction inputs)
  - `key D` -- Commitment key image (serialized as `D * INV_EIGHT` for safety)
- **Invariant**: `s.size()` equals the ring size (mixin + 1). `I` and `D` must not equal the identity point.

### `rct::mgSig` (`rctTypes.h:169`)

MLSAG (Multilayered Linkable Spontaneous Anonymous Group) signature structure.

- **Fields**:
  - `keyM ss` -- Matrix of scalars indexed by `[column][row]`
  - `key cc` -- Initial challenge scalar
  - `keyV II` -- Key images (not serialized; reconstructed)
- **Invariant**: `ss` must be rectangular. `ss.size()` equals the ring size, `ss[i].size()` equals the number of rows (keys per ring member). Used in RCTTypeFull, RCTTypeSimple, RCTTypeBulletproof, and RCTTypeBulletproof2.

### `rct::rangeSig` (`rctTypes.h:203`)

Borromean range proof structure.

- **Fields**:
  - `boroSig asig` -- Borromean ring signature (`s0[64]`, `s1[64]`, `ee`)
  - `key64 Ci` -- 64 sub-commitments, each committing to 0 or `2^i`
- **Invariant**: `sum(Ci) == C` where `C` is the commitment being proved. Each `Ci` commits to either 0 or `2^i`, proving the committed amount is in `[0, 2^64)`.

### `rct::Bulletproof` (`rctTypes.h:213`)

Bulletproof range proof structure (based on https://eprint.iacr.org/2017/1066).

- **Fields**:
  - `keyV V` -- Pedersen commitments (not serialized; restored from `outPk`)
  - `key A, S` -- Blinded vector commitments
  - `key T1, T2` -- Polynomial commitment points
  - `key taux, mu` -- Blinding scalars
  - `keyV L, R` -- Inner-product proof round elements
  - `key a, b, t` -- Final inner-product scalars
- **Invariant**: `L.size() == R.size()` and both are non-empty. `V.size()` is the number of amounts proved.

### `rct::BulletproofPlus` (`rctTypes.h:250`)

Bulletproofs+ range proof structure (based on https://eprint.iacr.org/2020/735).

- **Fields**:
  - `keyV V` -- Pedersen commitments (not serialized; restored from `outPk`)
  - `key A, A1, B` -- Blinded commitments
  - `key r1, s1, d1` -- Final proof scalars
  - `keyV L, R` -- Weighted inner-product proof round elements
- **Invariant**: `L.size() == R.size()` and both are non-empty. Proof is more compact than `Bulletproof` (fewer elements: no `T1`, `T2`, `taux`, `mu`, `t`).

### `rct::rctSigBase` (`rctTypes.h:319`)

Base (non-prunable) portion of a RingCT signature.

- **Fields**:
  - `uint8_t type` -- RCT type enum (0-6)
  - `key message` -- Transaction prefix hash (not serialized; reconstructed)
  - `ctkeyM mixRing` -- Mix ring of public (destination, commitment) pairs (not serialized; reconstructed)
  - `keyV pseudoOuts` -- Pseudo output commitments (only for RCTTypeSimple; moved to prunable for later types)
  - `vector<ecdhTuple> ecdhInfo` -- Encrypted amount data for each output
  - `ctkeyV outPk` -- Output public keys and commitments
  - `xmr_amount txnFee` -- Transaction fee in atomic units

### `rct::rctSigPrunable` (`rctTypes.h:417`)

Prunable portion of a RingCT signature (can be dropped after sufficient confirmations).

- **Fields**:
  - `vector<rangeSig> rangeSigs` -- Borromean range proofs (RCTTypeFull/Simple)
  - `vector<Bulletproof> bulletproofs` -- Bulletproof range proofs (RCTTypeBulletproof/2/CLSAG)
  - `vector<BulletproofPlus> bulletproofs_plus` -- Bulletproofs+ range proofs (RCTTypeBulletproofPlus)
  - `vector<mgSig> MGs` -- MLSAG signatures (pre-CLSAG types)
  - `vector<clsag> CLSAGs` -- CLSAG signatures (RCTTypeCLSAG and RCTTypeBulletproofPlus)
  - `keyV pseudoOuts` -- Pseudo output commitments (for bulletproof and later types)

### `rct::rctSig` (`rctTypes.h:614`)

Complete RingCT signature, inheriting from `rctSigBase` and containing `rctSigPrunable p`.

- **Method**: `get_pseudo_outs()` returns the appropriate pseudo-outputs vector depending on type (base for RCTTypeSimple, prunable for bulletproof types).

### RCT Type Enum (`rctTypes.h:299`)

| Value | Name | Ring Sig | Range Proof | Era |
|------:|------|----------|-------------|-----|
| 0 | `RCTTypeNull` | None | None | Pre-RingCT (transparent) |
| 1 | `RCTTypeFull` | MLSAG (1 sig) | Borromean | Original RingCT |
| 2 | `RCTTypeSimple` | MLSAG (N sigs) | Borromean | Post-fork |
| 3 | `RCTTypeBulletproof` | MLSAG | Bulletproof | v8 fork |
| 4 | `RCTTypeBulletproof2` | MLSAG | Bulletproof | v10 fork (compact ECDH) |
| 5 | `RCTTypeCLSAG` | CLSAG | Bulletproof | v13 fork |
| 6 | `RCTTypeBulletproofPlus` | CLSAG | Bulletproofs+ | v16 fork (current) |

### Other Notable Structures

- **`multisig_kLRki`** (`rctTypes.h:114`): Multisig key data (k, L, R, ki). The secret `k` is wiped on destruction.
- **`multisig_out`** (`rctTypes.h:123`): Multisig output data (c, mu_p, c0 vectors for all inputs).
- **`RCTConfig`** (`rctTypes.h:309`): Configuration selecting range proof type (`RangeProofBorromean` or `RangeProofPaddedBulletproof`) and BP version (1-4).
- **`geDsmp`** (`rctTypes.h:163`): Wrapper around `ge_dsmp` for precomputed point multiplication.
- **`boroSig`** (`rctTypes.h:156`): Raw Borromean signature data (`s0[64]`, `s1[64]`, `ee`).

### Well-Known Constants

- **`H`** (`rctTypes.h:634`): Secondary basepoint `H = toPoint(cn_fast_hash(G))`, used as the value generator in Pedersen commitments `C = aG + bH`.
- **`H2`** (`rctTypes.h:639`): Precomputed array of `2^i * H` for `i = 0..63`, used in Borromean range proofs.
- **`Z`** (`rctOps.h:62`): Zero scalar (32 zero bytes).
- **`I`** (`rctOps.h:63`): Identity scalar (1 in little-endian).
- **`L`** (`rctOps.h:64`): Curve order.
- **`G`** (`rctOps.h:65`): Ed25519 basepoint (compressed).
- **`EIGHT`** / **`INV_EIGHT`** (`rctOps.h:66-67`): Scalar 8 and its modular inverse, used to prevent small-subgroup attacks on commitment key images.

## Public API

### Key Generation and Initialization (`rctOps.h`)

| Function | Description |
|----------|-------------|
| `key skGen()` / `void skGen(key &)` | Generate a random scalar suitable for use as a secret key or mask. Uses `random32_unbiased`. |
| `key pkGen()` | Generate a random curve point (for testing). Computes `skGen() * G`. |
| `void skpkGen(key &sk, key &pk)` | Generate a random secret key and its corresponding public key. |
| `tuple<ctkey,ctkey> ctskpkGen(xmr_amount)` | Generate a secret/public ctkey pair with a Pedersen commitment to the given amount. |
| `void genC(key &C, const key &a, xmr_amount amount)` | Compute Pedersen commitment `C = aG + amount*H`. |
| `key commit(xmr_amount amount, const key &mask)` | Compute Pedersen commitment `C = mask*G + amount*H`. |
| `key zeroCommit(xmr_amount amount)` | Compute commitment with zero blinding factor: `C = G + amount*H`. Uses a precomputed lookup table for ~90 common denominations (0 through 10^19) for efficiency. |
| `keyM keyMInit(size_t rows, size_t cols)` | Initialize a key matrix of given dimensions. Note: indexed by column first, then row. |
| `keyV skvGen(size_t rows)` | Generate a vector of random secret keys. |
| `bool toPointCheckOrder(ge_p3 *P, const unsigned char *data)` | Deserialize a point and verify it has the correct group order. Returns false if the point is not in the main subgroup. |

### Scalar/Point Arithmetic (`rctOps.h`)

| Function | Description |
|----------|-------------|
| `key scalarmultBase(const key &a)` | Compute `a * G`. Reduces `a` mod L before multiplication. |
| `key scalarmultKey(const key &P, const key &a)` | Compute `a * P` for arbitrary point `P`. Throws on invalid point. |
| `key scalarmultH(const key &a)` | Compute `a * H` using the precomputed H generator. |
| `key scalarmult8(const key &P)` | Compute `8 * P`. Used to clear the cofactor. |
| `bool isInMainSubgroup(const key &a)` | Check if point `a` is in the prime-order subgroup (i.e., `L*a == identity`). |
| `void addKeys(key &AB, const key &A, const key &B)` | Elliptic curve point addition: `AB = A + B`. |
| `key addKeys(const keyV &A)` | Sum a vector of curve points. Returns identity for empty input. |
| `void addKeys1(key &aGB, const key &a, const key &B)` | Compute `aGB = a*G + B`. |
| `void addKeys2(key &aGbB, const key &a, const key &b, const key &B)` | Compute `aGbB = a*G + b*B` via double-scalar multiplication. |
| `void precomp(ge_dsmp rv, const key &B)` | Precompute point `B` for efficient use in `addKeys3`. |
| `void addKeys3(key &aAbB, const key &a, const key &A, const key &b, const ge_dsmp B)` | Compute `a*A + b*B` where `B` is precomputed. |
| `void addKeys_aGbBcC(...)` | Compute `a*G + b*B + c*C` (triple scalar multiplication; B, C precomputed). |
| `void addKeys_aAbBcC(...)` | Compute `a*A + b*B + c*C` (all three precomputed). |
| `void subKeys(key &AB, const key &A, const key &B)` | Curve point subtraction: `AB = A - B`. |
| `bool equalKeys(const key &A, const key &B)` | Byte-wise equality check (not constant-time; for non-secret data). |

### Hashing (`rctOps.h`)

| Function | Description |
|----------|-------------|
| `void cn_fast_hash(key &hash, const void *data, size_t l)` | Keccak-256 hash of arbitrary data. |
| `void hash_to_scalar(key &hash, const void *data, size_t l)` | Hash data and reduce result modulo L to produce a scalar. |
| `key cn_fast_hash(const key &in)` | Hash a single 32-byte key. |
| `key hash_to_scalar(const key &in)` | Hash a key and reduce to scalar. |
| `key cn_fast_hash(const keyV &keys)` | Hash a concatenated vector of keys. |
| `key cn_fast_hash(const ctkeyV &PC)` | Hash a vector of ctkeys (64 bytes each). |
| `void hash_to_p3(ge_p3 &hash8_p3, const key &k)` | Hash a key to a curve point in `ge_p3` representation (multiplied by cofactor 8). |

### ECDH Encoding/Decoding (`rctOps.h`)

| Function | Description |
|----------|-------------|
| `key genAmountEncodingFactor(const key &k)` | Derive the XOR mask for amount encoding from a shared secret. Hashes `"amount" || k`. |
| `key genCommitmentMask(const key &sk)` | Derive the commitment mask from a shared secret. Hashes `"commitment_mask" || sk` to scalar. |
| `void ecdhEncode(ecdhTuple &unmasked, const key &sharedSec, bool v2)` | Encode (encrypt) amount and mask using ECDH shared secret. In v2 mode, zeroes the mask and XORs only the first 8 bytes of amount. In v1 mode, adds hash-derived scalars. |
| `void ecdhDecode(ecdhTuple &masked, const key &sharedSec, bool v2)` | Decode (decrypt) amount and mask. In v2 mode, derives the mask deterministically and XOR-decrypts the amount. |

### Borromean Ring Signatures (`rctSigs.h`)

| Function | Description |
|----------|-------------|
| `boroSig genBorromean(const key64 x, const key64 P1, const key64 P2, const bits indices)` | Generate a Borromean ring signature over 64 parallel 2-member rings. `x[i]` is the secret key for the ring determined by `indices[i]`. |
| `bool verifyBorromean(const boroSig &bb, const key64 P1, const key64 P2)` | Verify a Borromean ring signature. Reconstructs the challenge chain and checks that `eeComputed == bb.ee`. |

### Range Proofs (`rctSigs.h`)

| Function | Description |
|----------|-------------|
| `rangeSig proveRange(key &C, key &mask, const xmr_amount &amount)` | Generate a Borromean range proof proving `amount` is in `[0, 2^64)`. Outputs commitment `C` and blinding factor `mask`. Decomposes amount into bits and creates sub-commitments `Ci` to each bit. |
| `bool verRange(const key &C, const rangeSig &as)` | Verify that `sum(Ci) == C` and each `Ci` commits to 0 or `2^i`. |
| `Bulletproof proveRangeBulletproof(keyV &C, keyV &masks, const vector<uint64_t> &amounts, span<const key> sk, hw::device &hwdev)` | Generate a Bulletproof range proof for multiple amounts. Derives masks from hardware device. Defined in `rctSigs.cpp:125`. |
| `bool verBulletproof(const Bulletproof &proof)` | Verify a single Bulletproof. Catches exceptions from invalid point deserialization. |
| `bool verBulletproof(const vector<const Bulletproof*> &proofs)` | Batch-verify multiple Bulletproofs (more efficient). |
| `BulletproofPlus proveRangeBulletproofPlus(keyV &C, keyV &masks, const vector<uint64_t> &amounts, span<const key> sk, hw::device &hwdev)` | Generate a Bulletproofs+ range proof. Defined in `rctSigs.cpp:151`. |
| `bool verBulletproofPlus(const BulletproofPlus &proof)` | Verify a single Bulletproofs+ proof. |
| `bool verBulletproofPlus(const vector<const BulletproofPlus*> &proofs)` | Batch-verify multiple Bulletproofs+ proofs. |

### MLSAG Signatures (`rctSigs.h`)

| Function | Description |
|----------|-------------|
| `mgSig MLSAG_Gen(const key &message, const keyM &pk, const keyV &xx, unsigned int index, size_t dsRows, hw::device &hwdev)` | Generate an MLSAG signature. `pk` is the key matrix (columns = ring members), `xx` are secret keys for column `index`, and `dsRows` is the number of rows requiring double-spend linkability (key images). |
| `bool MLSAG_Ver(const key &message, const keyM &pk, const mgSig &sig, size_t dsRows)` | Verify an MLSAG signature. Reconstructs the challenge chain over all columns and checks circularity. |
| `mgSig proveRctMG(const key &message, const ctkeyM &pubs, const ctkeyV &inSk, const keyV &outMasks, const ctkeyV &outPk, unsigned int index, const key &txnFee, const key &message, hw::device &hwdev)` | Full RCT MLSAG proof. Constructs a matrix where the last row is `sum(input_commitments) - sum(output_commitments) - fee*H`, proving balance. |
| `mgSig proveRctMGSimple(const key &message, const ctkeyV &pubs, const ctkey &inSk, const key &a, const key &Cout, unsigned int index, hw::device &hwdev)` | Simple RCT MLSAG proof for a single input. The ring has 2 rows: destination key and `commitment - Cout`. |
| `bool verRctMG(...)` / `bool verRctMGSimple(...)` | Corresponding verification functions. |

### CLSAG Signatures (`rctSigs.h`)

| Function | Description |
|----------|-------------|
| `clsag CLSAG_Gen(const key &message, const keyV &P, const key &p, const keyV &C, const key &z, const keyV &C_nonzero, const key &C_offset, unsigned int l, hw::device &hwdev)` | Generate a CLSAG signature. `P` = ring public keys, `p` = secret key for index `l`, `C` = adjusted commitments, `z` = commitment secret, `C_nonzero` = original commitments, `C_offset` = pseudo output. Uses domain-separated aggregation hashes (`HASH_KEY_CLSAG_AGG_0`, `AGG_1`, `ROUND`). |
| `clsag proveRctCLSAGSimple(const key &message, const ctkeyV &pubs, const ctkey &inSk, const key &a, const key &Cout, unsigned int index, hw::device &hwdev)` | Simple RCT CLSAG proof. Subtracts `Cout` from each ring member's commitment to form the adjusted ring, then calls `CLSAG_Gen`. |
| `bool verRctCLSAGSimple(const key &message, const clsag &sig, const ctkeyV &pubs, const key &C_offset)` | Verify a CLSAG signature. Recomputes aggregation hashes, iterates the challenge chain, and checks that the final challenge matches `c1`. Validates that key images `I` and `D*8` are not the identity point. |

### Top-Level RingCT Protocol (`rctSigs.h`)

| Function | Description |
|----------|-------------|
| `rctSig genRct(const key &message, const ctkeyV &inSk, const keyV &destinations, const vector<xmr_amount> &amounts, const ctkeyM &mixRing, const keyV &amount_keys, unsigned int index, ctkeyV &outSk, const RCTConfig &rct_config, hw::device &hwdev)` | Generate a full RCT signature (RCTTypeFull). Creates range proofs for each output, ECDH-encodes amounts, and produces an MLSAG signature over the full mix ring. Limited to single-input transactions. |
| `rctSig genRctSimple(const key &message, const ctkeyV &inSk, const keyV &destinations, const vector<xmr_amount> &inamounts, const vector<xmr_amount> &outamounts, xmr_amount txnFee, const ctkeyM &mixRing, const keyV &amount_keys, const vector<unsigned int> &index, ctkeyV &outSk, const RCTConfig &rct_config, hw::device &hwdev)` | Generate a simple RCT signature. Supports multiple inputs, each with its own ring. Creates pseudo-output commitments where `sum(pseudoOuts) == sum(outPk) + fee*H`. The last pseudo-output mask is derived to ensure balance. Selects ring signature type (MLSAG or CLSAG) and range proof type (Borromean, Bulletproof, or Bulletproofs+) based on `rct_config`. |
| `bool verRct(const rctSig &rv, bool semantics)` | Verify a full RCT signature. When `semantics=true`: checks range proofs (threaded). When `semantics=false`: checks the MLSAG signature against the mix ring. |
| `bool verRctSemanticsSimple(const vector<const rctSig*> &rvv)` | Batch semantic verification for simple RCT: checks `sum(pseudoOuts) == sum(outPk) + fee*H`, and batch-verifies all range proofs. |
| `bool verRctNonSemanticsSimple(const rctSig &rv)` | Non-semantic verification: verifies each ring signature (MLSAG or CLSAG) against its mix ring (threaded). |
| `xmr_amount decodeRct(const rctSig &rv, const key &sk, unsigned int i, key &mask, hw::device &hwdev)` | Decode the amount for output `i` of a full RCT transaction. Uses ECDH decoding with the destination secret key, then verifies the reconstructed commitment matches `outPk[i].mask`. |
| `xmr_amount decodeRctSimple(const rctSig &rv, const key &sk, unsigned int i, key &mask, hw::device &hwdev)` | Decode the amount for output `i` of a simple RCT transaction. Same logic as `decodeRct` but accepts simple RCT types. |
| `key get_pre_mlsag_hash(const rctSig &rv, hw::device &hwdev)` | Compute the message hash used as input to ring signatures. Concatenates three hashes: (1) `rv.message` (tx prefix hash), (2) hash of serialized `rctSigBase`, (3) hash of all range proof data. Delegates final computation to `hwdev.mlsag_prehash`. |

### Bulletproof API (`bulletproofs.h`)

| Function | Description | Preconditions |
|----------|-------------|---------------|
| `Bulletproof bulletproof_PROVE(const key &v, const key &gamma)` | Prove a single value `v` with blinding factor `gamma`. | `v` must be a valid scalar encoding of a 64-bit value. |
| `Bulletproof bulletproof_PROVE(uint64_t v, const key &gamma)` | Prove a single uint64 value. | |
| `Bulletproof bulletproof_PROVE(const keyV &v, const keyV &gamma)` | Aggregated proof for multiple values (as keys). | `v.size() == gamma.size()`, both <= `BULLETPROOF_MAX_OUTPUTS` (16). |
| `Bulletproof bulletproof_PROVE(const vector<uint64_t> &v, const keyV &gamma)` | Aggregated proof for multiple uint64 values. | `v.size() == gamma.size()`, both <= 16. |
| `bool bulletproof_VERIFY(const Bulletproof &proof)` | Verify a single proof. | |
| `bool bulletproof_VERIFY(const vector<const Bulletproof*> &proofs)` | Batch-verify multiple proofs (more efficient). | |
| `bool bulletproof_VERIFY(const vector<Bulletproof> &proofs)` | Batch-verify by value. | |

### Bulletproofs+ API (`bulletproofs_plus.h`)

Same interface pattern as Bulletproofs:

| Function | Description |
|----------|-------------|
| `BulletproofPlus bulletproof_plus_PROVE(...)` | Four overloads matching the Bulletproof API. Maximum aggregation: `BULLETPROOF_PLUS_MAX_OUTPUTS` (16). |
| `bool bulletproof_plus_VERIFY(...)` | Three overloads: single proof, vector of pointers (batch), vector by value. |

### Type Classification Utilities (`rctTypes.h`)

| Function | Returns true for |
|----------|-----------------|
| `bool is_rct_simple(int type)` | RCTTypeSimple, Bulletproof, Bulletproof2, CLSAG, BulletproofPlus |
| `bool is_rct_bulletproof(int type)` | RCTTypeBulletproof, Bulletproof2, CLSAG |
| `bool is_rct_bulletproof_plus(int type)` | RCTTypeBulletproofPlus |
| `bool is_rct_borromean(int type)` | RCTTypeFull, RCTTypeSimple |
| `bool is_rct_clsag(int type)` | RCTTypeCLSAG, RCTTypeBulletproofPlus |

### Data Conversion Utilities (`rctTypes.h`)

| Function | Description |
|----------|-------------|
| `key d2h(xmr_amount val)` | Convert uint64 to 32-byte little-endian key. |
| `xmr_amount h2d(const key &test)` | Convert first 8 bytes of key to uint64. |
| `void d2b(bits amountb, xmr_amount val)` | Convert uint64 to 64-element bit array. |
| `xmr_amount b2d(bits amountb)` | Convert 64-element bit array to uint64. |

### Type-Casting Utilities (`rctTypes.h`)

Inline cast functions between `rct::key` and `crypto::public_key` / `crypto::secret_key` / `crypto::key_image` / `crypto::hash`: `pk2rct`, `sk2rct`, `ki2rct`, `hash2rct`, and their reverses `rct2pk`, `rct2sk`, `rct2ki`, `rct2hash`. All are reinterpret casts since both types are 32-byte arrays.

## Internal Logic

### Pedersen Commitments

The fundamental building block of RingCT is the Pedersen commitment: `C = a*G + b*H`, where `G` is the Ed25519 basepoint, `H` is a nothing-up-my-sleeve secondary generator derived as `H = toPoint(cn_fast_hash(G))`, `a` is the blinding factor (mask), and `b` is the committed value (amount).

Pedersen commitments are additively homomorphic: `C1 + C2 = (a1+a2)*G + (b1+b2)*H`. This allows verifying that the sum of input amounts equals the sum of output amounts by checking that the sum of input commitments minus the sum of output commitments minus `fee*H` equals a commitment to zero (i.e., a multiple of `G`).

### Pre-MLSAG Hash Construction (`get_pre_mlsag_hash`)

Before signing, the message for the ring signature is constructed by hashing three components:

1. **`rv.message`**: The transaction prefix hash.
2. **Serialized `rctSigBase`**: Includes the RCT type, fee, ECDH info, and output public keys/commitments. Serialized to binary and hashed with `cn_fast_hash`.
3. **Range proof data**: All range proof elements (Borromean scalars/commitments, or Bulletproof/Bulletproofs+ proof elements) are concatenated into a key vector and hashed. For Bulletproofs, V commitments are excluded (they are already covered by outPk in part 2).

These three hashes are passed to `hwdev.mlsag_prehash()` to produce the final signing message, allowing hardware wallets to participate in signing.

### Borromean Range Proofs

Used in RCTTypeFull and RCTTypeSimple. Each output amount is decomposed into 64 bits. For each bit position `i`:
- A sub-commitment `Ci = ai*G + bit_i * 2^i * H` is created (committing to 0 or `2^i`).
- A Borromean ring signature proves knowledge of `ai` for either `Ci` (if `bit=0`) or `Ci - 2^i*H` (if `bit=1`).

The mask is `sum(ai)` and the commitment is `sum(Ci) = mask*G + amount*H`.

**Proving flow** (`proveRange`, line 535 in rctSigs.cpp):
1. Decompose amount to bits.
2. For each bit: generate random `ai`, compute `Ci`, compute `CiH = Ci - H2[i]`.
3. Accumulate `mask = sum(ai)`, `C = sum(Ci)`.
4. Call `genBorromean` with the 64 parallel 2-member rings.

**Verification flow** (`verRange`, line 567):
1. Recompute `CiH[i] = Ci - H2[i]` and `Ctmp = sum(Ci)`.
2. Check `Ctmp == C`.
3. Call `verifyBorromean` on the signature with the two sets of points.

### MLSAG (Multilayered Linkable Spontaneous Anonymous Group)

Reference: https://eprint.iacr.org/2015/1098

MLSAG extends linkable ring signatures to prove knowledge of a secret key in one column of a key matrix, supporting multiple rows with different linkability requirements.

**Proving flow** (`MLSAG_Gen`, line 377):
1. For the signing column `index`: compute `alpha[j]` (random nonces), `aG[j] = alpha[j]*G`, and for linkable rows: `aHP[j] = alpha[j]*Hp(pk[index][j])`, plus key images `II[j] = xx[j]*Hp(pk[index][j])`.
2. Hash the nonces to get challenge `c_old`.
3. For each non-signing column: generate random `ss[i]`, compute `L = ss[i][j]*G + c_old*pk[i][j]` and (for linkable rows) `R = ss[i][j]*Hp(pk[i][j]) + c_old*II[j]`, then hash to get next challenge.
4. Close the ring at the signing index: `ss[index][j] = alpha[j] - c * xx[j]`.

**Verification flow** (`MLSAG_Ver`, line 462):
1. Starting from `c_old = cc`, for each column recompute L and R values from `ss` and the challenge.
2. After processing all columns, verify `c_final == cc` (circular challenge chain).

### CLSAG (Compact Linkable Spontaneous Anonymous Group)

Reference: https://eprint.iacr.org/2019/654

CLSAG is a more efficient replacement for MLSAG. It aggregates the signing key and commitment key into a single ring signature using two aggregation coefficients `mu_P` and `mu_C`, producing a signature that is roughly half the size of MLSAG.

**Proving flow** (`CLSAG_Gen`, line 243):
1. Compute key images: `I = p * Hp(P[l])`, `D = z * Hp(P[l])`. Store `D * INV_EIGHT` for serialization safety.
2. Compute aggregation hashes `mu_P` and `mu_C` using domain-separated hashing over the ring keys, key images, and commitment offset.
3. Generate random nonce `a`, compute `aG = a*G` and `aH = a*Hp(P[l])`.
4. Hash the initial round data to get challenge `c`.
5. For each decoy index: generate random `s[i]`, compute:
   - `L = s[i]*G + (c*mu_P)*P[i] + (c*mu_C)*C[i]`
   - `R = s[i]*Hp(P[i]) + (c*mu_P)*I + (c*mu_C)*D`
   - Hash to get next challenge.
6. Close the ring: `s[l] = a - c*(mu_P*p + mu_C*z)`.

**Verification flow** (`verRctCLSAGSimple`, line 875):
1. Validate scalar and key image constraints.
2. Reconstruct `D*8` from the serialized `D` (which is `D*INV_EIGHT`).
3. Recompute `mu_P`, `mu_C` from the ring data.
4. Starting from `c = c1`, iterate the challenge chain through all ring members.
5. Verify `c_final == c1`.

### CLSAG Mathematical Specification

Source: `src/ringct/rctSigs.cpp:243-370` (proving), `src/ringct/rctSigs.cpp:875-990` (verification)

This section provides the exact mathematical formulas implemented in Monero's CLSAG, suitable for reimplementation.

**Notation:**
- `P[0..n-1]`: Ring public keys (destination keys)
- `C[0..n-1]`: Ring commitment keys (original, non-zero commitments)
- `C_offset`: Pseudo output commitment
- `l`: Index of the real signing key in the ring
- `p`: Secret spend key for `P[l]`
- `z`: Secret blinding factor such that `z*G = C[l] - C_offset`
- `I`: Key image = `p * Hp(P[l])`
- `D`: Commitment key image = `z * Hp(P[l])`
- `Hp()`: Hash-to-point function (cofactor-8 hash)
- `Hs()`: Hash-to-scalar function (Keccak-256 reduced mod L)

**Domain separators** (from `src/cryptonote_config.h:260-262`):

All domain separator strings are copied into 32-byte zero-filled buffers via `sc_0()` + `memcpy()`:

| Constant | String (bytes) | Purpose |
|----------|---------------|---------|
| `HASH_KEY_CLSAG_AGG_0` | `"CLSAG_agg_0"` (11 bytes) | Aggregation hash for key component `mu_P` |
| `HASH_KEY_CLSAG_AGG_1` | `"CLSAG_agg_1"` (11 bytes) | Aggregation hash for commitment component `mu_C` |
| `HASH_KEY_CLSAG_ROUND` | `"CLSAG_round"` (11 bytes) | Per-round challenge hash |

**Step 1: Key images**
```
I = p * Hp(P[l])
D = z * Hp(P[l])
sig.D = D * INV_EIGHT          (stored for serialization safety)
```

**Step 2: Aggregation hashes**
```
mu_P = Hs("CLSAG_agg_0\0...0" || P[0] || ... || P[n-1] || C[0] || ... || C[n-1] || I || sig.D || C_offset)
mu_C = Hs("CLSAG_agg_1\0...0" || P[0] || ... || P[n-1] || C[0] || ... || C[n-1] || I || sig.D || C_offset)
```
Note: The hash input uses `sig.D` (= `D * INV_EIGHT`), not `D` itself. The domain separator occupies 32 bytes (11-byte string + 21 zero bytes).

**Step 3: Initial challenge**

Generate random nonce `alpha`. Compute:
```
aG = alpha * G
aH = alpha * Hp(P[l])
c = Hs("CLSAG_round\0...0" || P[0..n-1] || C[0..n-1] || C_offset || message || aG || aH)
```

**Step 4: Ring traversal** (starting from `i = (l+1) % n`)

For each decoy index `i ≠ l`, generate random `s[i]` and compute:
```
L = s[i]*G + (c*mu_P)*P[i] + (c*mu_C)*(C[i] - C_offset)
R = s[i]*Hp(P[i]) + (c*mu_P)*I + (c*mu_C)*D
c_new = Hs("CLSAG_round\0...0" || P[0..n-1] || C[0..n-1] || C_offset || message || L || R)
c = c_new
```

If `i+1 == 0 (mod n)`, store `c` as `sig.c1` (the initial challenge for verification).

**Step 5: Close the ring**
```
s[l] = alpha - c * (mu_P * p + mu_C * z)
```

**Verification** (`verRctCLSAGSimple`):

1. Reconstruct `D_8 = scalarmult8(sig.D)` (reverses the `INV_EIGHT` storage)
2. Validate: `I ≠ identity`, `D_8 ≠ identity`, all `s[i]` are reduced scalars
3. Recompute `mu_P`, `mu_C` from the ring data (same formula as signing)
4. Starting from `c = sig.c1`, iterate through all `n` ring members computing `L`, `R`, and the next challenge
5. Verify: `c_final == sig.c1` (the challenge chain closes)

### Bulletproofs+ Mathematical Specification

Source: `src/ringct/bulletproofs_plus.cc` (1121 lines). Reference: https://eprint.iacr.org/2020/735

This section provides the exact generator construction, proof structure, and notation conventions used in Monero's Bulletproofs+ implementation, suitable for reimplementation.

**Generator construction** (from `bulletproofs_plus.cc:109-138`):

Generators are derived deterministically from the Pedersen generator `H`:
```
Hi[i] = hash_to_point(cn_fast_hash(H || "bulletproof_plus" || varint(2*i)))
Gi[i] = hash_to_point(cn_fast_hash(H || "bulletproof_plus" || varint(2*i + 1)))
```

Where:
- `H` is the 32-byte Pedersen generator (`rctTypes.h:634`)
- `"bulletproof_plus"` is the literal string `config::HASH_KEY_BULLETPROOF_PLUS_EXPONENT` (16 bytes)
- `varint()` encodes the index using Monero's standard 7-bit varint format
- `hash_to_point()` applies `rct::hash_to_p3()` which hashes to a curve point (cofactor-8)
- Total generators: `Hi[0..maxN*maxM-1]` and `Gi[0..maxN*maxM-1]`, where `maxN=64` and `maxM=16`

Source: `src/cryptonote_config.h:245-246`

| Constant | Value | Purpose |
|----------|-------|---------|
| `HASH_KEY_BULLETPROOF_PLUS_EXPONENT` | `"bulletproof_plus"` | Domain separator for generator derivation |
| `HASH_KEY_BULLETPROOF_PLUS_TRANSCRIPT` | `"bulletproof_plus_transcript"` | Initial Fiat-Shamir transcript hash |

**Parameters:**
- `maxN = 64`: Bits per value (proves values in `[0, 2^64)`)
- `maxM = BULLETPROOF_PLUS_MAX_OUTPUTS = 16`: Maximum aggregated values per proof
- Inner product rounds: `log2(N * M)` where `M` is padded to next power of 2
- Multi-exponentiation: Straus algorithm for ≤ 232 points, Pippenger for larger sets

**CRITICAL: Notation swap** (from source comment in `bulletproofs_plus.cc`):

Monero swaps the roles of generators relative to the paper (eprint 2020/735):
- **Monero's `H`** = paper's `g` (the **value** generator in commitments `C = mask*G + amount*H`)
- **Monero's `G`** = paper's `h` (the **blinding** generator)

This means Pedersen commitments are `C = mask*G + amount*H`, where `mask` is the blinding factor and `amount` is the committed value.

**Proof structure** (`rctTypes.h:250`):
```
BulletproofPlus = (A, A1, B, r1, s1, d1, L[0..rounds-1], R[0..rounds-1])
```

Where:
- `A`: Blinded vector commitment (curve point)
- `A1, B`: Weighted inner-product proof elements (curve points)
- `r1, s1, d1`: Final proof scalars
- `L[i], R[i]`: Per-round inner-product proof elements (curve points)
- `V` (commitments) are NOT serialized — restored from `outPk` during verification

Compared to original Bulletproofs `(A, S, T1, T2, taux, mu, L, R, a, b, t)`, BP+ saves 3 scalars.

**Transcript** (Fiat-Shamir):

The initial transcript hash is:
```
transcript = Hs("bulletproof_plus_transcript")
```

The transcript is updated at each proof step by hashing the current transcript state with new proof elements.

**Precomputed constant:**

`TWO_SIXTY_FOUR_MINUS_ONE = 2^64 - 1` is precomputed by repeated squaring of `2` (6 iterations), then subtracting 1. Used in verification to simplify range check equations.

### Bulletproofs

Reference: https://eprint.iacr.org/2017/1066

Bulletproofs replace Borromean range proofs with a logarithmic-size proof based on an inner-product argument. They prove that committed values lie in `[0, 2^64)`.

**Key parameters**:
- `maxN = 64`: bits per value
- `maxM = BULLETPROOF_MAX_OUTPUTS = 16`: maximum aggregated values per proof
- Generator caches: `Hi_p3[maxN*maxM]`, `Gi_p3[maxN*maxM]` derived deterministically from `H` using domain-separated hashing with `HASH_KEY_BULLETPROOF_EXPONENT`.
- Multi-exponentiation uses Straus (for <= 232 points) or Pippenger (larger) algorithms with precomputed caches.

**Proving flow** (`bulletproof_PROVE`, line 486):
1. Pad the number of values to the next power of 2.
2. Compute value vectors `aL` (bits of values) and `aR = aL - 1`.
3. Generate blinding vectors and compute commitments `A = alpha*G + <aL, Gi> + <aR, Hi>` and `S = rho*G + <sL, Gi> + <sR, Hi>`.
4. Compute Fiat-Shamir challenges `y`, `x`.
5. Compute polynomial commitments `T1`, `T2`.
6. Compute challenge `x` and evaluate: `taux`, `mu`, `t`.
7. Run the inner-product argument (log rounds): at each round, split the vectors, compute `L[i]` and `R[i]`, get challenge, fold.
8. Output proof elements: `(V, A, S, T1, T2, taux, mu, L, R, a, b, t)`.

**Verification flow** (`bulletproof_VERIFY`, line 810):
1. Reconstruct all Fiat-Shamir challenges from the proof elements.
2. Compute the multi-exponentiation check combining all proof equations into a single equation.
3. Batch verification: for multiple proofs, combine them with random weights to detect any invalid proof with overwhelming probability.
4. Final check is a single multi-exponentiation equaling the identity point.

### Bulletproofs+

Reference: https://eprint.iacr.org/2020/735

Bulletproofs+ is an improvement over Bulletproofs using a weighted inner-product argument. It produces smaller proofs (fewer group elements) with a faster verifier.

**Key differences from Bulletproofs**:
- Uses a weighted inner-product argument instead of the standard one.
- The proof contains `(A, A1, B, r1, s1, d1, L, R)` instead of `(A, S, T1, T2, taux, mu, L, R, a, b, t)` -- 3 fewer scalars.
- Uses a constant initial Fiat-Shamir transcript hash (`HASH_KEY_BULLETPROOF_PLUS_TRANSCRIPT`).
- Generators are derived using `HASH_KEY_BULLETPROOF_PLUS_EXPONENT` (distinct from Bulletproof generators).
- Pre-computes `2^64 - 1` for use in verification.

**NOTE ON NOTATION** (from source comment): In Monero's construction, the roles of generators `g` and `h` from the paper are swapped. `H` takes the role of `g` (value generator) and `G` takes the role of `h` (blinding generator).

**Proving flow** (`bulletproof_plus_PROVE`, line 513):
1. Pad values to next power of 2.
2. Build vectors for the weighted inner-product relation.
3. Run the weighted inner-product protocol with Fiat-Shamir challenges.
4. Output proof: `(V, A, A1, B, r1, s1, d1, L, R)`.

**Verification flow** (`bulletproof_plus_VERIFY`, line 799):
1. Reconstruct challenges from proof transcript.
2. Build a single multi-exponentiation check.
3. Support batch verification with random weights.
4. The verification equation checks all proof relations simultaneously.

### genRctSimple Flow (Transaction Signing)

The `genRctSimple` function (line 1108) is the main entry point for creating confidential transactions:

1. **Select RCT type** based on `rct_config.bp_version`: 0/4 = BulletproofPlus, 3 = CLSAG, 2 = Bulletproof2, 1 = Bulletproof.
2. **Generate range proofs**: Either Borromean (per-output) or aggregated Bulletproof/Bulletproofs+ (single proof for all outputs). In fake transaction mode (hardware wallet first pass), dummy proofs are used.
3. **ECDH-encode amounts**: Encrypt each output's amount and mask using the shared secret with the receiver.
4. **Construct pseudo-output commitments**: For `N` inputs, generate `N-1` random masks `a[i]` and derive the last mask as `a[N-1] = sum(output_masks) - sum(a[0..N-2])` to ensure `sum(pseudoOuts) == sum(outPk) + fee*H`.
5. **Compute pre-MLSAG hash**: Hash the transaction prefix, base signature, and range proof data.
6. **Generate ring signatures**: One CLSAG (or MLSAG) per input, each proving knowledge of the spending key and commitment blinding factor for one member of the ring.

### verRctSemanticsSimple + verRctNonSemanticsSimple Flow

Verification is split into two phases:

1. **Semantic verification** (`verRctSemanticsSimple`, line 1344): Checks structural validity and the balance equation. Verifies `sum(pseudoOuts) == sum(outPk) + fee*H`. Batch-verifies all range proofs (Bulletproofs/Bulletproofs+ batched across transactions; Borromean individually threaded).

2. **Non-semantic verification** (`verRctNonSemanticsSimple`, line 1484): Verifies each ring signature (CLSAG or MLSAG) against its mix ring. Each verification is submitted to a thread pool for parallel execution.

## Dependencies

### What RingCT depends on

- **`crypto/`**: Ed25519 curve operations (`crypto-ops.h`, `crypto-ops.c`), key types (`crypto.h`), random number generation (`random.h`), Keccak hashing (`keccak.h`)
- **`crypto/generic-ops.h`**: Generic comparison operators for crypto types
- **`sodium/crypto_verify_32.h`**: Constant-time 32-byte comparison (used in `key::operator==`)
- **`common/perf_timer.h`**: Performance timing macros (`PERF_TIMER`)
- **`common/threadpool.h`**: Thread pool for parallel verification
- **`common/util.h`**: Utility functions
- **`serialization/`**: Binary, JSON, and debug archive serialization infrastructure
- **`cryptonote_config.h`**: Configuration constants (`BULLETPROOF_MAX_OUTPUTS`, `BULLETPROOF_PLUS_MAX_OUTPUTS`, hash key domain separators)
- **`device/device.hpp`**: Hardware wallet device abstraction (`hw::device`) for key operations

### What depends on RingCT

- **`cryptonote_core/blockchain.cpp`**: Transaction validation during block processing
- **`cryptonote_core/cryptonote_core.cpp`**: Core node logic
- **`cryptonote_core/tx_verification_utils.cpp`**: Transaction verification utilities
- **`cryptonote_core/cryptonote_tx_utils.cpp`** / `.h`: Transaction construction
- **`wallet/wallet2.cpp`** / `.h`: Wallet transaction building and amount decoding
- **`rpc/`**: RPC data structures and handlers
- **`multisig/`**: Multisig transaction building and CLSAG context (`multisig_tx_builder_ringct`, `multisig_clsag_context`)
- **`device/device_ledger.cpp`**: Ledger hardware wallet integration
- **`device_trezor/trezor/protocol.cpp`**: Trezor hardware wallet integration
- **`simplewallet/simplewallet.cpp`**: Simple wallet CLI

## Known Issues

- **`src/ringct/rctSigs.cpp:1219`**: `TODO: unused ??` -- Comments out `txnFeeKey` computation in `genRctSimple`, noting it appears unused. The fee key is computed elsewhere when needed for verification but this local variable was apparently left as dead code.

No other TODO, FIXME, HACK, or XXX comments were found in the ringct source files.
