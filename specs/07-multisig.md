# Multisig

## Overview

The multisig module implements M-of-N threshold multisignature functionality for Monero wallets. It provides the cryptographic infrastructure for multiple participants (N, up to 16) to collaboratively construct a shared public key such that any subset of M participants can produce valid transaction signatures. The module handles three major concerns: (1) a multi-round Diffie-Hellman key exchange (KEX) protocol to establish the shared multisig spend key and common view key, (2) key image generation for outputs owned by the multisig group, and (3) a MuSig2-style multisig CLSAG signing protocol for constructing RingCT transactions. The design is based on MRL-0009, with key aggregation coefficients to prevent key cancellation attacks, and references MuSig2 and FROST for the signing nonce scheme.

## Key Files

| File | Lines | Description |
|------|-------|-------------|
| `src/multisig/multisig_account.h` | 303 | Core `multisig_account` class definition with KEX state machine and all member fields |
| `src/multisig/multisig_account.cpp` | 211 | Account constructors, status queries, `initialize_kex`, `kex_update`, config validation, and round-count helpers |
| `src/multisig/multisig_account_kex_impl.cpp` | 972 | Full KEX implementation: DH round processing, message sanitization, key aggregation, round finalization, booster messages |
| `src/multisig/multisig_kex_msg.h` | 109 | `multisig_kex_msg` class: KEX message construction, parsing, and signature verification |
| `src/multisig/multisig_kex_msg.cpp` | 293 | KEX message serialization (base58 + binary archive), signature creation/validation, versioned parsing (V2) |
| `src/multisig/multisig_kex_msg_serialization.h` | 78 | Serialization structs for round-1 and general KEX messages |
| `src/multisig/multisig_tx_builder_ringct.h` | 119 | `tx_builder_ringct_t` class for building multisig RingCT transactions with CLSAG signatures |
| `src/multisig/multisig_tx_builder_ringct.cpp` | 1058 | Full transaction construction: input/output preparation, range proofs, RCT signature scaffolding, partial signing |
| `src/multisig/multisig_clsag_context.h` | 137 | `CLSAG_context_t` class for multisig CLSAG challenge computation with MuSig2-style nonce combination |
| `src/multisig/multisig_clsag_context.cpp` | 257 | CLSAG context initialization, mu coefficient computation, challenge ring traversal, nonce combination |
| `src/multisig/multisig.h` | 69 | Standalone helpers: blinded secret key derivation, key image generation, composite key image assembly |
| `src/multisig/multisig.cpp` | 140 | Implementation of blinded key derivation, partial key image generation, and composite key image construction |
| `src/multisig/CMakeLists.txt` | 58 | Build configuration; links against `ringct`, `cryptonote_basic`, `cryptonote_core`, `common`, `cncrypto` |

**Total: ~3,729 lines across 13 files.**

## Data Structures

### `multisig::multisig_account_status`

Declared in `multisig_account.h:43-54`. A plain struct summarizing the state of a multisig account.

| Field | Type | Description |
|-------|------|-------------|
| `multisig_is_active` | `bool` | Whether the account has been initialized (at least one KEX round completed) |
| `kex_is_done` | `bool` | Whether all main KEX rounds have completed |
| `is_ready` | `bool` | Whether the account is fully ready to sign (main rounds + post-KEX verification done) |
| `threshold` | `uint32_t` | M in M-of-N |
| `total` | `uint32_t` | N in M-of-N |

### `multisig::multisig_account`

Declared in `multisig_account.h:94-280`. The central class managing multisig account state and the KEX protocol. Marked `final`.

**Type alias:**
- `multisig_keyset_map_memsafe_t` = `std::unordered_map<crypto::public_key_memsafe, std::unordered_set<crypto::public_key>>` -- Maps a (memsafe) public key to the set of participant base pubkeys ("origins") that share knowledge of it.

**Member variables (private):**

