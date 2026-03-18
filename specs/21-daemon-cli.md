# Daemon & CLI

## Overview

Monero provides two primary command-line tools: `monerod` (the daemon) and `monero-wallet-cli` (simplewallet). The daemon is the full-node implementation that maintains the blockchain, manages P2P networking, serves RPC requests, and optionally provides a ZMQ pub/sub interface. It can run in interactive mode with a built-in command shell or as a background service. The simplewallet is a feature-rich interactive wallet client that connects to a running daemon over RPC, supporting full transaction management, multisig operations, hardware wallet integration, and a Multisig Messaging System (MMS).

## Key Files

| File | Description |
|------|-------------|
| `src/daemon/main.cpp` | Daemon entry point; parses CLI args, loads config file, dispatches to daemonizer or one-shot command execution |
| `src/daemon/daemon.h` | `t_daemon` class declaration; owns all internals and runs the main event loop |
| `src/daemon/daemon.cpp` | `t_daemon` implementation; constructs `t_internals`, starts core/RPC/ZMQ/P2P, manages shutdown |
| `src/daemon/command_server.h` | `t_command_server` declaration; binds interactive command names to handler functions |
| `src/daemon/command_server.cpp` | `t_command_server` implementation; registers all 40+ interactive commands with descriptions |
| `src/daemon/command_parser_executor.h` | `t_command_parser_executor` declaration; parses string arguments then delegates to RPC executor |
| `src/daemon/command_parser_executor.cpp` | `t_command_parser_executor` implementation; argument parsing and validation for each command |
| `src/daemon/rpc_command_executor.h` | `t_rpc_command_executor` declaration; executes commands via RPC client or direct core_rpc_server pointer |
| `src/daemon/rpc_command_executor.cpp` | `t_rpc_command_executor` implementation; issues RPC calls and formats output |
| `src/daemon/command_line_args.h` | Daemon-specific command-line argument definitions (log, ZMQ, proxy, public-node, etc.) |
| `src/daemon/executor.h` | `t_executor` class; adapter used by `daemonizer` to create and run the daemon |
| `src/daemon/executor.cpp` | `t_executor` implementation; creates `t_daemon` instance, dispatches interactive/non-interactive run |
| `src/daemon/core.h` | `t_core` wrapper around `cryptonote::core`; initializes and deinitializes the core |
| `src/daemon/p2p.h` | `t_p2p` wrapper around `nodetool::node_server`; initializes and runs the P2P net loop |
| `src/daemon/protocol.h` | `t_protocol` wrapper around `t_cryptonote_protocol_handler`; manages protocol init and P2P endpoint binding |
| `src/daemon/rpc.h` | `t_rpc` wrapper around `core_rpc_server`; initializes, runs, and stops the HTTP RPC server |
| `src/simplewallet/simplewallet.h` | `simple_wallet` class declaration; implements `i_wallet2_callback`, declares 90+ command handlers |
| `src/simplewallet/simplewallet.cpp` | `simple_wallet` implementation (~10,500 lines); registers commands, wallet init/open/create, full interactive shell |
| `src/daemonizer/daemonizer.h` | Platform-independent daemonization interface; defines `daemonize()` and `arg_non_interactive` |

## Daemon

### Startup Sequence

The daemon startup flows through these stages:

1. **`main()` in `src/daemon/main.cpp`**: Calls `tools::on_startup()`, builds three option groups (visible, hidden, core_settings), and parses the command line with Boost.Program_options. Handles `--help` and `--version` immediately.

2. **Config file loading**: If a config file exists (default `~/.bitmonero/bitmonero.conf`, adjusted for testnet/stagenet), it is parsed. Only `core_settings` options are permitted in the config file; options restricted to the command line produce an explicit error.

3. **Network selection**: Reads `--testnet`, `--stagenet`, and `--regtest` flags. At most one may be specified.

4. **Data directory and logging**: Resolves the data directory (default `~/.bitmonero/`), configures log file path, rotation size, and log level.

5. **One-shot command mode**: If positional arguments are present (e.g., `monerod status`), the daemon creates a `t_command_server` connected via RPC to an already-running daemon instance, dispatches the command via `process_command_vec()`, and exits. RPC login credentials can come from `--rpc-login` or the `RPC_LOGIN` environment variable.

