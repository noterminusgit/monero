# Multisig Wallet Operations

## Overview

The `wallet2` class implements M-of-N multisig wallet functionality by integrating with the standalone `multisig` module for cryptographic key exchange, key image generation, and transaction signing. A multisig wallet splits the spend secret key among N participants such that at least M must collaborate to authorize a transaction. The wallet layer orchestrates the multi-round key exchange setup protocol, manages per-output signing nonces and partial key images, serializes/deserializes multisig data for inter-participant communication, and coordinates the round-robin CLSAG partial signing process using a MuSig2-style nonce aggregation scheme. The maximum number of signers is capped at 16 (`config::MULTISIG_MAX_SIGNERS`) to prevent combinatorial explosion in DH-based key exchange.

## Key Files

| File | Description |
|------|-------------|
| `src/wallet/wallet2.h` | Declares multisig data structures (`multisig_info`, `multisig_sig`, `multisig_tx_set`) and all multisig wallet methods. |
| `src/wallet/wallet2.cpp` | Implements wallet-level multisig setup, export/import, signing, and nonce management. |
| `src/multisig/multisig_account.h` | Defines `multisig_account` class encapsulating key exchange state, and `multisig_account_status` struct. |
| `src/multisig/multisig_account.cpp` | Implements `multisig_account` constructors, status queries, `initialize_kex()`, `kex_update()`, and round-count helpers. |
| `src/multisig/multisig_account_kex_impl.cpp` | Implements the core key exchange logic: DH derivation rounds, key aggregation, and the post-kex verification round. |
| `src/multisig/multisig_kex_msg.h` | Defines `multisig_kex_msg` class for constructing, signing, parsing, and validating key exchange messages. |
| `src/multisig/multisig_kex_msg.cpp` | Implements kex message serialization (base58-encoded, cryptographically signed). |
| `src/multisig/multisig_kex_msg_serialization.h` | Defines serializable structs for round-1 and general kex messages. |
| `src/multisig/multisig.h` | Declares low-level primitives: `get_multisig_blinded_secret_key()`, `generate_multisig_key_image()`, `generate_multisig_LR()`, `generate_multisig_composite_key_image()`. |
| `src/multisig/multisig.cpp` | Implements the low-level multisig primitives. |
| `src/multisig/multisig_tx_builder_ringct.h` | Defines `tx_builder_ringct_t` for constructing and partially signing RingCT multisig transactions. |
| `src/multisig/multisig_tx_builder_ringct.cpp` | Implements multisig transaction construction, partial signing, and finalization. |
| `src/multisig/multisig_clsag_context.h` | Defines `CLSAG_context_t` for multisig CLSAG challenge computation with MuSig2-style nonce combination. |
| `src/multisig/multisig_clsag_context.cpp` | Implements CLSAG context initialization and `combine_alpha_and_compute_challenge()`. |
| `src/multisig/CMakeLists.txt` | Build definition; the `multisig` library links against `ringct`, `cryptonote_basic`, `cryptonote_core`, `common`, `cncrypto`. |
| `src/cryptonote_config.h` | Defines `MULTISIG_MAX_SIGNERS = 16`. |

## Data Structures

### `wallet2::multisig_info` (wallet2.h:296)

Per-output, per-participant multisig metadata exchanged during `export_multisig()`/`import_multisig()`.

```cpp
struct multisig_info {
    struct LR {
        rct::key m_L;  // Public nonce component: L = k * G
        rct::key m_R;  // Public nonce component: R = k * Hp(output_pubkey)
    };
    crypto::public_key m_signer;                    // Base pubkey of the participant who generated this info
    std::vector<LR> m_LR;                           // Signing nonce pairs (one per tx attempt slot)
    std::vector<crypto::key_image> m_partial_key_images; // Partial key images, one per multisig privkey held
};
```

### `wallet2::multisig_sig` (wallet2.h:609)

Represents one signing attempt for a particular subgroup of threshold signers within a pending multisig transaction.