| Field | Type | Description |
|-------|------|-------------|
| `m_threshold` | `uint32_t` | Minimum co-signers needed (M). Default 0. |
| `m_signers` | `vector<crypto::public_key>` | Sorted base pubkeys of all N participants; used as participant IDs |
| `m_base_privkey` | `crypto::secret_key` | This participant's base private key (used for DH exchanges, message signing) |
| `m_base_pubkey` | `crypto::public_key` | Derived from `m_base_privkey` |
| `m_base_common_privkey` | `crypto::secret_key` | This participant's contribution to the shared common (view) private key |
| `m_multisig_privkeys` | `vector<crypto::secret_key>` | This participant's private key shares of the multisig spend key |
| `m_common_privkey` | `crypto::secret_key` | Aggregate common private key (shared by all, used as the multisig view key) |
| `m_multisig_pubkey` | `crypto::public_key` | The group's multisig public spend key |
| `m_common_pubkey` | `crypto::public_key` | The group's common public key (view key) |
| `m_kex_rounds_complete` | `uint32_t` | Number of KEX rounds completed so far. Default 0. |
| `m_kex_keys_to_origins_map` | `multisig_keyset_map_memsafe_t` | Pubkeys for the in-progress KEX round mapped to origin signers |
| `m_next_round_kex_message` | `string` | Serialized KEX message for the current in-progress round |

**Invariants:**
- `m_signers` is always sorted and contains unique elements when set.
- `m_base_pubkey` is always in `m_signers`.
- All signer pubkeys are in the prime-order subgroup of Ed25519 and are not the identity point.
- `1 <= m_threshold <= m_signers.size()` and `2 <= m_signers.size() <= 16` (when configured).
- `m_kex_rounds_complete <= multisig_kex_rounds_required(N, M) + 1`.
- After `main_kex_rounds_done()`, `m_kex_keys_to_origins_map` is cleared and `m_multisig_pubkey` is the final aggregate key.

### `multisig::multisig_kex_msg`

Declared in `multisig_kex_msg.h:50-108`. Represents a single KEX protocol message. Marked `final`.

| Field | Type | Description |
|-------|------|-------------|
| `m_msg` | `string` | Full serialized message string (magic prefix + base58-encoded content + signature) |
| `m_kex_round` | `uint32_t` | Which KEX round this message belongs to |
| `m_msg_pubkeys` | `vector<crypto::public_key>` | Public keys carried by the message (empty in round 1) |
| `m_msg_privkey` | `crypto::secret_key` | Private key carried in round-1 messages (the base common privkey); `null_skey` otherwise |
| `m_signing_pubkey` | `crypto::public_key` | The base pubkey of the signer who created this message |

**Message format:**
```
msg_content = kex_round | signing_pubkey | expand(msg_pubkeys) | OPTIONAL msg_privkey
msg_to_sign = versioning-domain-sep | msg_content
msg = versioning-domain-sep | base58(serialize(msg_content | crypto_sig(msg_to_sign)))
```

**Message version prefixes:**
- `MultisigV1` -- Deprecated V1 (rejected on parse).
- `MultisigxV1` -- Deprecated V1 exchange message (rejected on parse).
- `MultisigxV2R1` -- V2 round 1 message (carries a private key).
- `MultisigxV2Rn` -- V2 round N>1 message (carries public keys).

### `multisig_kex_msg_serializable_round1`

Declared in `multisig_kex_msg_serialization.h:43-57`. Serialization-only struct for round-1 KEX messages.

| Field | Type |
|-------|------|
| `msg_privkey` | `crypto::secret_key` |
| `signing_pubkey` | `crypto::public_key` |
| `signature` | `crypto::signature` |

### `multisig_kex_msg_serializable_general`

Declared in `multisig_kex_msg_serialization.h:60-77`. Serialization-only struct for round N>1 KEX messages.

| Field | Type |
|-------|------|
| `kex_round` | `uint32_t` (varint serialized) |
| `msg_pubkeys` | `vector<crypto::public_key>` |
| `signing_pubkey` | `crypto::public_key` |
| `signature` | `crypto::signature` |

### `multisig::signing::CLSAG_context_t`

Declared in `multisig_clsag_context.h:47-133`. Manages the state for computing a single CLSAG ring signature in a multisig context using MuSig2-style nonce combination. Marked `final`.