6. **Daemonization**: If no positional command is given, calls `daemonizer::daemonize()` which, depending on options:
   - **Interactive mode** (default on POSIX, or when `--non-interactive` is absent): Calls `t_executor::run_interactive()` which creates a `t_daemon` and calls `t_daemon::run(true)`.
   - **Non-interactive mode** (`--non-interactive` or `--detach`): Calls `t_executor::run_non_interactive()` which creates a `t_daemon` and calls `t_daemon::run(false)`.

7. **`t_daemon` constructor** (via `t_internals`): Initializes components in strict order:
   - `t_core`: Initializes `cryptonote::core` with checkpoints and proxy/DNS settings.
   - `t_protocol`: Initializes `t_cryptonote_protocol_handler` with the core reference.
   - `t_p2p`: Initializes `nodetool::node_server` with the protocol handler.
   - Resolves circular dependencies: `protocol.set_p2p_endpoint(p2p)` and `core.set_protocol(protocol)`.
   - `t_rpc` (one or two instances): Creates the main RPC server and optionally a restricted RPC server if `--rpc-restricted-bind-port` is specified.
   - ZMQ server: Unless `--no-zmq` is specified, initializes `ZmqServer` with `DaemonHandler`, optionally sets up ZMQ pub sockets for txpool, chain, and miner notifications.

8. **`t_daemon::run()`**: Installs a signal handler (SIGINT/SIGTERM triggers shutdown), then:
   - Runs `core.run()`.
   - Starts all RPC servers (each with 2 threads).
   - If interactive: creates a `t_command_server` bound directly to the first RPC server instance (no network hop) and starts the console handler with `start_handling()`.
   - Starts the ZMQ server if enabled.
   - Advertises the public RPC port via P2P if `--public-node` is configured.
   - Calls `p2p.run()` which blocks until P2P shutdown signal.
   - On shutdown: stops command handling, ZMQ, and all RPC servers.

### Command Loop

In interactive mode, the daemon provides a command prompt powered by `epee::console_handlers_binder`. The `t_command_server` constructor registers all commands with their handler functions, usage strings, and descriptions. The command dispatch chain is:

```
console input -> t_command_server (m_command_lookup)
             -> t_command_parser_executor (parse args)
             -> t_rpc_command_executor (execute via RPC or direct server call)
```

When operating in interactive mode locally, `t_rpc_command_executor` uses a direct pointer to `core_rpc_server` (bypassing the network). When used for one-shot commands (`monerod <command>`), it connects via HTTP RPC to the running daemon.

#### Available Interactive Commands

**Blockchain & Status:**

| Command | Usage | Description |
|---------|-------|-------------|
| `help` | `help [<command>]` | Show help section or documentation about a command |
| `apropos` | `apropos <keyword> [<keyword> ...]` | Search all command descriptions for keyword(s) |
| `status` | `status` | Show the current daemon status |
| `print_height` | `print_height` | Print the local blockchain height |
| `print_bc` | `print_bc <begin_height> [<end_height>]` | Print blockchain info in a given block range |
| `print_block` | `print_block <block_hash> \| <block_height>` | Print a given block |
| `print_tx` | `print_tx <transaction_hash> [+hex] [+json]` | Print a given transaction |
| `is_key_image_spent` | `is_key_image_spent <key_image>` | Check whether a key image is in the spent set |
| `diff` | `diff` | Show the current difficulty |
| `bc_dyn_stats` | `bc_dyn_stats <last_block_count>` | Print blockchain dynamic state info |
| `alt_chain_info` | `alt_chain_info [blockhash]` | Print information about alternative chains |
| `hard_fork_info` | `hard_fork_info <version>` | Print hard fork voting information |
| `version` | `version` | Print version information |
| `print_coinbase_tx_sum` | `print_coinbase_tx_sum <start_height> [<block_count>]` | Print sum of coinbase transactions |
| `output_histogram` | `output_histogram [@<amount>] <min_count> [<max_count>]` | Print output histogram |
| `print_status` | `print_status` | Print the current daemon status (brief) |

**Transaction Pool:**