```cpp
struct multisig_sig {
    rct::rctSig sigs;                                    // The RingCT signature structure (unused/legacy field)
    std::unordered_set<crypto::public_key> ignore;       // Signers excluded from this attempt (identifies the signer subgroup)
    std::unordered_set<rct::key> used_L;                 // Public L nonces consumed by this attempt (coordinates nonce usage)
    std::unordered_set<crypto::public_key> signing_keys; // Multisig signing pubkeys that have contributed partial sigs
    rct::multisig_out msout;                             // Multisig output key data

    rct::keyM total_alpha_G;  // [num_inputs][kAlphaComponents] summed public nonces (G component)
    rct::keyM total_alpha_H;  // [num_inputs][kAlphaComponents] summed public nonces (H component)
    rct::keyV c_0;            // [num_inputs] CLSAG challenges (c_0 stored in proof)
    rct::keyV s;              // [num_inputs] aggregated signature responses
};
```

### `wallet2::multisig_tx_set` (wallet2.h:728)

Serializable container for multisig transaction proposals exchanged between participants.

```cpp
struct multisig_tx_set {
    std::vector<pending_tx> m_ptx;                    // Pending transactions (each contains multisig_sigs)
    std::unordered_set<crypto::public_key> m_signers; // Base pubkeys of participants who have signed
};
```

### `wallet2::transfer_details` multisig fields (wallet2.h:352-353)

Each wallet output (transfer) stores multisig-specific data:

```cpp
std::vector<rct::key> m_multisig_k;              // Private signing nonces for this output (wiped after use)
std::vector<multisig_info> m_multisig_info;      // One entry per other participant (from import_multisig)
bool m_key_image_partial;                        // True if key image is incomplete (not all participants' info imported)
```

### `wallet2::pending_tx` multisig fields (wallet2.h:652-653)

```cpp
std::vector<multisig_sig> multisig_sigs;          // Signing attempts for different signer subgroups
crypto::secret_key multisig_tx_key_entropy;       // Entropy for deterministic tx key derivation in multisig
```

### Wallet-level multisig state (wallet2.h:1962-1967)

```cpp
bool m_multisig;                                  // Whether this wallet is multisig
uint32_t m_multisig_threshold;                    // M in M-of-N
std::vector<crypto::public_key> m_multisig_signers; // Base pubkeys of all N participants (sorted)
uint32_t m_multisig_rounds_passed;                // Number of completed key exchange rounds
std::vector<crypto::public_key> m_multisig_derivations; // Intermediate kex derivations (empty when kex complete)
bool m_enable_multisig;                           // Feature flag (wallet2.h:2017)
```

### `multisig::multisig_account_status` (multisig_account.h:43)

```cpp
struct multisig_account_status {
    bool multisig_is_active;    // Account has been initialized via make_multisig
    bool kex_is_done;           // Main key exchange rounds are complete
    bool is_ready;              // Post-kex verification done; wallet can sign transactions
    uint32_t threshold;         // M
    uint32_t total;             // N
};
```

### `multisig::multisig_account` (multisig_account.h:94)

Core account class managing key exchange and key storage:

- `m_threshold` / `m_signers`: M-of-N configuration.
- `m_base_privkey` / `m_base_pubkey`: Participant's identity keypair (H(spend_secret_key)).
- `m_base_common_privkey`: View key share (H(view_secret_key)).
- `m_multisig_privkeys`: The account's private key shares of the multisig spend key.
- `m_common_privkey` / `m_common_pubkey`: Shared view key (known to all participants).
- `m_multisig_pubkey`: The aggregate multisig spend public key.
- `m_kex_rounds_complete`: Progress counter for key exchange.
- `m_kex_keys_to_origins_map`: Maps intermediate kex pubkeys to their originating signers.
- `m_next_round_kex_message`: The serialized message to send to other participants.

### `multisig::multisig_kex_msg` (multisig_kex_msg.h:50)

Signed, serialized key exchange message with structure:
```
msg = versioning-domain-sep | b58(msg_content | crypto_sig[signing_privkey](msg_to_sign))
msg_content = kex_round | signing_pubkey | expand(msg_pubkeys) | OPTIONAL msg_privkey
```
Round 1 messages include a private key (the `base_common_privkey` share for constructing the aggregate view key).

### `multisig::signing::CLSAG_context_t` (multisig_clsag_context.h:47)