| Field | Type | Description |
|-------|------|-------------|
| `initialized` | `bool` | Whether `init()` has been called successfully |
| `c_params` | `rct::keyV` | Challenge hash input: domain-sep, {P}, {C}, C_offset, message, L, R |
| `c_params_L_offset` | `size_t` | Index in `c_params` where L will be placed |
| `c_params_R_offset` | `size_t` | Index in `c_params` where R will be placed |
| `b_params` | `rct::keyV` | MuSig2 nonce-combination factor hash input (extended with ring data, nonces, fake responses, indices) |
| `b_params_L_offset` | `size_t` | Index in `b_params` for L aggregate nonces |
| `b_params_R_offset` | `size_t` | Index in `b_params` for R aggregate nonces |
| `mu_P` | `rct::key` | CLSAG 'concise' aggregation coefficient for the one-time address ring {P} |
| `mu_C` | `rct::key` | CLSAG 'concise' aggregation coefficient for the commitment ring {C - C_offset} |
| `n` | `size_t` | Ring size |
| `wH_l_precomp` | `rct::geDsmp` | Precomputed aggregate key image: mu_P*I + mu_C*D |
| `W_precomp` | `vector<rct::geDsmp>` | Precomputed aggregate ring members: mu_P*P_i + mu_C*(C_i - C_offset) |
| `H_precomp` | `vector<rct::geDsmp>` | Precomputed H_p(P_i) for each ring member |
| `G_precomp` | `rct::geDsmp` | Precomputed generator G |
| `l` | `size_t` | Real signing index in the ring |
| `s` | `rct::keyV` | Signature responses (fake responses pre-filled, real response slot to be computed) |
| `num_alpha_components` | `size_t` | Number of parallel signing nonces per signer (constant = 2, as in MuSig2) |

### `multisig::signing::tx_builder_ringct_t`

Declared in `multisig_tx_builder_ringct.h:55-115`. Constructs RingCT transactions for multisig signing. Marked `final`.

| Field | Type | Description |
|-------|------|-------------|
| `initialized` | `bool` | Whether `init()` has been called successfully |
| `reconstruction` | `bool` | Whether the builder is reconstructing an already-created tx |
| `cached_w` | `rct::keyV` | Cached: `mu_P * (local_keys + sender_receiver_secret) + mu_C * (commitment_to_zero_secret)` per input |
| `CLSAG_contexts` | `vector<CLSAG_context_t>` | One CLSAG context per transaction input |

**Constant:**
- `kAlphaComponents = 2` -- Number of parallel signing nonces per signer (as in MuSig2/FROST).

## Public API

### Account Lifecycle (multisig_account)

#### Constructor: `multisig_account(base_privkey, base_common_privkey)`
- **Signature:** `multisig_account(const crypto::secret_key&, const crypto::secret_key&)`
- Initializes a fresh account from the participant's base spend and view private keys.
- Derives `m_base_pubkey` from `m_base_privkey`.
- Generates the first-round KEX message containing the `base_common_privkey` (for constructing the shared view key).
- Sets `m_multisig_pubkey` and `m_common_pubkey` to the identity point.
- **Preconditions:** `base_privkey` must be a valid Ed25519 scalar.
- **Error:** Throws if public key derivation fails.

#### Constructor: `multisig_account(threshold, signers, ...full state...)`
- **Signature:** `multisig_account(uint32_t, vector<public_key>, secret_key, secret_key, vector<secret_key>, secret_key, public_key, public_key, uint32_t, multisig_keyset_map_memsafe_t, string)`
- Reconstructs from persisted full account state. Not recommended for normal use.
- Validates `kex_rounds_complete > 0`, re-derives `m_base_pubkey`, calls `set_multisig_config()`.
- If main KEX rounds are done, regenerates the post-KEX verification message.
- **Preconditions:** `kex_rounds_complete > 0`; `kex_rounds_complete <= kex_rounds_required + 1`.
- **Error:** Throws on invalid rounds count, invalid signers, or failed pubkey derivation.

### Account Status Queries

#### `account_is_active() -> bool`
Returns `true` when `m_kex_rounds_complete > 0` (at least one KEX round initialized).

