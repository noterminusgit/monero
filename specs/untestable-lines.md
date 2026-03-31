# Untestable Lines and Functions

This document catalogs every source file with code that **cannot be unit tested** and explains why. Functions are categorized by the reason they resist unit testing.

---

## Categories

| Code | Category | Description |
|------|----------|-------------|
| **D** | Requires Daemon | Needs running `monerod` with blockchain state, RPC server, or P2P network |
| **N** | Requires Network | Needs HTTP connections, sockets, DNS resolution, or Levin protocol |
| **H** | Requires Hardware | Needs physical Ledger/Trezor device connected via USB/HID |
| **A** | Anonymous Namespace | Helper functions in `namespace {}` that cannot be accessed from test code |
| **I** | Interactive/CLI | Requires terminal I/O, signal handlers, or user prompts |
| **P** | Platform-specific | OS-specific code paths (Windows services, POSIX signals, etc.) |
| **E** | Entry Point | `main()` functions that bootstrap entire applications |
| **T** | Template Instantiation | Requires specific template parameters only available at link time with full daemon |

---

## 1. Wallet Layer

### `src/wallet/wallet2.cpp` (15,447 lines)

**~70% untestable** (~10,800 lines). The wallet is tightly coupled to a daemon via `m_node_rpc_proxy` and `m_http_client`.

#### D — Requires Daemon Connection (~9,500 lines)

| Function | Lines | Reason |
|----------|-------|--------|
| `refresh()`, `pull_blocks()`, `pull_and_parse_next_blocks()` | ~1170-2200 | Full block sync via RPC |
| `pull_hashes()`, `pull_next_blocks()` | ~2200-2500 | Block hash retrieval via RPC |
| `process_parsed_blocks()`, `process_new_transaction()` | ~2500-3600 | Block/tx processing with key image tracking |
| `update_pool_state()`, `process_pool_state()` | ~3600-3800 | Mempool sync via `get_transaction_pool_hashes` RPC |
| `fast_refresh()` | ~3900-4050 | Hash-only sync via daemon |
| `get_rct_distribution()` | ~4325-4363 | Output distribution via `/get_output_distribution.bin` |
| `detach_blockchain()`, `handle_reorg()` | ~4365-4500 | Chain reorganization handling |
| `check_connection()`, `check_version()` | ~6347-6402 | Daemon connectivity check via `m_http_client` |
| `rescan_spent()` | ~7240-7291 | Key image spent status via `/is_key_image_spent` RPC |
| `rescan_blockchain()` | ~7293-7400 | Full blockchain rescan |
| `get_dynamic_base_fee_estimate()`, `get_base_fee()` | ~8488-8550 | Fee estimation via `m_node_rpc_proxy` |
| `create_transaction()`, `create_transactions_2()`, `create_transactions_all()`, `create_transactions_single()`, `create_transactions_from()` | ~8600-10200 | Transaction construction (needs decoy selection via daemon) |
| `get_outs()`, `get_fake_outs()` | ~9000-9500 | Decoy output selection via RPC histogram/distribution |
| `transfer_selected()`, `transfer_selected_rct()` | ~10200-10800 | Transfer execution with RPC calls |
| `commit_tx()` | ~10800-10900 | Broadcasting via `/sendrawtransaction` |
| `get_tx_key()`, `check_tx_key()`, `check_tx_proof()` | ~11000-11500 | Proof verification (some need daemon for key images) |
| `get_reserve_proof()`, `check_reserve_proof()` | ~11500-11800 | Reserve proof generation/verification |
| `import_key_images()`, `export_key_images()` | ~12000-12300 | Key image import/export with daemon sync |
| `submit_multisig()` | ~12500-12600 | Multisig tx submission via daemon |
| `use_fork_rules()` | ~8450-8480 | Hard fork check via `m_node_rpc_proxy` |
| `light_wallet_*()` (all) | ~13000-13500 | Light wallet mode (OpenMonero API) |

#### A — Anonymous Namespace Helpers (~200 lines)

| Function | Lines | Reason |
|----------|-------|--------|
| `get_transaction_history()` helpers | ~150-200 | Internal formatting, not accessible externally |
| Various lambda helpers | scattered | Inline lambdas for sorting/filtering |

