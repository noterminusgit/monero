# Known Issues & Technical Debt

A comprehensive inventory of TODO, FIXME, HACK, and XXX comments across the Monero
codebase (`src/` and `contrib/epee/`), along with design concerns identified during review.

---

## Critical / Security

These items affect consensus correctness, funds safety, or key material handling.

- **`src/wallet/wallet2.cpp:6165`** -- DANGER comment: If `num_signers - threshold > 1` but the wallet's future multisig settings will be `num_signers - threshold == 1`, then the booster message WILL leak the future multisig wallet's private keys when the wallet2 multisig wallet is uninitialized.

- **`src/multisig/multisig_account_kex_impl.cpp:118`** -- TODO: need a constant-time `operator<` for sorting secret keys. Currently sorts secret keys with a potentially timing-leaky comparison, which could expose key material via side channels during multisig key exchange.

- **`src/multisig/multisig_kex_msg.cpp:217-219`** -- V1 multisig kex messages are deprecated as "unsafe". The code asserts against their use but the code path still exists.

- **`src/blockchain_db/lmdb/db_lmdb.cpp:1637`** -- FIXME: `mdb_env_close()` is "not yet thread safe!!! Use with care." Closing the LMDB environment while other threads may still be accessing it risks data corruption or crashes.

- **`src/wallet/api/wallet.h:265`** -- TODO: harden password handling in the wallet API. The wallet password is stored as a plain `std::string m_password` member, which may persist in memory. References related discussions in monero-gui #1537, feather #72, monero #8619.