#### `main_kex_rounds_done() -> bool`
Returns `true` when the account is active and `m_kex_rounds_complete >= multisig_kex_rounds_required(N, M)`.

#### `multisig_is_ready() -> bool`
Returns `true` when main KEX is done and `m_kex_rounds_complete >= multisig_setup_rounds_required(N, M)` (i.e., all rounds including post-KEX verification are complete).

### Key Exchange Operations

#### `initialize_kex(threshold, signers, expanded_msgs_rnd1)`
- **Signature:** `void initialize_kex(uint32_t, vector<public_key>, const vector<multisig_kex_msg>&)`
- Begins multisig key exchange. Sets threshold and signers, then processes round-1 messages.
- Uses transactional semantics: the account is only mutated on success.
- **Preconditions:** Account must not already be active (`!account_is_active()`).
- **Error:** Throws if already initialized, or if message processing fails.

#### `kex_update(expanded_msgs, force_update_use_with_caution)`
- **Signature:** `void kex_update(const vector<multisig_kex_msg>&, bool = false)`
- Processes messages for the current in-progress KEX round and advances to the next round.
- Uses transactional semantics.
- **Preconditions:** Account must be active and not yet ready.
- `force_update_use_with_caution = true`: Allows updating with an incomplete signer set.
  - For post-KEX verification: requires only 1 input message (risky -- assumes honest threshold subgroup).
  - For intermediate rounds: requires messages from `N - 1 - (round - 1)` other signers.
- **Error:** Throws if not initialized, already complete, or message validation fails.

#### `get_multisig_kex_round_booster(threshold, num_signers, expanded_msgs) -> multisig_kex_msg`
- **Signature:** `multisig_kex_msg get_multisig_kex_round_booster(uint32_t, uint32_t, const vector<multisig_kex_msg>&) const`
- Creates a KEX message for the round *after* the current in-progress round, to "boost" another participant who may be behind.
- Useful for scenarios like 2-of-3 escrowed purchasing where vendor+arbitrator can prepare messages that allow the buyer to complete KEX in fewer interactive rounds.
- **Preconditions:** Account must not have completed all intermediate KEX rounds (there must be a round to boost). At least one input message required.
- **Error:** Throws if boosting is not possible or messages are invalid.

### Getters

All const, returning references to internal state:

| Method | Return Type | Description |
|--------|-------------|-------------|
| `get_threshold()` | `uint32_t` | M |
| `get_signers()` | `const vector<public_key>&` | Sorted signer base pubkeys |
| `get_base_privkey()` | `const secret_key&` | Local base private key |
| `get_base_pubkey()` | `const public_key&` | Local base public key |
| `get_base_common_privkey()` | `const secret_key&` | Local base common (view) private key |
| `get_multisig_privkeys()` | `const vector<secret_key>&` | Local private key shares of the multisig spend key |
| `get_common_privkey()` | `const secret_key&` | Aggregate common (view) private key |
| `get_multisig_pubkey()` | `const public_key&` | Final multisig spend public key |
| `get_common_pubkey()` | `const public_key&` | Common (view) public key |
| `get_kex_rounds_complete()` | `uint32_t` | Number of completed KEX rounds |
| `get_kex_keys_to_origins_map()` | `const multisig_keyset_map_memsafe_t&` | Current round's key-to-origins mapping |
| `get_next_kex_round_msg()` | `const string&` | Serialized message for the next KEX round |

### Free Functions (Round Calculations)

#### `multisig_kex_rounds_required(num_signers, threshold) -> uint32_t`
- Returns `N - M + 1` (number of main KEX rounds, not counting post-KEX verification).
- **Preconditions:** `num_signers >= threshold >= 1`.
- **Error:** Throws if preconditions violated.

#### `multisig_setup_rounds_required(num_signers, threshold) -> uint32_t`
- Returns `multisig_kex_rounds_required() + 1` (includes the post-KEX verification round).

### KEX Message API (multisig_kex_msg)