#### Already Testable / Tested (~4,400 lines)

| Function | Status |
|----------|--------|
| `generate()`, `restore_keys()`, key management | Tested |
| `get_fee_multiplier()`, `get_fee_algorithm()`, fee utilities | Tested |
| Subaddress generation, `get_subaddress()` | Tested |
| File I/O: `store()`, `load()`, password change | Tested |
| `get_address_as_str()`, URI parsing, attributes, tx notes | Tested |
| Balance queries (`balance()`, `unlocked_balance()`) | Tested |
| Address book operations | Tested |
| `check_hard_fork_version()` (static) | Testable |

---

### `src/wallet/wallet_rpc_server.cpp` (5,166 lines)

**100% untestable via unit tests.** Every function is an RPC handler that requires a fully initialized wallet2 + daemon.

| Category | Lines | Reason |
|----------|-------|--------|
| D+N | 5,166 | All `on_*` handlers call wallet2 methods that need daemon |

---

### `src/wallet/node_rpc_proxy.cpp` (350 lines)

**100% untestable.** All functions call `m_daemon_rpc_mutex` + `invoke_http_json_rpc`.

| Category | Lines | Reason |
|----------|-------|--------|
| D+N | 350 | Pure RPC proxy — every method does HTTP to daemon |

---

### `src/wallet/message_store.cpp` (1,495 lines)

**~85% untestable.** Requires PyBitmessage network transport.

| Category | Lines | Reason |
|----------|-------|--------|
| N | ~1,270 | Message send/receive via `message_transporter` (HTTP to PyBitmessage) |
| Testable | ~225 | `signer_config_complete()`, `signer_labels_complete()`, `get_sanitized_text()` — pure logic |

---

## 2. RPC Layer

### `src/rpc/core_rpc_server.cpp` (3,964 lines)

**~95% untestable.** Each `on_*` handler requires `m_core` (Blockchain + tx_pool + P2P).

| Category | Lines | Reason |
|----------|-------|--------|
| D | ~3,760 | All `on_*` handlers: `on_get_height`, `on_get_blocks`, `on_get_hashes`, `on_get_info`, `on_mining_status`, `on_get_transaction_pool`, `on_get_connections`, `on_get_block_header*`, `on_submit_block`, `on_get_output_histogram`, `on_get_coinbase_tx_sum`, `on_get_output_distribution`, `on_sync_info`, `on_pop_blocks`, etc. |
| Testable | ~200 | RPC command request/response serialization roundtrips (already tested) |

### `src/rpc/daemon_handler.cpp` (950 lines)

**100% untestable.** ZMQ RPC handler requiring full daemon core.

| Category | Lines | Reason |
|----------|-------|--------|
| D | 950 | All `handle()` methods call `m_core` |

---

### `src/daemon/rpc_command_executor.cpp` (2,501 lines)

**100% untestable.** Every command calls `m_rpc_server->on_*()` or `m_rpc_client->json_rpc_request()`.

| Category | Lines | Reason |
|----------|-------|--------|
| D+N | ~2,350 | All command methods (print_height, print_block, show_hash_rate, etc.) |
| A | ~200 | Anonymous namespace helpers: `get_human_time_ago()`, `get_time_hms()`, `make_error()`, `print_peer()`, `print_block_header()` — these are testable logic trapped in anonymous namespace |

**Recommendation:** Refactor `get_human_time_ago`, `get_time_hms`, `make_error` out of anonymous namespace to enable testing (~200 lines recoverable).

---

## 3. Core Layer

### `src/cryptonote_core/blockchain.cpp` (5,616 lines)

**~55% untestable** (~3,100 lines). Many functions tested via TestDB/FAKECHAIN fixture.