Manages the CLSAG challenge computation for multisig, implementing MuSig2-style nonce combination:
- Computes nonce combination factor `b = H(...)` from ring members, nonces, key images, and other parameters.
- Combines local signer's private nonces as `alpha_combined = sum_i(b^i * alpha[i])`.
- Produces the CLSAG challenge `c_0` and the final challenge `c` for partial signature generation.
- Uses `kAlphaComponents = 2` parallel nonces per signer (matching MuSig2/FROST).

### `multisig::signing::tx_builder_ringct_t` (multisig_tx_builder_ringct.h:55)

Transaction builder for multisig RingCT transactions:
- `init()`: Prepares an unsigned transaction, computing outputs, range proofs, and CLSAG contexts.
- `first_partial_sign()`: The tx proposer creates the initial partial signature for one input.
- `next_partial_sign()`: Co-signers add their partial signatures for all inputs.
- `finalize_tx()` (static): Assembles the final CLSAG signatures from aggregated partial components.

## Public API

### Setup Methods

#### `get_multisig_first_kex_msg()`

```cpp
std::string get_multisig_first_kex_msg() const;
```

Returns the initial key exchange message to send to other participants before `make_multisig()` is called. Creates an uninitialized `multisig_account` from the wallet's blinded spend and view keys, then returns its round-1 kex message. This message contains the participant's base pubkey and their `base_common_privkey` share.

**Preconditions:** Wallet must not yet be multisig. Wallet keys must be available.

#### `make_multisig()`

```cpp
std::string make_multisig(const epee::wipeable_string &password,
    const std::vector<std::string> &kex_messages,
    const std::uint32_t threshold);
```

Converts a normal wallet into a multisig wallet by performing the first round of key exchange.

**Parameters:**
- `password`: Wallet password (needed to decrypt/re-encrypt keys).
- `kex_messages`: Round-1 messages from all N participants (including self).
- `threshold`: The M in M-of-N.

**Behavior:**
1. Decrypts account keys.
2. Creates an uninitialized `multisig_account` using `get_uninitialized_multisig_account()` (blinding the spend and view keys via `get_multisig_blinded_secret_key()`).
3. Validates input messages: all must be round 1, no duplicate signers, self must be present.
4. Calls `multisig_account::initialize_kex(threshold, signers, expanded_msgs)`.
5. Saves original keys for MMS (Multisig Messaging System) compatibility.
6. Clears wallet caches, updates account base with multisig keys.
7. Sets `m_multisig = true`, records threshold, signers, derivations.
8. Saves wallet file if applicable.
9. Returns the next kex round message (empty string if 1-of-N, i.e., kex completed in one round).

**Preconditions:** Wallet must not already be multisig. All N participants' round-1 messages required.

**Returns:** Empty string if key exchange is complete (N-of-N or 1-of-N); otherwise the next round's kex message to distribute.

#### `exchange_multisig_keys()`

```cpp
std::string exchange_multisig_keys(const epee::wipeable_string &password,
    const std::vector<std::string> &kex_messages,
    const bool force_update_use_with_caution = false);
```

Advances the key exchange by one round. Called repeatedly until the return value is empty (kex complete).

**Parameters:**
- `password`: Wallet password.
- `kex_messages`: Messages from other participants for the current round. May be empty if main kex rounds are done (returns post-kex verification message).
- `force_update_use_with_caution`: If true, allows proceeding with fewer than N-1 messages. Dangerous: malicious messages could produce an invalid account.

**Behavior:**
1. Reconstructs the `multisig_account` from wallet state.
2. If `kex_messages` is empty and main kex is done, returns the post-kex verification message.
3. Calls `multisig_account::kex_update()`.
4. Updates wallet spend public key, account base, derivations, and rounds-passed counter.
5. When `multisig_is_ready()` becomes true, stores keys, resets subaddresses.

**Preconditions:** Wallet must be multisig (`m_multisig == true`). Key exchange must not already be complete (`!is_ready`).

**Returns:** Empty string when fully complete; otherwise the next round's kex message.

#### `get_multisig_key_exchange_booster()`

```cpp
std::string get_multisig_key_exchange_booster(const epee::wipeable_string &password,
    const std::vector<std::string> &kex_messages,
    const std::uint32_t threshold,
    const std::uint32_t num_signers);
```

Produces a kex message for the round *after* the current in-progress round, allowing another participant to skip ahead. Useful in scenarios like 2-of-3 escrowed purchasing where vendor and arbitrator can boost the buyer's setup.