#### Constructor: `multisig_kex_msg(round, signing_privkey, msg_pubkeys, msg_privkey)`
- Constructs and signs a new KEX message from components.
- Round 1: requires a valid `msg_privkey`; `msg_pubkeys` is ignored.
- Round > 1: `msg_pubkeys` must all be valid (non-null, in prime subgroup); `msg_privkey` is ignored.
- **Error:** Throws on invalid round (must be > 0), invalid keys, or subgroup check failure.

#### Constructor: `multisig_kex_msg(msg_string)`
- Parses a serialized KEX message string, deserializes, and validates the signature.
- Rejects deprecated V1 messages (`MultisigV1`, `MultisigxV1`).
- **Error:** Throws on parse failure, invalid keys, subgroup check failure, or signature verification failure.

#### Getters
- `get_msg() -> const string&` -- Full serialized message.
- `get_round() -> uint32_t` -- KEX round number.
- `get_msg_pubkeys() -> const vector<public_key>&` -- Pubkeys in the message.
- `get_msg_privkey() -> const secret_key&` -- Private key (round 1 only).
- `get_signing_pubkey() -> const public_key&` -- The signer's base pubkey.

### Key Image Helpers (multisig.h)

#### `get_multisig_blinded_secret_key(key) -> secret_key`
- Converts a private key into a blinded multisig key: `H(key || "Multisig\0...")`.
- Used to derive multisig private keys from base spend keys and DH derivations.
- **Preconditions:** `key != null_skey`.
- **Error:** Throws on null key.

#### `generate_multisig_key_image(keys, multisig_key_index, out_key, ki) -> bool`
- Generates a key image component: `ki = multisig_privkey[index] * H_p(out_key)`.
- **Returns:** `false` if `multisig_key_index` is out of bounds.

#### `generate_multisig_LR(pkey, k, L, R)`
- Computes `L = k*G` and `R = k*H_p(pkey)` (proof components for a multisig key image).

#### `generate_multisig_composite_key_image(keys, subaddresses, out_key, tx_public_key, additional_tx_keys, real_output_index, pkis, ki) -> bool`
- Assembles a complete key image from the local partial key image and other participants' key image components (`pkis`).
- The local partial KI includes the view key component, subaddress component, and all local multisig privkey components.
- Deduplicates components using a hash set before adding.
- **Returns:** `false` if `generate_key_image_helper` fails.

### Transaction Building (tx_builder_ringct_t)

#### `init(...) -> bool`
- **Signature:** `bool init(account_keys, extra, subaddr_account, subaddr_minor_indices, sources, destinations, change, rct_config, use_rct, reconstruction, tx_secret_key, tx_aux_secret_keys, tx_secret_key_entropy, unsigned_tx)`
- Prepares an unsigned multisig RingCT transaction.
- Sorts sources by key image, generates deterministic tx secret keys from an entropy seed, shuffles destinations (unless reconstructing), creates output one-time addresses and amount commitments, builds range proofs (Bulletproofs or Bulletproofs+), and initializes CLSAG contexts for each input.
- For new transactions: generates fresh random `tx_secret_key_entropy`.
- For reconstructions: verifies that the deterministically regenerated tx secret keys match the provided ones, verifies balance, and revalidates range proofs.
- **Preconditions:** `use_rct == true`; `sources` non-empty; `rct_config.bp_version` is 3 (BP) or 4 (BP+); `rct_config.range_proof_type` is `RangeProofPaddedBulletproof`.
- **Returns:** `false` on any validation or computation failure.

#### `first_partial_sign(source, total_alpha_G, total_alpha_H, alpha, c_0, s) -> bool`
- Produces the first partial CLSAG signature for a single input (source index).
- Combines MuSig2-style nonces, computes the CLSAG challenge ring, and creates the initial partial response:
  `s = alpha_combined - c * [mu_P * (local_keys) + mu_C * (commitment_to_zero_secret)]`.
- **Preconditions:** `initialized == true`, `reconstruction == false`, `source < num_sources`.
- **Returns:** `false` if not initialized, is a reconstruction, or source index is out of range.

#### `next_partial_sign(total_alpha_G, total_alpha_H, alpha, x, c_0, s) -> bool`
- Adds an intermediate signer's partial signature contributions for all inputs.
- For each input: combines nonces, recomputes the challenge, and accumulates: `s += alpha_combined - c * mu_P * x`.
- **Preconditions:** `initialized == true`, `reconstruction == true`, all input vectors must match `num_sources`.
- **Returns:** `false` on precondition failure.