| Category | Lines | Reason |
|----------|-------|--------|
| D | ~3,100 | `add_new_block()`, `handle_block_to_main_chain()`, `handle_alternative_block()`, `pop_block_from_blockchain()`, `validate_block_template()`, `create_block_template()`, `check_tx_inputs()`, `have_tx_keyimges_as_spent()` (the spending version), `get_block_template()`, `update_next_cumulative_weight_limit()`, `add_block_as_invalid()`, `check_block_timestamp()`, `get_last_n_blocks_weights()`, `get_long_term_block_weight_median()`, `complete_timestamps_vector()`, `build_alt_chain()`, full validation paths |
| Already tested | ~2,500 | `init()`, `get_current_blockchain_height()`, `get_tail_id()`, `get_difficulty_*()`, `get_dynamic_base_fee()`, `get_dynamic_base_fee_estimate_2021_scaling()`, `check_fee()`, `get_fee_quantization_mask()`, `check_tx_outputs()`, `have_block()`, `have_tx()`, `get_short_chain_history()`, `find_blockchain_supplement()`, `check_difficulty_checkpoints()`, `get_hard_fork_*()`, `get_blocks()`, `get_transactions()`, hard fork queries |

---

### `src/cryptonote_core/cryptonote_core.cpp` (1,970 lines)

**~75% untestable** (~1,480 lines).

| Category | Lines | Reason |
|----------|-------|--------|
| D | ~1,480 | `init()`, `deinit()`, `handle_incoming_tx()`, `handle_incoming_txs()`, `handle_incoming_block()`, `prepare_handle_incoming_blocks()`, `cleanup_handle_incoming_blocks()`, `on_synchronized()`, `get_blockchain_storage()`, `get_miner()`, mining control |
| Testable | ~490 | `construct_miner_tx()`, `get_account_address_checksum()`, `get_block_hashing_blob()`, serialization utilities — partially tested |

---

### `src/cryptonote_core/tx_pool.cpp` (1,966 lines)

**~40% untestable** (~790 lines).

| Category | Lines | Reason |
|----------|-------|--------|
| D | ~790 | `add_tx()` full validation paths (need blockchain for key image double-spend checks, hard fork version), `on_blockchain_inc()`, `on_blockchain_dec()`, `get_transactions_and_spent_keys_info()`, `get_pool_for_rpc()` |
| Already tested | ~1,176 | `add_tx()` error paths, `take_tx()`, `remove_stuck_transactions()`, `have_tx_keyimg_as_spent()`, `remove_transaction_keyimages()`, `get_complement()`, transaction lifecycle |

---

## 4. P2P / Network Layer

### `src/p2p/net_node.inl` (3,257 lines)

**~85% untestable** (~2,770 lines). Template class requiring P2P infrastructure.

| Category | Lines | Reason |
|----------|-------|--------|
| T+N | ~2,770 | `run()`, `init()`, `deinit()`, `try_to_connect_and_handshake_with_new_peer()`, `do_handshake_with_peer()`, `handle_remote_peerlist()`, `idle_worker()`, `connections_maker()`, `make_new_connection_from_peerlist()`, `fix_time_delta()`, `handle_*()` P2P commands, peer management, zone management |
| Testable | ~487 | Basic data structures serialization (tested in test_protocol_pack.cpp, p2p_net_node_tests.cpp) |

---

### `src/cryptonote_protocol/cryptonote_protocol_handler.inl` (2,908 lines)

**~99% untestable** (~2,880 lines).

| Category | Lines | Reason |
|----------|-------|--------|
| T+D+N | ~2,880 | `handle_notify_new_block()`, `handle_notify_new_fluffy_block()`, `handle_notify_new_transactions()`, `handle_request_get_objects()`, `handle_response_get_objects()`, `process_payload_sync_data()`, `on_connection_synchronized()`, `try_add_next_blocks()`, all sync state machine logic — requires `m_core` + `m_p2p` |
| Testable | ~28 | `get_payload_sync_data()` (CORE_SYNC_DATA serialization — tested) |

---

## 5. Daemon / CLI Layer

### `src/daemon/daemon.cpp` (284 lines)

**100% untestable.** Orchestrates core, P2P, RPC, ZMQ lifecycle.

| Category | Lines | Reason |
|----------|-------|--------|
| D+N+I | 284 | `run()`, `stop()`, `stop_p2p()`, constructor — all need full daemon infrastructure |

---

### `src/daemon/main.cpp` (373 lines)

**100% untestable.** Entry point.