| Command | Usage | Description |
|---------|-------|-------------|
| `print_pool` | `print_pool` | Print the transaction pool (long format) |
| `print_pool_sh` | `print_pool_sh` | Print the transaction pool (short format) |
| `print_pool_stats` | `print_pool_stats` | Print the transaction pool statistics |
| `flush_txpool` | `flush_txpool [<txid>]` | Flush a transaction or the whole pool |
| `relay_tx` | `relay_tx <txid>` | Relay a given transaction by txid |

**P2P Networking:**

| Command | Usage | Description |
|---------|-------|-------------|
| `print_pl` | `print_pl [white] [gray] [pruned] [publicrpc] [<limit>]` | Print the current peer list |
| `print_pl_stats` | `print_pl_stats` | Print peer list statistics |
| `print_cn` | `print_cn` | Print current connections |
| `print_net_stats` | `print_net_stats` | Print network statistics |
| `limit` | `limit [<kB/s>]` | Get or set download and upload limit |
| `limit_up` | `limit_up [<kB/s>]` | Get or set upload limit |
| `limit_down` | `limit_down [<kB/s>]` | Get or set download limit |
| `out_peers` | `out_peers <max_number>` | Set max number of outgoing peers |
| `in_peers` | `in_peers <max_number>` | Set max number of incoming peers |
| `ban` | `ban [<IP>\|@<filename>] [<seconds>]` | Ban an IP or list of IPs from a file |
| `unban` | `unban <address>` | Unban a given IP |
| `bans` | `bans` | Show currently banned IPs |
| `banned` | `banned <address>` | Check whether an address is banned |
| `sync_info` | `sync_info` | Print blockchain sync state information |

**Mining:**

| Command | Usage | Description |
|---------|-------|-------------|
| `start_mining` | `start_mining <addr> [<threads>\|auto] [do_background_mining] [ignore_battery]` | Start mining for specified address |
| `stop_mining` | `stop_mining` | Stop mining |
| `mining_status` | `mining_status` | Show current mining status |
| `show_hr` | `show_hr` | Start showing hash rate |
| `hide_hr` | `hide_hr` | Stop showing hash rate |

**Maintenance & Management:**

| Command | Usage | Description |
|---------|-------|-------------|
| `save` | `save` | Save the blockchain |
| `set_log` | `set_log <level>\|<{+,-,}categories>` | Change log level/categories (0-4) |
| `update` | `update (check\|download)` | Check or download updates |
| `pop_blocks` | `pop_blocks <nblocks>` | Remove blocks from end of blockchain |
| `prune_blockchain` | `prune_blockchain [confirm]` | Prune the blockchain |
| `check_blockchain_pruning` | `check_blockchain_pruning` | Check the blockchain pruning |
| `set_bootstrap_daemon` | `set_bootstrap_daemon (auto\|none\|host[:port] [username] [password]) [proxy]` | Set bootstrap daemon for wallet syncing |
| `flush_cache` | `flush_cache [bad-txs] [bad-blocks]` | Flush specified cache(s) |
| `rpc_payments` | `rpc_payments` | Print information about RPC payments |
| `stop_daemon` / `exit` | `stop_daemon` | Stop the daemon |

### Command-Line Arguments

**General Options:**

| Argument | Default | Description |
|----------|---------|-------------|
| `--help` | - | Show help and exit |
| `--version` | - | Show version and exit |
| `--config-file` | `~/.bitmonero/bitmonero.conf` | Specify configuration file (adjusted for testnet/stagenet) |

**Logging:**

| Argument | Default | Description |
|----------|---------|-------------|
| `--log-file` | `<data-dir>/bitmonero.log` | Specify log file path |
| `--log-level` | `""` | Log level 0-4 or categories string |
| `--max-log-file-size` | `MAX_LOG_FILE_SIZE` | Maximum log file size in bytes |
| `--max-log-files` | `MAX_LOG_FILES` | Maximum number of rotated log files (0 = unlimited) |

**Network:**

| Argument | Default | Description |
|----------|---------|-------------|
| `--testnet` | `false` | Run on testnet |
| `--stagenet` | `false` | Run on stagenet |
| `--regtest` | `false` | Run in regression testing mode |
| `--proxy` | `""` | SOCKS5 proxy address (e.g., `127.0.0.1:9050`) |
| `--proxy-allow-dns-leaks` | `false` | Allow DNS leaks outside of proxy |
| `--public-node` | `false` | Advertise as public node; requires restricted RPC |
| `--non-interactive` | `false` | Run in non-interactive (headless) mode |