**Preconditions:** Must not be fully ready. If multisig is active, threshold/num_signers must match wallet settings.

**Warning:** If the wallet is uninitialized and `num_signers - threshold > 1`, this can leak private keys.

**Returns:** A booster kex message for round `num_completed_rounds + 2`.

### Status Methods

#### `get_multisig_status()`

```cpp
multisig::multisig_account_status get_multisig_status() const;
```

Returns the current multisig status: whether active, kex completion state, threshold, total signers. Checks `m_multisig_rounds_passed` against `multisig_kex_rounds_required()` and `multisig_setup_rounds_required()`.

#### `has_multisig_partial_key_images()`

```cpp
bool has_multisig_partial_key_images() const;
```

Returns true if any transfer has `m_key_image_partial == true`, indicating that `import_multisig()` needs to be called to complete key images.

#### `get_multisig_seed()`

```cpp
bool get_multisig_seed(epee::wipeable_string& seed, const epee::wipeable_string &passphrase = std::string()) const;
```

Exports the multisig wallet seed as a hex string containing threshold, total, spend key, view key, all multisig private keys, and all signer pubkeys. Optionally encrypted with a passphrase using `cn_slow_hash`.

**Preconditions:** Wallet must be multisig and fully ready (post-kex verification complete).

#### `is_multisig_enabled()` / `enable_multisig()`

```cpp
bool is_multisig_enabled() const;
void enable_multisig(bool enable);
```

Feature flag controlling whether multisig operations are allowed.

### Info Export/Import Methods

#### `export_multisig()`

```cpp
cryptonote::blobdata export_multisig();
```

Exports multisig info (partial key images and signing nonces) for all wallet outputs.

**Behavior for each output:**
1. Wipes any previously generated nonces (`m_multisig_k`).
2. Generates partial key images for each local multisig private key.
3. Computes the number of signing nonce tuples needed: `C(N-1, N-M)` combinations times `kAlphaComponents` (2) nonces each.
4. Generates random nonces and corresponding L/R pairs.
5. Records nonces in `transfer_details::m_multisig_k` (private) and L/R pairs in the export blob (public).

**Serialization format:** `"Monero multisig export\001"` + encrypted(header + binary_archive(info)), where header = spend_pubkey + view_pubkey + signer_pubkey. Encrypted with the view secret key.

**Critical:** Calling this function invalidates all previously exported nonces and any in-progress signing attempts.

#### `import_multisig()`

```cpp
size_t import_multisig(std::vector<cryptonote::blobdata> blobs);
```

Imports multisig info blobs from other participants.

**Parameters:** `blobs` - Export data from `threshold - 1` to `N - 1` other participants.

**Behavior:**
1. Validates magic header, decrypts with view secret key, verifies spend/view pubkeys match.
2. Skips self-signed data and duplicates.
3. Validates all L, R, and partial key image values are in the main subgroup.
4. For each output, combines imported partial key images with local ones via `generate_multisig_composite_key_image()` to produce complete key images.
5. Triggers a blockchain rescan (`handle_reorg()` + `refresh()`) starting from the earliest block containing a partial key image.

**Returns:** Number of outputs processed.

**Preconditions:** Wallet must be multisig. Number of import sources must satisfy `threshold - 1 <= count <= N - 1`.

### Signing Methods

#### `sign_multisig_tx()`

```cpp
bool sign_multisig_tx(multisig_tx_set &exported_txs, std::vector<crypto::hash> &txids);
```

Adds the local signer's partial signatures to a multisig transaction set.

**Behavior for each pending transaction:**
1. Validates the tx set is not empty, not already signed by this signer, and not already fully signed.
2. Rejects frozen outputs.
3. Reconstructs the transaction via `tx_builder_ringct_t::init()` with `reconstruction = true`.
4. For each signing attempt (`multisig_sig`) that includes this signer (not in `ignore` set):
   a. Retrieves local nonces for each input via `get_multisig_k()` (nonces are consumed and zeroed -- critical one-time use).
   b. Aggregates local multisig key shares that haven't been used by prior signers (round-robin approach).
   c. Calls `tx_builder_ringct_t::next_partial_sign()` with total nonces, local nonces, and aggregated signing key.