#### `finalize_tx(sources, c_0, s, unsigned_tx) -> bool` (static)
- Writes the final challenges (`c1`) and real responses (`s[l]`) into the transaction's CLSAG signatures.
- **Preconditions:** Vector sizes must match.
- **Returns:** `false` on size mismatch or out-of-range real output index.

### CLSAG Context (CLSAG_context_t)

#### `init(P, C_nonzero, C_offset, message, I, D, l, s, num_alpha_components) -> bool`
- Prepares all precomputed tables and hash parameter vectors for a single CLSAG ring signature.
- Computes mu_P and mu_C aggregation coefficients, precomputes `W_i = mu_P*P_i + mu_C*(C_i - C_offset)` and `H_p(P_i)` for all ring members.
- **Preconditions:** `P.size() > 0`; `C_nonzero.size() == P.size()`; `s.size() == P.size()`; `l < P.size()`.
- **Returns:** `false` on invalid inputs.

#### `combine_alpha_and_compute_challenge(total_alpha_G, total_alpha_H, alpha, alpha_combined, c_0, c) -> bool`
- Combines MuSig2 nonces: computes factor `b = H(b_params)`, then `alpha_combined = sum(b^i * alpha[i])`.
- Computes the CLSAG challenge ring starting from the combined nonce at index `l`, traversing all ring members to find `c_0` and the final challenge `c` that the signers must respond to.
- **Preconditions:** `initialized == true`; all alpha vectors must have `num_alpha_components` elements.
- **Returns:** `false` if not initialized or size mismatch.

#### `get_mu(mu_P, mu_C) -> bool`
- Returns the CLSAG aggregation coefficients mu_P and mu_C.
- **Preconditions:** `initialized == true`.
- **Returns:** `false` if not initialized.

## Internal Logic

### M-of-N Key Exchange Protocol

The KEX protocol establishes a shared multisig spend key through `N - M + 1` main rounds plus one post-KEX verification round (total: `N - M + 2` setup rounds). The number of rounds equals `multisig_kex_rounds_required(N, M) = N - M + 1`.

**Key exchange round flow:**

1. **Round 1 (initialization):**
   - Each participant creates a `multisig_kex_msg` containing their `base_common_privkey` (for the shared view key) signed by their `base_privkey`.
   - On processing round-1 messages (`initialize_kex_update`):
     - All participants' base common privkeys are collected and hashed together (sorted first) to form the aggregate `common_privkey` = `H(sorted_base_common_privkeys)`. This becomes the shared multisig view key.
     - For N-of-N (`kex_rounds_required == 1`), the base privkey is directly used as the multisig privkey share.
   - Round-1 messages are processed via `multisig_kex_process_round_msgs` which:
     - Sanitizes pubkeys (the signing pubkeys of each message are treated as "msg pubkeys" in round 1).
     - Performs DH derivations: `D = 8 * base_privkey * other_pubkey` for each other signer's base pubkey.
     - Maps each derivation to the set of signers who share that secret.

2. **Intermediate rounds (2 through N-M):**
   - Each participant sends DH derivations from the previous round to other participants.
   - On receiving messages:
     - Pubkeys are sanitized: duplicates removed, keys in the local "exclude" set filtered out, origins tracked.
     - Each pubkey must be recommended by exactly `round_num` other signers.
     - Each origin must recommend exactly `(N-2) choose (round-1)` pubkeys.
     - New DH derivations are computed: `D = 8 * base_privkey * received_pubkey` for each received key.

3. **Final main round (N-M+1):**
   - When `m_kex_rounds_complete + 2 == kex_rounds_required`:
     - DH derivations are converted to private key shares: `multisig_privkey = H(derivation)` via `get_multisig_blinded_secret_key`.
     - The corresponding public keys `multisig_privkey * G` are sent in the next message.
   - When `m_kex_rounds_complete + 1 == kex_rounds_required`:
     - Received public keys from other signers are collected as key shares for the final aggregate key.
     - Key aggregation with MRL-0009 coefficients: for each component key K, compute `coeff = H(K, sorted_all_keys, domain_sep)`, then `aggregate = sum(coeff_i * K_i)`.
     - Local multisig privkeys are multiplied by their respective aggregation coefficients.
     - The result is `m_multisig_pubkey`.