**ZMQ:**

| Argument | Default | Description |
|----------|---------|-------------|
| `--zmq-rpc-bind-ip` | `127.0.0.1` | IP for ZMQ RPC server |
| `--zmq-rpc-bind-port` | Network-dependent default | Port for ZMQ RPC server |
| `--zmq-pub` | - | Address(es) for ZMQ pub (tcp or ipc) |
| `--no-zmq` | `false` | Disable ZMQ RPC server entirely |

**Concurrency:**

| Argument | Default | Description |
|----------|---------|-------------|
| `--max-concurrency` | `0` (auto) | Max threads for parallel jobs |

## Simplewallet

### Startup Sequence

1. **`main()` in `src/simplewallet/simplewallet.cpp`**: Registers wallet-specific options (`--wallet-file`, `--generate-new-wallet`, `--generate-from-device`, `--generate-from-view-key`, `--generate-from-spend-key`, `--generate-from-keys`, `--generate-from-multisig-keys`, `--generate-from-json`, etc.) plus `wallet2` options. Passes to `wallet_args::main()` for combined parsing (including common wallet options like `--daemon-address`, `--password`, `--testnet`, etc.).

2. **`simple_wallet::init(vm)`**: Determines operation mode based on which `--generate-*` flag was provided (at most one allowed):
   - **No wallet argument**: Prompts user interactively for wallet file path and whether to open or create.
   - **`--generate-new-wallet`**: Creates new wallet with random keys or from seed (if `--restore-deterministic-wallet`).
   - **`--generate-from-view-key`**: Prompts for standard address and view secret key, creates watch-only wallet.
   - **`--generate-from-spend-key`**: Prompts for spend secret key, derives deterministic wallet.
   - **`--generate-from-keys`**: Prompts for address, spend key, and view key.
   - **`--generate-from-multisig-keys`**: Restores from multisig seed (with experimental multisig confirmation).
   - **`--generate-from-device`**: Creates wallet backed by hardware device.
   - **`--generate-from-json`**: Creates wallet from JSON description file.
   - **`--wallet-file`**: Opens existing wallet file.

3. **Wallet open/create**: All paths converge to either `new_wallet()` (multiple overloads) or `open_wallet()`. Password is prompted. Network type (mainnet/testnet/stagenet) is validated. Seed language is chosen if applicable.

4. **One-shot command mode**: If positional arguments are given (e.g., `monero-wallet-cli --wallet-file=w balance`), the wallet executes `process_command()` on those arguments, then calls `stop()` and `deinit()` and exits.

5. **Interactive mode**: `simple_wallet::run()`:
   - Calls `try_connect_to_daemon()` to verify daemon connectivity.
   - Calls `refresh_main(0, ResetNone, true)` for initial blockchain sync.
   - Enables auto-refresh and starts the background idle thread (`wallet_idle_thread()`) which periodically checks for inactivity lock, refresh, and MMS messages.
   - Enters the console handler loop via `m_cmd_binder.run_handling()` with a dynamic prompt.

### Interactive Commands

The simplewallet registers 90+ commands in its constructor. Commands are grouped by category below.

**Wallet Management:**

| Command | Usage |
|---------|-------|
| `save` | Save wallet data |
| `save_watch_only` | Save a watch-only keys file |
| `password` | Change the wallet's password |
| `viewkey` | Display the private view key |
| `spendkey` | Display the private spend key |
| `seed` | Display the Electrum-style mnemonic seed |
| `encrypted_seed` | Display the encrypted mnemonic seed |
| `restore_height` | Display the restore height |
| `seed_set_language` | (via `set`) Set the wallet's seed language |
| `set` | `set <option> [<value>]` -- configure wallet settings (30+ options) |
| `wallet_info` | Show wallet information |
| `status` | Show wallet status |
| `lock` | Lock the wallet console |

**Balance & Transfers:**