5. If this is the final (threshold-reaching) signature:
   a. Finds the signing attempt where no ignored signer overlaps with the set of signers.
   b. Calls `tx_builder_ringct_t::finalize_tx()` to produce the completed CLSAG signatures.
   c. Records the tx hash in `txids`.
6. Wipes all remaining unused nonces for the transaction's inputs.
7. Adds local signer to `exported_txs.m_signers`.

**Preconditions:** Wallet must be multisig and ready. `export_multisig()`/`import_multisig()` must have been performed. Transaction must not already be signed by this participant.

#### `sign_multisig_tx_to_file()`

```cpp
bool sign_multisig_tx_to_file(multisig_tx_set &exported_txs, const std::string &filename, std::vector<crypto::hash> &txids);
```

Calls `sign_multisig_tx()` then saves the result to a file via `save_multisig_tx()`.

#### `sign_multisig_tx_from_file()`

```cpp
bool sign_multisig_tx_from_file(const std::string &filename, std::vector<crypto::hash> &txids, std::function<bool(const multisig_tx_set&)> accept_func);
```

Loads a multisig tx set from file, optionally runs an acceptance callback, then signs and saves back.

#### `sign_multisig_participant()`

```cpp
std::string sign_multisig_participant(const std::string& data) const;
```

Signs arbitrary data with the multisig signer key (the blinded spend secret key). Returns `"SigMultisigPkV1"` + base58-encoded signature. Used for authenticating as a multisig participant.

**Preconditions:** Wallet must be multisig.

### Transaction Persistence Methods

#### `save_multisig_tx()` (4 overloads)

```cpp
std::string save_multisig_tx(multisig_tx_set txs);
bool save_multisig_tx(const multisig_tx_set &txs, const std::string &filename);
std::string save_multisig_tx(const std::vector<pending_tx>& ptx_vector);
bool save_multisig_tx(const std::vector<pending_tx>& ptx_vector, const std::string &filename);
```

Serializes a multisig tx set to a binary blob, encrypted with the view secret key, prefixed with `"Monero multisig unsigned tx set\001"`. Before serialization, wipes nonce secrets (`m_multisig_k`, `multisig_kLRki.k`) from the data to avoid leaking private values.

#### `make_multisig_tx_set()`

```cpp
multisig_tx_set make_multisig_tx_set(const std::vector<pending_tx>& ptx_vector) const;
```

Wraps pending transactions into a `multisig_tx_set`, recording the local signer's signing key pubkeys and base pubkey in the set.

#### `parse_multisig_tx_from_str()` / `load_multisig_tx()` / `load_multisig_tx_from_file()`

```cpp
bool parse_multisig_tx_from_str(std::string multisig_tx_st, multisig_tx_set &exported_txs) const;
bool load_multisig_tx(cryptonote::blobdata blob, multisig_tx_set &exported_txs,
    std::function<bool(const multisig_tx_set&)> accept_func = NULL);
bool load_multisig_tx_from_file(const std::string &filename, multisig_tx_set &exported_txs,
    std::function<bool(const multisig_tx_set&)> accept_func = NULL);
```

Deserialize and validate multisig tx sets. `parse_multisig_tx_from_str()` checks magic prefix, decrypts with view key, deserializes, and performs sanity checks on selected_transfers and vin sizes. `load_multisig_tx()` additionally invokes the accept callback and stores tx keys if the tx is fully signed.

### Internal / Helper Methods

#### `get_uninitialized_multisig_account()` (private)

```cpp
void get_uninitialized_multisig_account(multisig::multisig_account &account_out) const;
```

Creates a fresh `multisig_account` using blinded versions of the wallet's spend and view secret keys: `k_base = H(spend_secret_key)`, `k_view = H(view_secret_key)`.

#### `get_reconstructed_multisig_account()` (private)

```cpp
void get_reconstructed_multisig_account(multisig::multisig_account &account_out) const;
```

Reconstructs a `multisig_account` from the wallet's current multisig state (threshold, signers, keys, rounds passed, derivations). Used by `exchange_multisig_keys()` and `get_multisig_key_exchange_booster()`.

#### `get_multisig_signer_public_key()`

```cpp
crypto::public_key get_multisig_signer_public_key() const;
```