| Category | Lines | Reason |
|----------|-------|--------|
| E+I | 373 | `main()`, command-line parsing, daemon bootstrap |

---

### `src/simplewallet/simplewallet.cpp` (11,557 lines)

**100% untestable.** Interactive CLI application.

| Category | Lines | Reason |
|----------|-------|--------|
| I+D | ~11,000 | All command handlers: interactive prompts, readline, wallet2 calls |
| A | ~557 | Anonymous namespace helpers: `parse_bool()`, `datestr_to_int()`, `interpret_auto_refresh_status()`, formatting functions — testable logic trapped in anonymous namespace |

**Recommendation:** Refactor ~557 lines of helpers out of anonymous namespace.

---

## 6. Hardware Device Layer

### `src/device/device_ledger.cpp` (2,380 lines)

**~90% untestable** (~2,142 lines). Requires physical Ledger device.

| Category | Lines | Reason |
|----------|-------|--------|
| H | ~2,142 | All `device_ledger::*` methods: `init()`, `connect()`, `generate_keys()`, `derive_subaddress_public_key()`, `sign()`, `open_tx()`, `close_tx()` — all send APDU commands via HID |
| Testable | ~238 | `ABPkeys`, `Keymap`, `HMACmap`, `SecHMAC`, `Status::to_string()` — helper classes (tested in device_registry.cpp) |

### `src/device/device_io_hid.cpp` (362 lines)

**100% untestable.** USB HID communication.

| Category | Lines | Reason |
|----------|-------|--------|
| H | 362 | `connect()`, `exchange()`, `wrapCommand()`, `unwrapReponse()` — all use `hid_*()` API |

### `src/device_trezor/` (3,973 lines total)

**100% untestable.** Requires Trezor device + protobuf + libusb.

| File | Lines | Category |
|------|-------|----------|
| `device_trezor_base.cpp` | 578 | H |
| `device_trezor.cpp` | 780 | H |
| `trezor/protocol.cpp` | 1,091 | H |
| `trezor/transport.cpp` | 1,293 | H |
| `trezor/messages_map.cpp` | 142 | H |
| `trezor/debug_link.cpp` | 89 | H |

---

## 7. Mining

### `src/cryptonote_basic/miner.cpp` (1,152 lines)

**~95% untestable** (~1,094 lines).

| Category | Lines | Reason |
|----------|-------|--------|
| D+P | ~1,094 | `start()`, `stop()`, `worker_thread()`, `request_block_template()` — needs blockchain + threads; platform-specific thread management |
| Testable | ~58 | `get_is_background_mining_enabled()`, `get_mining_throttle()` — simple getters |

---

## 8. Blockchain Utilities

### `src/blockchain_utilities/*.cpp` (6,089 lines total)

**100% untestable.** All are standalone `main()` programs that open LMDB databases.

| File | Lines | Category | Reason |
|------|-------|----------|--------|
| `blockchain_import.cpp` | 786 | E+D | Opens DB, imports blocks |
| `blockchain_export.cpp` | 184 | E+D | Opens DB, exports blocks |
| `blockchain_blackball.cpp` | 1,709 | E+D | Opens DB, marks outputs |
| `blockchain_ancestry.cpp` | 725 | E+D | Opens DB, traces ancestry |
| `blockchain_depth.cpp` | 323 | E+D | Opens DB, measures depth |
| `blockchain_prune.cpp` | 737 | E+D | Opens DB, prunes blocks |
| `blockchain_prune_known_spent_data.cpp` | 283 | E+D | Opens DB, prunes spent data |
| `blockchain_stats.cpp` | 385 | E+D | Opens DB, prints stats |
| `blockchain_usage.cpp` | 242 | E+D | Opens DB, reports usage |
| `bootstrap_file.cpp` | 534 | D | Block file serialization |
| `blocksdat_file.cpp` | 181 | D | Blocks.dat file handling |

---

### `src/gen_multisig/gen_multisig.cpp` (239 lines)

**100% untestable.** Entry point for multisig wallet generation.

| Category | Lines | Reason |
|----------|-------|--------|
| E+I | 239 | `main()` + interactive wallet creation |

---

## 9. Common / Epee Utilities

### `src/common/dns_utils.cpp` (599 lines)