- **`src/wallet/wallet2.cpp:4105-4107`** -- Known subtle race condition with the txpool. Developer note: "a subtle race condition with the txpool... It was pretty subtle IIRC, and so I needed time to think about how to refix it after the move, and I never got to it." (PR #6097)

- **`src/cryptonote_core/blockchain.cpp:1332`** -- FIXME: fork activation height logic "will fail if fork activation heights are subject to voting." Could produce incorrect difficulty targets during contested hard forks.

- **`src/cryptonote_core/blockchain.cpp:3948`** -- FIXME: `get_difficulty_for_next_block` can assert. During block validation the code needs to handle this more gracefully; an assertion failure here could crash the daemon during consensus operations.

- **`src/wallet/wallet2.cpp:1082`** -- Comment notes a possible issue where a gamma-constructed decoy may no longer be feasible to spend since consensus rules changed after the gamma was constructed.

---

## Bugs

Confirmed or suspected incorrect behavior noted by developers in comments.

- **`src/wallet/wallet2.cpp:4003`** -- FIXME: "this isn't right, but simplewallet just logs that we got a block." The block notification callback receives a dummy empty block instead of the actual block data during wallet refresh.

- **`src/cryptonote_core/blockchain.cpp:2229-2250`** -- FIXME: Function appears to want to return false if any transactions belonging to blocks are missing, but the logic may not match intent. Also: `rsp.missed_ids` seems to be for missed blocks, not missed transactions, suggesting a naming/logic mismatch.

- **`src/cryptonote_core/tx_pool.cpp:504`** -- FIXME: Can return early before removal of all key images. A partial key image removal could leave the pool in an inconsistent state if the function returns false mid-operation.

- **`src/cryptonote_core/blockchain.cpp:3966`** -- FIXME: height parameter is not used in PoW check function despite being declared. Either it should be used or removed.

- **`src/rpc/core_rpc_server.cpp:1820`** -- FIXME: `send_stop_signal()` replaced with a workaround because the original "isn't working quite right." The daemon stop mechanism via RPC uses a non-standard code path.

- **`src/cryptonote_core/blockchain.cpp:640`** -- FIXME: HardFork data not properly handled when popping blocks. "Besides the below, popping a block should also remove the last entry in the list of known hard fork versions."

- **`src/checkpoints/checkpoints.cpp:138`** -- FIXME: "is this the desired behavior?" on `is_alternative_block_allowed()`. The checkpoint validation logic for alternative chains may not behave as intended.

- **`src/cryptonote_core/blockchain.cpp:2139`** -- FIXME: "is it even possible for a checkpoint to show up not on the main chain?" Suggests uncertainty about whether a code path that handles this case is reachable or dead code.

---

## Technical Debt

### Cryptonote Core (`src/cryptonote_core/`)

- **`src/cryptonote_core/blockchain.cpp:72-76`** -- TODO: Clean up code; possibly change how outputs are referred to/indexed in blockchain and wallets. Long-standing refactoring request.

- **`src/cryptonote_core/blockchain.cpp:151`** -- TODO: Investigate if relative-to-absolute output offset conversion is necessary / why this is done.

- **`src/cryptonote_core/blockchain.cpp:278`** -- FIXME: possibly move DB initialization into the constructor to avoid accidentally dereferencing a null BlockchainDB pointer.

- **`src/cryptonote_core/blockchain.cpp:341`** -- TODO: add function to create and store genesis block, taking testnet into account.

- **`src/cryptonote_core/blockchain.cpp:353`** -- TODO: if blockchain load successful, verify blockchain against both hard-coded and runtime-loaded (and enforced) checkpoints.

- **`src/cryptonote_core/blockchain.cpp:488`** -- TODO: make sure sync() exceptions are not simply ignored higher up the call stack.

- **`src/cryptonote_core/blockchain.cpp:767`** -- TODO: function was "poorly written" and its intent is unclear. Needs investigation and rewrite.

- **`src/cryptonote_core/blockchain.cpp:1523-1530`** -- TODO/FIXME: `create_block_template` only had minimal modifications for BlockchainDB. References `DEBUG_CREATE_BLOCK_TEMPLATE` flag that is not referenced elsewhere.

- **`src/cryptonote_core/blockchain.cpp:1594`** -- TODO: bare assert with no message when `get_block_by_hash` fails for `from_block` in block template creation.

- **`src/cryptonote_core/blockchain.cpp:1643, 1963`** -- FIXME: consider moving away from `block_extended_info` at some point. Duplicated in two separate locations.

- **`src/cryptonote_core/blockchain.cpp:2033`** -- FIXME: incomplete comment about allowing block by hash lookups from the DB.

- **`src/cryptonote_core/blockchain.cpp:2512, 2598`** -- TODO: return type should be void with exceptions instead of bool for `get_blocks` and `get_transactions` functions.

- **`src/cryptonote_core/blockchain.cpp:2785`** -- FIXME: change `find_blockchain_supplement` argument to `std::vector`, low priority.

- **`src/cryptonote_core/blockchain.cpp:3003`** -- FIXME: function seems to be merely a wrapper around another function of the same name with one extra bit of functionality.

- **`src/cryptonote_core/blockchain.cpp:3309`** -- FIXME: consider moving functionality specific to one input into `check_tx_input()` rather than having it in `check_tx_inputs()`.

- **`src/cryptonote_core/blockchain.cpp:3806`** -- TODO: `check_block_timestamp` has changed on upstream, needs revisit.

- **`src/cryptonote_core/blockchain.cpp:4109, 4121`** -- XXX: notes about differences from old code in block addition (miner tx handling and tx existence checking).

- **`src/cryptonote_core/blockchain.cpp:4177`** -- TODO: move the section checking if daemon has all txs from a block to right after the PoW check for efficiency.

- **`src/cryptonote_core/blockchain.cpp:4206`** -- FIXME: the storage should not be responsible for validation. Separation of concerns issue.

- **`src/cryptonote_core/blockchain.cpp:4233`** -- TODO: why is keeping invalid blocks done? Make sure it makes sense.

- **`src/cryptonote_core/blockchain.cpp:4310`** -- TODO: figure out the best way to deal with block addition failure (currently just logs error).

- **`src/cryptonote_core/blockchain.cpp:4543`** -- TODO: Refactor, consider returning a failure height and letting caller decide course of action for chain reorganization.

- **`src/cryptonote_core/blockchain.cpp:5504`** -- FIXME: clear tx_pool because the process might have been terminated and caused it to store txs kept by blocks.

- **`src/cryptonote_core/blockchain.h:1175`** -- TODO: evaluate whether typedefs are left over from `blockchain_storage` (legacy code).

- **`src/cryptonote_core/blockchain.h:1187`** -- TODO: add reader/writer lock to `m_blockchain_lock` instead of using critical section. Currently uses a single mutex which limits concurrent read access.

- **`src/cryptonote_core/tx_pool.h:631`** -- TODO: confirm comments and investigate whether current behavior is desired.

- **`src/cryptonote_core/tx_pool.h:653`** -- TODO: time constant for stuck transaction check should be a named constant, not hard-coded.

- **`src/cryptonote_core/tx_pool.h:657`** -- TODO: look into doing the sorted tx container better.

- **`src/cryptonote_core/tx_pool.cpp:87`** -- TODO: constants should be in the header or somewhere more accessible.

- **`src/cryptonote_core/tx_pool.cpp:206`** -- TODO: Investigate why `kept_by_block` transactions skip key image spent check.

- **`src/cryptonote_core/tx_pool.cpp:730, 785, 1190, 1567`** -- TODO: investigate whether boolean return is appropriate. Four separate functions (`remove_stuck_transactions`, `get_relayable_transactions`, `get_transactions_and_spent_keys_info`, `fill_block_template`) all have this same concern.

### Blockchain DB (`src/blockchain_db/`)

- **`src/blockchain_db/blockchain_db.h:1141`** -- TODO: Rewrite so all calls to `remove_*` are done in concrete members of the base class rather than leaking implementation details.

- **`src/blockchain_db/blockchain_db.h:1364`** -- TODO: decide if current behavior is correct for missing transactions (return behavior unclear).

- **`src/blockchain_db/blockchain_db.h:1394`** -- TODO: should outputs spent with a low mixin (especially 0) be excluded from the count? Privacy implications.

- **`src/blockchain_db/blockchain_db.h:1481`** -- FIXME: undocumented function. "Need to check with git blame and ask what this does to document it."

- **`src/blockchain_db/blockchain_db.h:1829`** -- TODO: `migrate_*` should perhaps be (or call) a series of functions which progressively update through version upgrades.

- **`src/blockchain_db/lmdb/db_lmdb.cpp:972`** -- TODO: compare pros and cons of looking up the tx hash's tx index once and passing it into functions vs repeated lookups (performance concern).

### Crypto (`src/crypto/`, `src/ringct/`)

- **`src/crypto/crypto.cpp:150`** -- TODO: allow specifying random value for key generation (for wallet recovery).

- **`src/ringct/rctSigs.cpp:1219`** -- TODO: unused variable `txnFeeKey` -- commented-out code computing `scalarmultH(d2h(rv.txnFee))`.

### Multisig (`src/multisig/`)

- **`src/multisig/multisig_account.h:61`** -- TODO: encapsulate key preparation for aggregation-style signing.

- **`src/multisig/multisig_account.h:263`** -- TODO: record which other signers have private key shares to enable aggregation signing instead of round-robin.

- **`src/multisig/multisig_account_kex_impl.cpp:480`** -- TODO: move `n_choose_k` function to a dedicated math library with unit tests.

- **`src/multisig/multisig_account_kex_impl.cpp:788`** -- TODO: record [pre-aggregation pubkeys : origins] map for aggregation-style signing.

### Wallet (`src/wallet/`)

- **`src/wallet/wallet2.h:343`** -- TODO: `m_key_image` stored twice in `transfer_details` struct. Wasteful duplication.

- **`src/wallet/wallet2.h:1945`** -- TODO: `m_upper_transaction_weight_limit` uses a fixed value; should auto-calculate or request from daemon.

- **`src/wallet/wallet2.cpp:1913`** -- TODO: handle the sweep case where wallet needs to detect tx2 by scanning tx1 first.

- **`src/wallet/wallet2.cpp:3699`** -- TODO: set `tx_propagation_timeout` to `CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE * 3 / 2` after v15 hardfork. Currently hardcoded to 500 seconds.

- **`src/wallet/wallet2.cpp:7352`** -- XXX: "this needs to be fast, so we'd need to get the starting heights from the daemon to be correct once voting kicks in." Performance concern in output selection.

- **`src/wallet/wallet_rpc_server.cpp:2159`** -- TODO: should the whole RPC call fail because of one bad transaction ID?

- **`src/wallet/api/wallet2_api.h:595`** -- TODO: check if `connectToDaemon` can be removed from the public API.

- **`src/wallet/api/wallet2_api.h:1309`** -- TODO: "delme" -- `walletExists` function marked for deletion.

- **`src/wallet/api/wallet.cpp:237, 252`** -- TODO: two empty TODO comments with no description in `on_unconfirmed_money_received` and related callback.

- **`src/wallet/api/wallet.cpp:500, 538`** -- TODO: validate language parameter when creating wallets. Currently accepts any string.

- **`src/wallet/api/wallet.cpp:715`** -- TODO: handle "deprecated" wallet format when opening.

- **`src/wallet/api/wallet.cpp:811`** -- TODO: unclear what needs to be done in `getSeed()` return.

- **`src/wallet/api/wallet.cpp:1072`** -- TODO: make `doRefresh` return bool to distinguish refresh errors from other errors.

- **`src/wallet/api/wallet.cpp:1610`** -- TODO: properly handle payment ID (add another method with explicit `payment_id` param) and handle amounts per-destination.

- **`src/wallet/api/wallet.cpp:1660`** -- TODO: copy-paste URL-as-address resolution from simplewallet.cpp. Missing functionality in API vs CLI.

- **`src/wallet/api/wallet.cpp:1717, 1811`** -- TODO: make error messages translatable with `tr()`. Error strings for `daemon_busy` exceptions are not localized.

- **`src/wallet/api/wallet.cpp:1928`** -- TODO: thread synchronization needed for `setListener()`.

- **`src/wallet/api/wallet.cpp:2478`** -- TODO: synchronize access in `pauseRefresh()`.

- **`src/wallet/api/pending_transaction.cpp:135`** -- TODO: extract method from transaction commit error handling loop.

- **`src/wallet/api/pending_transaction.cpp:138`** -- TODO: make error messages translatable with `tr()`.

- **`src/wallet/api/pending_transaction.h:56`** -- TODO: continue with interface (incomplete API).

- **`src/wallet/api/unsigned_transaction.cpp:246`** -- TODO: Is the mixin loop needed or is `sources[0]` sufficient?

- **`src/wallet/api/unsigned_transaction.cpp:293`** -- TODO: return integrated address if short payment ID exists.

- **`src/wallet/api/transaction_history.cpp:113`** -- TODO: configurable values for transaction history query (currently hardcoded min_height).

### P2P & Network (`src/p2p/`, `src/cryptonote_protocol/`, `src/net/`)

- **`src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:82`** -- XXX: hardcoded minimum block size estimate of 500 bytes.

- **`src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:108`** -- XXX: empty comment in rate limiting section.

- **`src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:125`** -- XXX: commented-out `delay = 0` line -- leftover debug code.

- **`src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:129-130`** -- XXX/TODO: debug sleep message and non-randomized sleep timing. "TODO randomize sleeps" -- rate limiting sleeps have a predictable pattern that could be fingerprinted.

- **`src/cryptonote_protocol/cryptonote_protocol_handler-base.cpp:134`** -- XXX LATER XXX: placeholder for future work in rate limiting.

- **`src/cryptonote_protocol/cryptonote_protocol_handler.inl:594`** -- TODO: Eventually drop support for old `NOTIFY_NEW_BLOCK` endpoint (deprecated protocol message).

- **`src/cryptonote_protocol/cryptonote_protocol_handler.inl:983, 990`** -- TODO: add announce usage tracking for Dandelion++ stem and fluff relay paths.

- **`src/cryptonote_protocol/cryptonote_protocol_handler.inl:1030`** -- XXX: commented-out response block size handler call.

- **`src/cryptonote_protocol/cryptonote_protocol_handler.inl:1966, 2782`** -- TODO (x2): investigate tallying peer counts by zone and comparing to max out peers by zone. Currently only considers public zone.

- **`src/cryptonote_protocol/cryptonote_protocol_handler.h:233`** -- XXX: commented-out response block handler.

- **`src/p2p/net_node.inl:779`** -- TODO: allow DNS seed node lookups via SOCKS proxy (remote-side resolution for anonymity networks).

- **`src/p2p/net_node.inl:788`** -- TODO: add IPv6 support for seed nodes.

- **`src/p2p/net_node.inl:808`** -- TODO: care about DNSSEC availability/validity when resolving seed nodes. Currently ignores DNSSEC status.

- **`src/net/tor_address.cpp:63`** -- TODO: v3 onion addresses have a checksum; base32 decoding is required to verify it. Currently not validated.

- **`src/net/i2p_address.cpp:47`** -- TODO: only b32 I2P addresses supported right now. No support for other address formats.

### RPC (`src/rpc/`)

- **`src/rpc/daemon_handler.cpp:241`** -- TODO: consider fixing `core::get_transactions` to not hide exceptions. Error handling is swallowed.

- **`src/rpc/daemon_handler.cpp:457`** -- TODO: make sure tx has reached other nodes before returning success. No relay confirmation.

- **`src/rpc/core_rpc_server.cpp:1445, 3352`** -- TODO (x2): same concern -- no confirmation that relayed transactions reached other nodes.

- **`src/rpc/core_rpc_server.cpp:3342`** -- TODO: `get_pool_transaction` could have an optional meta parameter to avoid a separate lookup.

- **`src/rpc/core_rpc_server_commands_defs.h:1512`** -- TODO: expose pool transaction data directly instead of as JSON string (`tx_json`).

- **`src/rpc/message_data_structs.h:157`** -- TODO: consider adding a data member to the block header response struct.

### Daemon & CLI (`src/daemon/`, `src/simplewallet/`)

- **`src/daemon/core.h:53`** -- TEMPORARY HACK: variable map is copied to avoid the original going out of scope before `run()` is called. Creates unnecessary copy of potentially large config.

- **`src/daemon/core.h:84`** -- TODO: get rid of circular dependencies in internals.

- **`src/daemon/main.cpp:129`** -- TODO: parse debug options like log level at startup.

- **`src/daemon/main.cpp:267`** -- FIXME: not sure on Windows implementation default for relative path base, needs further review.

- **`src/daemon/rpc_command_executor.cpp:1420`** -- TODO (commented-out): get rid of hard-coded constants in Windows service stop logic.

- **`src/daemonizer/windows_daemonizer.inl:140`** -- TODO: set the service status for return codes in Windows daemonizer.

### Epee Framework (`contrib/epee/`)

- **`contrib/epee/include/net/abstract_tcp_server2.inl:42-43`** -- TODO (x2): bare TODO comments on Boost includes -- unclear what needs to change, possibly migrate to `std::` equivalents.

- **`contrib/epee/include/net/network_throttle-detail.hpp:62`** -- TODO: slot size is hardcoded for 1 second in `time_to_slot()`. Should be configurable.

- **`contrib/epee/include/net/network_throttle-detail.hpp:63`** -- TODO: for large window sizes, performance should be improved by subtracting on change of `m_last_sample_time` instead of recalculating the average.

- **`contrib/epee/include/net/network_throttle-detail.hpp:95`** -- TODO: `get_sleep_time()` documented as "not safe: only if time didn't change" -- potential TOCTOU issue.

- **`contrib/epee/include/net/network_throttle.hpp:74`** -- TODO: enforce that casts between network time/speed types are only explicit to prevent unit conversion mistakes.

- **`contrib/epee/include/net/network_throttle.hpp:104`** -- XXX: `i_network_throttle` members marked `public` instead of intended `protected`. Breaks encapsulation.

- **`contrib/epee/include/net/levin_protocol_handler_async.h:700`** -- TODO: "or better just keep removing random elements (performance)" -- current connection closing strategy is suboptimal.

- **`contrib/epee/include/net/abstract_tcp_server2.h:533`** -- TODO: change `m_thread_name_prefix` from string to enum `server_type`.

- **`contrib/epee/include/storages/portable_storage_template_helper.h:32`** -- TODO: (mj-xmr) reduce include dependency in a future PR.

- **`contrib/epee/include/storages/portable_storage.h:124`** -- TODO: XML format support abandoned -- dead return-false code path.

- **`contrib/epee/include/storages/portable_storage.h:229`** -- TODO: optimize code to work without `get_next_val` function.

- **`contrib/epee/include/storages/portable_storage_from_bin.h:205`** -- TODO: add optimization for binary deserialization loop.

- **`contrib/epee/include/storages/levin_abstract_invoke2.h:64`** -- TODO: add true const support to serialization. Currently uses `const_cast` to call `store()` on a const reference.

- **`contrib/epee/src/portable_storage.cpp:113`** -- TODO: bare TODO on `return true` after `load_from_binary` -- unclear if additional validation is needed.

- **`contrib/epee/src/network_throttle-detail.cpp:59`** -- TODO: bare TODO with no description.

- **`contrib/epee/src/network_throttle-detail.cpp:168`** -- TODO: optimize slot movement when moving multiple slots at once.

- **`contrib/epee/src/network_throttle-detail.cpp:351`** -- TODO: throttle decision weights `a1=20, a2=10, a3=10, am=10` are tuning constants with a note "TODO 70 => 20" suggesting they are not finalized.

- **`contrib/epee/src/connection_basic.cpp:46`** -- TODO: bare TODO with no description above throttle include.

- **`contrib/epee/src/connection_basic.cpp:252`** -- XXX LATER XXX: placeholder for future work in connection handling.

### Serialization (`src/serialization/`)

- **`src/serialization/binary_archive.h:52`** -- TODO: fix `size_t` warning on x32 platforms. Architecture-specific compilation issue.

### Other (`src/common/`, `src/cryptonote_basic/`, `src/checkpoints/`, `src/blockchain_utilities/`)

- **`src/common/dns_utils.cpp:397`** -- TODO: "parse the string in a less stupid way, probably with regex." The `address_from_txt_record` function uses fragile manual string parsing.

- **`src/common/dns_utils.cpp:447`** -- TODO: update to allow conveying that DNSSEC was not available (vs. available but invalid).

- **`src/common/dns_utils.h:107, 159`** -- TODO (x2): modify DNS record retrieval functions to properly accommodate DNSSEC validation status.

- **`src/common/rpc_client.h:106, 134`** -- TODO (x2): handle `CORE_RPC_STATUS_BUSY` response from daemon. Currently treats busy as an error rather than retrying.

- **`src/common/notify.cpp:44-46`** -- TODO: improve tokenization to handle paths with whitespace/quotes; add Windows unicode support.

- **`src/cryptonote_basic/account.cpp:274, 280`** -- TODO (x2): change address formatting code to base 58 (both standard and integrated address to-string functions).

- **`src/cryptonote_basic/cryptonote_format_utils.cpp:238`** -- TODO: validate tx after parsing from blob. Currently parses but does not validate.

- **`src/cryptonote_basic/difficulty.cpp:158`** -- TODO: consider throwing an exception instead of returning 0 on difficulty overflow. Returning 0 triggers "difficulty overhead" error at a higher level.

- **`src/blockchain_utilities/blockchain_import.cpp:331`** -- TODO: use `bootstrap.read_chunk()` instead of manual buffer reading.

- **`src/blockchain_utilities/blockchain_import.cpp:559`** -- TODO: if there was an error, the last added block is probably at zero-based height h-2 (error reporting is off-by-one).

- **`src/blockchain_utilities/blockchain_import.cpp:732`** -- TODO: protocol stub is only for testing; should use real validation of relayed objects.

---

## Design Concerns

Architectural and systemic issues observed while reviewing the codebase.

### Thread Safety Gaps

1. **LMDB close not thread-safe** (`src/blockchain_db/lmdb/db_lmdb.cpp:1637`): The database close operation explicitly states it is not thread safe, yet the daemon can have multiple threads accessing the DB concurrently.

2. **Wallet API missing synchronization**: Multiple wallet API methods (`setListener` at wallet.cpp:1928, `pauseRefresh` at wallet.cpp:2478) have TODO comments acknowledging missing thread synchronization. The wallet API is expected to be called from GUI threads.

3. **Non-thread-safe crypto functions**: `generate_random_bytes_not_thread_safe()` and `add_extra_entropy_not_thread_safe()` in `src/crypto/random.c` are used in `src/crypto/crypto.cpp:102,108`. While callers may hold locks, the naming suggests this is a known concern.

4. **Blockchain lock granularity**: `blockchain.h:1187` notes the lock should be a reader/writer lock instead of a critical section. This is a performance bottleneck since all blockchain access (including reads) takes an exclusive lock.

### Missing Error Handling

5. **Swallowed exceptions in RPC**: `core::get_transactions` hides exceptions (`src/rpc/daemon_handler.cpp:241`), making it difficult to diagnose failures in transaction retrieval.

6. **Transaction relay with no confirmation**: At least three separate locations (`daemon_handler.cpp:457`, `core_rpc_server.cpp:1445,3352`) return success to the caller without confirming that relayed transactions actually reached other nodes.

7. **Bool-return functions that should throw**: Multiple functions in `blockchain.cpp` (lines 2512, 2598) and `tx_pool.cpp` (lines 730, 785, 1190, 1567) return bool when they should throw exceptions, making error propagation unreliable.

8. **RPC busy status ignored**: `rpc_client.h:106,134` treats `CORE_RPC_STATUS_BUSY` as a generic error rather than implementing retry logic.

### Incomplete Validation

9. **Missing tx validation after parse**: `cryptonote_format_utils.cpp:238` parses a transaction from a blob but has a TODO to actually validate it. Deserialized but unvalidated transactions could propagate.

10. **Tor v3 address checksum not verified**: `tor_address.cpp:63` notes that v3 onion addresses have checksums that are not currently validated, allowing malformed addresses.

11. **I2P address format limited**: `i2p_address.cpp:47` only supports b32 addresses.

12. **DNSSEC validation ignored**: `p2p/net_node.inl:808` does not check DNSSEC availability or validity when resolving seed node DNS records, leaving this network bootstrap path vulnerable to DNS spoofing.

13. **Language parameter unvalidated**: `wallet.cpp:500,538` accept any string as a mnemonic language without validation.

### Legacy Code / Dead Code

14. **XML format dead code**: `portable_storage.h:124` contains a `return false` for XML format loading with a comment indicating it will never be used again.

15. **Leftover debug code**: Multiple XXX-marked debug artifacts in `cryptonote_protocol_handler-base.cpp` (commented-out delay, debug sleep messages, placeholder blocks).

16. **Deprecated protocol endpoints**: `cryptonote_protocol_handler.inl:594` still supports old `NOTIFY_NEW_BLOCK` that should eventually be dropped.

17. **Unused typedef leftovers**: `blockchain.h:1175` has typedefs potentially left over from the old `blockchain_storage` class.

### Encapsulation / API Issues

18. **Broken access control in throttle**: `network_throttle.hpp:104` has members that should be `protected` marked as `public` with an XXX comment.

19. **Const-correctness bypass**: `levin_abstract_invoke2.h:64` uses `const_cast` to call `store()` on a const reference, indicating the serialization framework lacks proper const support.

20. **Circular dependencies**: `daemon/core.h:84` explicitly notes circular dependency issues in the daemon internals.

21. **Variable map copy hack**: `daemon/core.h:53` copies the entire variable map to work around a lifetime/scope issue, indicating an architectural problem with initialization ordering.

### Privacy Concerns

22. **Predictable rate limiting sleep**: `cryptonote_protocol_handler-base.cpp:130` has a TODO to randomize sleep times. Predictable sleep patterns could enable traffic analysis.

23. **Low-mixin output counting**: `blockchain_db.h:1394` raises the question of whether outputs spent with low mixin (especially 0) should be excluded from counts, which has privacy implications for ring signature selection.