Returns the public key corresponding to the wallet's (blinded) spend secret key. This serves as the participant's unique identifier in the multisig group.

#### `get_multisig_signing_public_key()`

```cpp
crypto::public_key get_multisig_signing_public_key(size_t idx) const;
crypto::public_key get_multisig_signing_public_key(const crypto::secret_key &skey) const;
```

Returns the public key for a specific multisig private key share (by index or secret key).

#### `get_multisig_k()`

```cpp
void get_multisig_k(size_t idx, const std::unordered_set<rct::key> &used_L, rct::key &nonce);
```

Retrieves and consumes a private nonce for output `idx` whose public component `L = k*G` matches one in `used_L`. The nonce is zeroed after retrieval to enforce single use. Throws `multisig_export_needed` if no matching nonce is found (meaning `export_multisig()` must be called again).

#### `get_multisig_composite_key_image()`

```cpp
crypto::key_image get_multisig_composite_key_image(size_t n) const;
```

Assembles the complete key image for output `n` by combining the local participant's partial key images with those from other participants (stored in `m_multisig_info`).

#### `get_multisig_kLRki()` / `get_multisig_composite_kLRki()`

```cpp
rct::multisig_kLRki get_multisig_kLRki(size_t n, const rct::key &k) const;
rct::multisig_kLRki get_multisig_composite_kLRki(size_t n,
    const std::unordered_set<crypto::public_key> &ignore_set,
    std::unordered_set<rct::key> &used_L,
    std::unordered_set<rct::key> &new_used_L) const;
```

`get_multisig_kLRki()` computes the L/R nonce pair for a given nonce `k` and output `n`. `get_multisig_composite_kLRki()` generates a fresh random nonce locally, then aggregates L/R components from `threshold - 1` other participants (skipping those in `ignore_set`), tracking used L values to prevent reuse.

#### `update_multisig_rescan_info()`

```cpp
void update_multisig_rescan_info(const std::vector<std::vector<rct::key>> &multisig_k,
    const std::vector<std::vector<tools::wallet2::multisig_info>> &info, size_t n);
```

Updates a single transfer's multisig info and recomputes its composite key image during import.

#### `frozen(const multisig_tx_set&)`

```cpp
bool frozen(const multisig_tx_set& txs) const;
```

Checks whether any output referenced by the multisig tx set is frozen. Uses batched key image lookup (O(M+N)) rather than per-image lookup to avoid quadratic cost.

## Signing Flow

The complete multisig transaction signing workflow involves the following steps:

### Phase 1: Key Exchange Setup

1. **Each participant** calls `get_multisig_first_kex_msg()` to produce their round-1 kex message containing their blinded base pubkey and common (view) privkey share.
2. Messages are distributed to all other participants.
3. **Each participant** calls `make_multisig(password, all_round1_msgs, threshold)`:
   - Initializes the multisig account via `multisig_account::initialize_kex()`.
   - Converts the wallet to multisig mode.
   - Returns the next round's kex message (or empty if complete).
4. For general M-of-N (where `N - M + 1 > 1`), participants exchange `N - M` additional rounds by calling `exchange_multisig_keys()` with each round's messages.
5. The final round is a post-kex verification round where participants confirm they all arrived at the same multisig public key.
6. Total setup rounds: `N - M + 1` kex rounds + 1 initialization = `N - M + 2` message exchanges.

### Phase 2: Sync Multisig Info

7. **Each participant** calls `export_multisig()` to produce their partial key images and signing nonces for all wallet outputs.
8. Export data is distributed to other participants.
9. **Each participant** calls `import_multisig(blobs_from_others)`:
   - Combines partial key images to form complete key images for each output.
   - Stores other participants' public signing nonces (L/R pairs).
   - Triggers a blockchain rescan to update spend status using the now-complete key images.

### Phase 3: Transaction Creation (Proposer)

10. The **proposer** (one participant) creates a transaction using `create_transactions_2()` or similar.
11. Inside `transfer_selected_rct()`, when `m_multisig == true`:
    - A `tx_builder_ringct_t` is initialized to construct the unsigned transaction body (outputs, range proofs, CLSAG contexts).
    - For each possible signing group of size `threshold` (that includes the proposer), a `multisig_sig` attempt is created:
      - Composite nonces are assembled via `get_multisig_composite_kLRki()`, combining the proposer's fresh random nonce with L/R pairs from other signers.
      - `first_partial_sign()` generates the proposer's initial partial CLSAG signature.
    - If threshold is 1, `finalize_tx()` completes the transaction immediately.