**~80% untestable** (~480 lines).

| Category | Lines | Reason |
|----------|-------|--------|
| N | ~480 | `DNSResolver::get_record()`, `get_txt_record()`, `check_address_syntax()` with DNS — all use `ub_resolve()` (libunbound) |
| Testable | ~119 | Address parsing helpers, `DNSResolver` constructor |

### `src/common/updates.cpp` (122 lines)

**100% untestable.** DNS-based update checking.

| Category | Lines | Reason |
|----------|-------|--------|
| N | 122 | `check_updates()` uses DNS TXT records |

### `src/common/download.cpp` (324 lines)

**100% untestable.** HTTP file downloads.

| Category | Lines | Reason |
|----------|-------|--------|
| N | 324 | `download()`, `download_async()` — HTTP client |

### `src/common/notify.cpp` (84 lines)

**~60% untestable** (~50 lines).

| Category | Lines | Reason |
|----------|-------|--------|
| P | ~50 | `spawn()` uses `fork()`/`execv()` (POSIX) or `CreateProcess` (Windows) |
| Testable | ~34 | Constructor, argument processing |

### `src/common/util.cpp` (1,155 lines)

**~40% untestable** (~460 lines). Largely tested already.

| Category | Lines | Reason |
|----------|-------|--------|
| P | ~300 | `daemonize()` (Windows service), signal handlers, `set_console_handler()`, process management |
| N | ~100 | `is_local_address()`, `get_default_data_dir()` — filesystem/platform dependent |
| P | ~60 | Windows-only: `GetModuleFileName`, `SHGetSpecialFolderPathA` |
| Already tested | ~695 | `vercmp()`, `sha256sum()`, `glob_to_regex()`, `get_human_readable_*()`, IP parsing — tested in util.cpp tests |

### `contrib/epee/src/net_helper.cpp` (small)

**100% untestable.** Boost.Asio socket management.

| Category | Lines | Reason |
|----------|-------|--------|
| N | all | TCP connection helpers |

### `contrib/epee/src/mlog.cpp` (~200 lines)

**Partially testable.** Logging initialization.

| Category | Lines | Reason |
|----------|-------|--------|
| P | ~150 | Syslog, file I/O, log rotation |
| Testable | ~50 | Log level parsing, category management |

---

## Summary

| Category | Estimated Lines | % of Codebase |
|----------|----------------|---------------|
| **D** — Requires Daemon | ~28,000 | ~37% |
| **N** — Requires Network | ~4,500 | ~6% |
| **H** — Requires Hardware | ~6,700 | ~9% |
| **I** — Interactive/CLI | ~12,000 | ~16% |
| **A** — Anonymous Namespace | ~960 | ~1.3% |
| **P** — Platform-specific | ~570 | ~0.8% |
| **E** — Entry Points | ~6,500 | ~8.6% |
| **T** — Template Issues | ~5,650 | ~7.5% |
| **Already Testable/Tested** | ~10,000+ | ~13% |
| **Total Source** | ~75,000 | 100% |

**Note:** Categories overlap significantly (many functions are D+N or E+I).

### Achievable Unit Test Coverage Ceiling

Given the architectural constraints above, the **theoretical maximum unit test line coverage** for this codebase is approximately **35-40%**. The remaining ~60-65% requires:

- **Integration tests** with a running daemon (D category)
- **Functional tests** with P2P network (N, T categories)
- **Hardware-in-the-loop tests** (H category)
- **End-to-end CLI tests** (I, E categories)
- **Refactoring** anonymous namespace helpers (A category — ~960 lines recoverable)

### Recommendations to Increase Testability

1. **Refactor anonymous namespace helpers** (~960 lines): Move `rpc_command_executor` helpers (`get_human_time_ago`, `get_time_hms`, `make_error`) and `simplewallet` helpers (`parse_bool`, `datestr_to_int`) into named namespaces or separate headers
2. **Inject dependencies**: Replace direct `m_http_client`/`m_node_rpc_proxy` calls with mockable interfaces
3. **Add integration test suite**: Use `monerod --regtest` mode for daemon-dependent code
4. **Add hardware simulation**: Mock HID layer for device testing