| Command | Usage |
|---------|-------|
| `balance` | `balance [detail]` |
| `incoming_transfers` | `incoming_transfers [available\|unavailable] [verbose] [uses] [index=...]` |
| `payments` | `payments <PID_1> [<PID_2> ...]` |
| `bc_height` | Show blockchain height |
| `transfer` | `transfer [index=...] [<priority>] [<ring_size>] (<URI>\|<address> <amount>) [subtractfeefrom=...] [<payment_id>]` |
| `sweep_all` | `sweep_all [index=...] [<priority>] [<ring_size>] [outputs=<N>] <address>` |
| `sweep_account` | `sweep_account <account> [index=...] [<priority>] [<ring_size>] [outputs=<N>] <address>` |
| `sweep_below` | `sweep_below <amount_threshold> [index=...] [<priority>] [<ring_size>] <address>` |
| `sweep_single` | `sweep_single [<priority>] [<ring_size>] [outputs=<N>] <key_image> <address>` |
| `sweep_unmixable` | Send all unmixable outputs to yourself |
| `donate` | `donate [index=...] [<priority>] [<ring_size>] <amount>` |
| `sign_transfer` | `sign_transfer [export_raw] [<filename>]` |
| `submit_transfer` | Submit a signed transaction from a file |
| `show_transfers` | `show_transfers [in\|out\|all\|pending\|failed\|pool\|coinbase] [index=...] [<min_height> [<max_height>]]` |
| `export_transfers` | Export transfers to CSV |
| `show_transfer` | `show_transfer <txid>` |
| `unspent_outputs` | `unspent_outputs [index=...] [<min_amount> [<max_amount>]]` |

**Account & Address Management:**

| Command | Usage |
|---------|-------|
| `account` | `account [new <label>\|switch <index>\|label <index> <label>\|tag ...\|untag ...\|tag_description ...]` |
| `address` | `address [new <label>\|mnew <count>\|all\|<index>\|label <index> <label>\|one-off <acct> <sub>]` |
| `integrated_address` | `integrated_address [device] [<payment_id>\|<address>]` |
| `address_book` | `address_book [(add <addr> [<desc>])\|(delete <index>)]` |

**Transaction Proofs & Verification:**

| Command | Usage |
|---------|-------|
| `get_tx_key` | `get_tx_key <txid>` |
| `set_tx_key` | `set_tx_key <txid> <tx_key> [<subaddress>]` |
| `check_tx_key` | `check_tx_key <txid> <txkey> <address>` |
| `get_tx_proof` | `get_tx_proof <txid> <address> [<message>]` |
| `check_tx_proof` | `check_tx_proof <txid> <address> <signature_file> [<message>]` |
| `get_spend_proof` | `get_spend_proof <txid> [<message>]` |
| `check_spend_proof` | `check_spend_proof <txid> <signature_file> [<message>]` |
| `get_reserve_proof` | `get_reserve_proof (all\|<amount>) [<message>]` |
| `check_reserve_proof` | `check_reserve_proof <address> <signature_file> [<message>]` |

**Transaction Notes:**

| Command | Usage |
|---------|-------|
| `set_tx_note` | `set_tx_note <txid> [free text note]` |
| `get_tx_note` | `get_tx_note <txid>` |
| `set_description` | `set_description [free text note]` |
| `get_description` | Get wallet description |

**Key Image / Output Management:**

| Command | Usage |
|---------|-------|
| `export_key_images` | `export_key_images [all] <filename>` |
| `import_key_images` | `import_key_images <filename>` |
| `export_outputs` | `export_outputs [all] <filename>` |
| `import_outputs` | `import_outputs <filename>` |
| `mark_output_spent` | `mark_output_spent <amount>/<offset> \| <filename> [add]` |
| `mark_output_unspent` | `mark_output_unspent <amount>/<offset>` |
| `is_output_spent` | `is_output_spent <amount>/<offset>` |
| `freeze` | `freeze <key_image>` |
| `thaw` | `thaw <key_image>` |
| `frozen` | `frozen <key_image>` |

**Ring Management:**

| Command | Usage |
|---------|-------|
| `print_ring` | `print_ring <key_image> \| <txid>` |
| `set_ring` | `set_ring <filename> \| ( <key_image> absolute\|relative <index> [...] )` |
| `unset_ring` | `unset_ring <txid> \| ( <key_image> [...] )` |
| `save_known_rings` | Save known rings to the shared database |