12. The proposer packages the result via `save_multisig_tx()` (serializes and encrypts with the shared view key).

### Phase 4: Co-signing

13. The proposer distributes the multisig tx set file to co-signers.
14. Each **co-signer** loads the tx set via `load_multisig_tx_from_file()`.
15. Each co-signer calls `sign_multisig_tx()`:
    - Reconstructs the transaction via `tx_builder_ringct_t::init()` with `reconstruction = true` to validate it.
    - For each signing attempt where the co-signer is not in the `ignore` set:
      - Retrieves local private nonces via `get_multisig_k()` (matched by public L values in `used_L`).
      - Aggregates the co-signer's unused multisig key shares.
      - Calls `next_partial_sign()` to add the partial signature.
    - If this is the M-th signer (reaching threshold), finds the fully-signed attempt and calls `finalize_tx()` to produce the valid transaction.
16. The signed tx set is saved and can be distributed back if more signatures are needed.

### Phase 5: Broadcast

17. Once a signing attempt reaches M signatures, `finalize_tx()` produces a complete transaction.
18. The completed transaction is broadcast via `commit_tx()`.

### Nonce Lifecycle

- Nonces are generated in `export_multisig()` and stored in `transfer_details::m_multisig_k`.
- Public nonce components (L/R) are shared via the export blob.
- During signing, private nonces are consumed exactly once via `get_multisig_k()` and immediately zeroed.
- After `sign_multisig_tx()`, all remaining unused nonces for the involved outputs are wiped.
- To create new transactions with the same outputs, `export_multisig()` must be called again.

## Dependencies

### What multisig depends on

| Dependency | Usage |
|------------|-------|
| `ringct` (rctTypes, rctOps, rctSigs, bulletproofs, bulletproofs_plus) | CLSAG signatures, key operations, range proofs |
| `cryptonote_basic` (account, format_utils) | Account keys, key image generation, transaction primitives |
| `cryptonote_core` (tx_utils) | Transaction construction utilities |
| `common` | Base58 encoding, utility functions |
| `cncrypto` | Core elliptic curve operations, hashing |
| `epee` | Serialization, HTTP, string tools |
| `boost::math` | Binomial coefficients for combination counting in kex |

### What depends on multisig

| Dependent | Usage |
|-----------|-------|
| `wallet2` | Primary consumer; orchestrates all multisig wallet operations |
| `simplewallet` | CLI commands for multisig setup and signing |
| `wallet_rpc_server` | RPC endpoints for multisig operations |
| `mms::message_store` | Multisig Messaging System for coordinating kex and signing between participants |

## Known Issues

The following TODO/FIXME/HACK/XXX/KLUDGE comments were found in multisig-related code:

| File | Line | Comment |
|------|------|---------|
| `src/multisig/multisig_account.h` | 61 | `TODO: encapsulates key preparation for aggregation-style signing` -- aggregation signing (vs round-robin) is not yet implemented. |
| `src/multisig/multisig_account.h` | 263 | `TODO: also record which other signers have these privkeys, to enable aggregation signing (instead of round-robin)` -- the `m_multisig_privkeys` vector does not track which other signers share each key. |
| `src/multisig/multisig_account_kex_impl.cpp` | 118 | `TODO: need a constant-time operator< for sorting secret keys` -- secret key sorting may be vulnerable to timing side channels. |
| `src/multisig/multisig_account_kex_impl.cpp` | 480 | `TODO: move to a 'math' library, with unit tests` -- a combination-counting helper should be refactored out. |
| `src/multisig/multisig_account_kex_impl.cpp` | 788 | `TODO: record [pre-aggregation pubkeys : origins] map for aggregation-style signing` -- same aggregation signing gap as above. |
| `src/wallet/wallet2.cpp` | 6049 | `KLUDGE: early return if there are no kex messages and main kex is complete` -- `exchange_multisig_keys()` returns the post-kex verification message when called with no messages; this behavior would be better as a separate method. |