4. **Post-KEX verification round:**
   - Each signer sends a message containing `[multisig_pubkey, common_pubkey]`.
   - On processing: verifies that all (or a sufficient subset of) other signers computed the same multisig pubkey and common pubkey.
   - On success: `multisig_is_ready()` returns true.

**Key exchange boosting:** The `get_multisig_kex_round_booster` method allows a participant to generate a message for round R+1 from round R messages of other participants, without completing round R themselves. This enables scenarios where some participants "jumpstart" others.

**Transactional update model:** Both `initialize_kex` and `kex_update` copy the account to a temporary, perform the update on the copy, and only replace the original on success. This prevents corruption from partial failures.

### Key Image Generation

For outputs owned by a multisig group, the full key image `KI = k_total * H_p(output_onetime_address)` cannot be computed by any single participant because no one holds `k_total`. Instead:

1. Each participant computes partial key images: one for each of their multisig private keys: `pki_m = multisig_privkeys[m] * H_p(out_key)`.
2. `generate_multisig_composite_key_image` assembles the complete KI:
   - Starts with the local full partial KI from `generate_key_image_helper` (includes view key, subaddress, and all local multisig key components).
   - Adds in other participants' partial KI components (`pkis`), deduplicating via a hash set.
   - The result is the true key image if sufficient components are provided.

### Multisig CLSAG Signing

The signing protocol produces CLSAG ring signatures compatible with Monero's on-chain format, using a MuSig2-style two-round nonce protocol.

**Constants:**
- `kAlphaComponents = 2` -- Each signer contributes 2 parallel nonces per CLSAG.

**Signing flow:**

1. **Transaction initialization (`init`):**
   - Sources (inputs) are sorted by key image for determinism.
   - A deterministic tx secret key seed is computed: `seed = H(H("domain_sep"), entropy, {key_images})`.
   - Tx secret keys are derived as a hash chain from the seed.
   - Output one-time addresses, amount keys, and view tags (for BP+ v4) are computed.
   - For each input, a CLSAG context is initialized with the ring, key images, and fake responses.
   - `cached_w[i] = mu_P * input_secret_key[i] + mu_C * commitment_to_zero_secret[i]` is computed for the first signer.

2. **First partial signature (`first_partial_sign`):**
   - For a given input, the CLSAG context combines MuSig2 nonces:
     - `b = H(b_params)` where `b_params` includes the ring, message, aggregate nonces, key image, auxiliary key image, fake responses, and indices.
     - `alpha_combined = sum(b^i * alpha[i])` for `i = 0..1`.
     - Combined public nonce: `L = sum(b^i * total_alpha_G[i])`, `R = sum(b^i * total_alpha_H[i])`.
   - The CLSAG challenge ring is traversed from index `l+1` around to `l` to compute `c_0` and the final challenge `c`.
   - Partial response: `s = alpha_combined - c * cached_w[source]`.

3. **Subsequent partial signatures (`next_partial_sign`):**
   - Reconstructing signers use the same CLSAG context to recompute the challenge.
   - Each adds their contribution: `s += alpha_combined_local - c * mu_P * x` where `x` is the signer's private input key component.

4. **Finalization (`finalize_tx`):**
   - The accumulated challenge `c_0` and response `s` for each input are written into `CLSAGs[i].c1` and `CLSAGs[i].s[real_output]` of the transaction.

**Range proofs:** The builder supports Bulletproofs (bp_version=3) and Bulletproofs+ (bp_version=4). View tags are used when bp_version >= 4.

**Deterministic tx keys:** The tx secret key derivation from `(entropy, key_images)` ensures that two different multisig transactions will never produce the same output one-time addresses (which would burn funds). During reconstruction, the deterministic keys are verified to match.

## Dependencies

### This module depends on:

| Dependency | Usage |
|------------|-------|
| `crypto/crypto.h` | Ed25519 keypair operations, key image generation, signature creation/verification, random generation |
| `crypto/crypto-ops.h` | Low-level scalar and group operations (`sc_mul`, `sc_add`, `sc_sub`, `sc_muladd`, `sc_mulsub`, `sc_check`) |
| `ringct/rctOps.h` | RingCT operations: `scalarmultKey`, `addKeys`, `addKeys3`, `hash_to_scalar`, `hash_to_p3`, `precomp`, identity/zero constants, INV_EIGHT/EIGHT |
| `ringct/rctTypes.h` | Types: `rct::key`, `rct::keyV`, `rct::keyM`, `rct::ctkey`, `rct::ctkeyV`, `rct::rctSig`, `rct::geDsmp`, `rct::RCTConfig` |
| `ringct/rctSigs.h` | `get_pre_mlsag_hash`, `genCommitmentMask` |
| `ringct/bulletproofs.h` | `bulletproof_PROVE`, `bulletproof_VERIFY` |
| `ringct/bulletproofs_plus.h` | `bulletproof_plus_PROVE`, `bulletproof_plus_VERIFY` |
| `cryptonote_basic/cryptonote_format_utils.h` | `generate_key_image_helper`, `get_transaction_prefix_hash`, tx extra manipulation, subaddress utilities |
| `cryptonote_basic/account.h` | `account_keys` struct |
| `cryptonote_core/cryptonote_tx_utils.h` | Transaction utility functions |
| `cryptonote_config.h` | Constants: `MULTISIG_MAX_SIGNERS` (16), hash domain separators for multisig, CLSAG, and tx key derivation |
| `common/base58.h` | Base58 encoding/decoding for KEX messages |
| `serialization/` | Binary archive serialization for KEX messages |
| `device/device.hpp` | Hardware device abstraction for key operations |
| `boost::math::binomial_coefficient` | Combinatorial calculations for validating KEX round message counts |

### Modules that depend on this:

| Consumer | Usage |
|----------|-------|
| `src/wallet/wallet2.h`, `src/wallet/wallet2.cpp` | Primary consumer: manages multisig wallet lifecycle, calls KEX operations, builds and signs multisig transactions |
| `src/wallet/api/wallet.cpp` | Wallet API layer: exposes multisig account state to the public C++ API |
| `src/wallet/wallet_rpc_server.cpp` | RPC interface: handles multisig-related RPC calls |
| `src/simplewallet/simplewallet.cpp` | CLI wallet: implements interactive multisig commands |
| `src/gen_multisig/` | Standalone multisig key generation tool |

## Known Issues

The following TODO/FIXME/HACK/XXX comments were found in the codebase:

1. **`src/multisig/multisig_account.h:61`** -- `TODO: encapsulates key preparation for aggregation-style signing` -- The account class does not yet support aggregation-style signing (as opposed to round-robin signing).

2. **`src/multisig/multisig_account.h:263`** -- `TODO: also record which other signers have these privkeys, to enable aggregation signing (instead of round-robin)` -- The `m_multisig_privkeys` vector does not track which other signers share each private key, which would be needed for aggregation-based (non-round-robin) signing.

3. **`src/multisig/multisig_account_kex_impl.cpp:118`** -- `TODO: need a constant-time operator< for sorting secret keys` -- The `make_multisig_common_privkey` function sorts secret keys using `memcmp`, which is not constant-time and could leak timing information about secret key values.

4. **`src/multisig/multisig_account_kex_impl.cpp:480`** -- `TODO: move to a 'math' library, with unit tests` -- The `n_choose_k_f` lambda (binomial coefficient wrapper using `boost::math::binomial_coefficient<double>` with float-to-int conversion) is defined inline and should be extracted to a shared math utility with proper tests.

5. **`src/multisig/multisig_account_kex_impl.cpp:788`** -- `TODO: record [pre-aggregation pubkeys : origins] map for aggregation-style signing` -- During the final KEX round, the pre-aggregation pubkey-to-origins mapping is discarded after computing the aggregate key, but would be needed for aggregation-style signing.