**Mining:**

| Command | Usage |
|---------|-------|
| `start_mining` | `start_mining [<number_of_threads>] [bg_mining] [ignore_battery]` |
| `stop_mining` | Stop mining in the daemon |

**Multisig:**

| Command | Usage |
|---------|-------|
| `prepare_multisig` | Export data needed to create a multisig wallet |
| `make_multisig` | `make_multisig <threshold> <string1> [<string>...]` |
| `exchange_multisig_keys` | `exchange_multisig_keys [force-update-use-with-caution] <string> [<string>...]` |
| `export_multisig_info` | `export_multisig_info <filename>` |
| `import_multisig_info` | `import_multisig_info <filename> [<filename>...]` |
| `sign_multisig` | `sign_multisig <filename>` |
| `submit_multisig` | `submit_multisig <filename>` |
| `export_raw_multisig_tx` | `export_raw_multisig_tx <filename>` |

**MMS (Multisig Messaging System):**

| Command | Usage |
|---------|-------|
| `mms` | `mms [<subcommand> [<parameters>]]` |
| `mms init` | `mms init <required>/<authorized> <label> <transport_addr>` |
| `mms info` | Display MMS configuration |
| `mms signer` | `mms signer [<number> <label> [<transport_address> [<monero_address>]]]` |
| `mms list` | List all messages |
| `mms next` | `mms next [sync]` |
| `mms sync` | Force generation of multisig sync info |
| `mms transfer` | `mms transfer <transfer_args>` |
| `mms delete` | `mms delete (<message_id> \| all)` |
| `mms send` | `mms send [<message_id>]` |
| `mms receive` | Check for new messages |
| `mms export` | `mms export <message_id>` |
| `mms note` | `mms note [<label> <text>]` |
| `mms show` | `mms show <message_id>` |
| `mms set` | `mms set <option_name> [<option_value>]` |
| `mms send_signer_config` | Send signer config to all authorized signers |
| `mms start_auto_config` | `mms start_auto_config [<label> <label> ...]` |
| `mms config_checksum` | Get checksum for configuration verification |
| `mms stop_auto_config` | Delete auto-config tokens and abort |
| `mms auto_config` | `mms auto_config <auto_config_token>` |

**Miscellaneous:**

| Command | Usage |
|---------|-------|
| `set_daemon` | `set_daemon <host>[:<port>] [trusted\|untrusted\|this-is-probably-a-spy-node]` |
| `save_bc` | Save the current blockchain data |
| `refresh` | Synchronize transactions and balance |
| `rescan_spent` | Rescan blockchain for spent outputs |
| `rescan_bc` | `rescan_bc [hard\|soft\|keep_ki] [start_height=0]` |
| `set_log` | `set_log <level>\|{+,-,}<categories>` |
| `payment_id` | Generate a new random payment ID (obsolete) |
| `fee` | Print fee and transaction backlog info |
| `sign` | `sign [<account_index>,<address_index>] [--spend\|--view] <filename>` |
| `verify` | `verify <filename> <address> <signature>` |
| `net_stats` | Print simple network stats |
| `public_nodes` | List known public nodes |
| `welcome` | Print basic info for first-time users |
| `version` | Print version information |
| `show_qr_code` | `show_qr_code [<subaddress_index>]` |
| `scan_tx` | `scan_tx <txid> [<txid> ...]` |
| `hw_key_images_sync` | Synchronize key images with hardware wallet |
| `hw_reconnect` | Attempt to reconnect hardware wallet |
| `help` | `help [<command> \| all]` |
| `apropos` | `apropos <keyword> [<keyword> ...]` |

### Command-Line Arguments

**Wallet Selection (mutually exclusive):**

| Argument | Description |
|----------|-------------|
| `--wallet-file=<path>` | Open existing wallet file |
| `--generate-new-wallet=<path>` | Generate new wallet and save to path |
| `--generate-from-device=<path>` | Generate new wallet from hardware device |
| `--generate-from-view-key=<path>` | Generate watch-only wallet from view key |
| `--generate-from-spend-key=<path>` | Generate deterministic wallet from spend key |
| `--generate-from-keys=<path>` | Generate wallet from private keys |
| `--generate-from-multisig-keys=<path>` | Generate master wallet from multisig keys |
| `--generate-from-json=<path>` | Generate wallet from JSON file |

**Recovery:**

| Argument | Default | Description |
|----------|---------|-------------|
| `--restore-deterministic-wallet` | `false` | Recover wallet using Electrum-style mnemonic seed |
| `--restore-from-seed` | `false` | Alias for `--restore-deterministic-wallet` |
| `--restore-multisig-wallet` | `false` | Recover multisig wallet from seed |
| `--electrum-seed=<words>` | `""` | Electrum seed for wallet recovery |
| `--restore-height=<n>` | `0` | Restore from specific blockchain height |
| `--restore-date=<date>` | `""` | Restore from estimated height on specified date |

**Other:**

| Argument | Default | Description |
|----------|---------|-------------|
| `--mnemonic-language=<lang>` | `""` | Language for mnemonic seed |
| `--non-deterministic` | `false` | Generate non-deterministic view and spend keys |
| `--do-not-relay` | `false` | Created transactions will not be relayed |
| `--create-address-file` | `false` | Create an address file for new wallets |
| `--subaddress-lookahead=<M:N>` | `""` | Set subaddress lookahead sizes |
| `--use-english-language-names` | `false` | Display English language names |

Additionally, `wallet2::init_options()` contributes numerous options including `--daemon-address`, `--daemon-host`, `--daemon-port`, `--password`, `--password-file`, `--testnet`, `--stagenet`, `--offline`, `--trusted-daemon`, `--untrusted-daemon`, `--daemon-ssl`, and various RPC/SSL-related options.

## Dependencies

**Daemon (`monerod`):**
- `cryptonote_core` -- blockchain storage, transaction validation, core logic
- `cryptonote_protocol` -- block/transaction relay protocol handler
- `p2p` (`nodetool::node_server`) -- peer-to-peer networking
- `rpc` (`core_rpc_server`) -- HTTP JSON-RPC and binary RPC server
- `rpc` (`ZmqServer`, `DaemonHandler`) -- ZeroMQ RPC and pub/sub
- `daemonizer` -- platform-specific daemonization (POSIX fork or Windows service)
- `epee` -- console handler, string tools, HTTP server framework, net utilities
- `blocks` -- embedded checkpoint data (when `PER_BLOCK_CHECKPOINT` is defined)
- Boost (program_options, filesystem, thread, algorithm)

**Simplewallet (`monero-wallet-cli`):**
- `wallet2` -- core wallet logic (key management, transaction construction, daemon communication)
- `mnemonics` (`ElectrumWords`) -- seed phrase generation and parsing
- `rpc_client` -- HTTP RPC client for daemon communication
- `cryptonote_basic` -- address parsing, transaction types, format utilities
- `ringct` -- Ring CT signature library
- `multisig` -- multisig key exchange and signing
- `message_store` -- MMS (Multisig Messaging System)
- `wallet_args` -- shared wallet argument parsing
- `QrCode` -- QR code generation for address display
- `epee` -- console handler, string tools, HTTP client
- Boost (program_options, filesystem, regex, format, lexical_cast, locale)
- RapidJSON -- JSON parsing for `--generate-from-json`

## Known Issues

The following `TODO`, `FIXME`, and `HACK` comments were found in the daemon source files:

- `src/daemon/main.cpp:129` -- `TODO parse the debug options like set log level right here at start`
- `src/daemon/main.cpp:267` -- `FIXME: not sure on windows implementation default, needs further review` (regarding `relative_path_base` on Windows)
- `src/daemon/core.h:53-55` -- `TEMPORARY HACK - Yes, this creates a copy, but otherwise the original variable map could go out of scope before the run method is called` (the `m_vm_HACK` member that copies the entire `variables_map`)
- `src/daemon/core.h:84` -- `TODO - get rid of circular dependencies in internals` (the `set_protocol` method exists because core and protocol have circular references)
- `src/daemon/rpc_command_executor.cpp:1420` -- `TODO - this is only temporary! Get rid of hard-coded constants!` (inside RPC command execution)
