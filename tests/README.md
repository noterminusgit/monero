# Running all tests

To run all tests, run:

```bash
cd /path/to/monero
make [-jn] debug-test # where n is number of compiler processes
```

To test a release build, replace `debug-test` with `release-test` in the previous command.

# Core tests

Core tests take longer than any other Monero tests, due to the high amount of computational work involved in validating core components.

Tests are located in `tests/core_tests/`, and follow a straightforward naming convention. Most cases cover core functionality (`block_reward.cpp`, `chaingen.cpp`, `rct.cpp`, etc.), while some cover basic security tests (`double_spend.cpp` & `integer_overflow.cpp`).

To run only Monero's core tests (after building):

```bash
cd build/debug/tests/core_tests
ctest
```

To run the same tests on a release build, replace `debug` with `release`.


# Crypto Tests

Crypto tests are located under the `tests/crypto` directory.

- `crypto-tests.h` contains test harness headers
- `main.cpp` implements the driver for the crypto tests

Tests correspond to components under `src/crypto/`. A quick comparison reveals the pattern, and new tests should continue the naming convention.

To run only Monero's crypto tests (after building):

```bash
cd build/debug/tests/crypto
ctest
```

To run the same tests on a release build, replace `debug` with `release`.

# Functional tests

Functional tests validate Monero daemon and wallet RPC interfaces and network behavior through end-to-end testing. They are located under the `tests/functional_tests` directory and are orchestrated by the test runner `functional_tests_rpc.py`, which spawns daemon and wallet processes automatically.

## Test modules

The 37 Python test modules cover the following areas:

- **Blockchain & Consensus** — `blockchain.py`, `block_template.py`, `chain_reorg.py`, `pruning.py`, `txpool.py`
- **Wallet Operations** — `wallet.py`, `wallet_accounts.py`, `wallet_daemon_switching.py`, `transfer.py`, `sweep_operations.py`
- **Transactions & Cryptography** — `tx_lifecycle.py`, `cold_signing.py`, `cold_signing_extended.py`, `proofs.py`, `sign_message.py`
- **Privacy** — `k_anonymity.py`, `key_image_output_queries.py`
- **P2P Networking** — `p2p.py`, `p2p_extended.py`, `bans.py`
- **Address & URI** — `address_book.py`, `integrated_address.py`, `validate_address.py`, `uri.py`
- **RPC & Access Control** — `daemon_info.py`, `daemon_state.py`, `rpc_payment.py`, `rpc_access_control.py`, `rpc_error_handling.py`, `http_digest_auth.py`
- **Mining & Distribution** — `mining.py`, `get_output_distribution.py`, `speed.py`
- **Sync & Events** — `background_sync_extended.py`, `zmq_events.py`
- **Multisig & Regression** — `multisig.py`, `bug_verification.py`

Tests use a Python RPC client framework in `utils/python-rpc/framework/` providing `daemon.py`, `wallet.py`, `zmq.py`, and `rpc.py`.

## Setup

Building all the tests requires installing the following dependencies:
```bash
pip install requests psutil monotonic zmq deepdiff
```

The test runner automatically configures 5 daemon instances and 7 wallet instances in regtest mode with fixed difficulty.

For manual execution, run a regtest daemon in the offline mode and with a fixed difficulty:
```bash
monerod --regtest --offline --fixed-difficulty 1
```
Alternatively, you can run multiple daemons and let them connect with each other by using `--add-exclusive-node`. In this case, make sure that the same fixed difficulty is given to all the daemons.

Next, restore a mainnet wallet with the following seed and restore height 0 (the file path doesn't matter):
```bash
velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted
```

Open the wallet file with `monero-wallet-rpc` with RPC port 18083. Finally, start tests by invoking ./blockchain.py or ./speed.py

## Running

```bash
cd build/debug
ctest -V -R functional_tests_rpc
```

## Parameters

Configuration of individual tests.

### Mining test

The following environment variables may be set to control the mining test:

- `MINING_NO_MEASUREMENT` - set to anything to use large enough and fixed mining timeouts (use case: very slow PCs and no intention to change the mining code)
- `MINING_SILENT`         - set to anything to disable mining logging

For example, to customize the run of the functional tests, you may run the following commands from the build directory:

```bash
export MINING_NO_MEASUREMENT=1
ctest -V -R functional_tests_rpc
unset MINING_NO_MEASUREMENT
```

# Fuzz tests

Fuzz tests are written using American Fuzzy Lop (AFL), and located under the `tests/fuzz` directory.

An additional helper utility is provided `contrib/fuzz_testing/fuzz.sh`. AFL must be installed, and some additional setup may be necessary for the script to run properly.

## OSS-Fuzz

Monero is integrated into [OSS-Fuzz](https://github.com/google/oss-fuzz) and the project integration
is available [here](https://github.com/google/oss-fuzz/tree/master/projects/monero). OSS-Fuzz builds
and runs the fuzzers continuously, so long as Monero's OSS-Fuzz [build script](https://github.com/google/oss-fuzz/blob/master/projects/monero/build.sh) builds them.

Issues found by OSS-Fuzz are publicly available (following a disclosure deadline) on the OSS-Fuzz issue tracker [here](https://issues.oss-fuzz.com/issues?q=project%3Dmonero).
The issue tracker only displays limited information, and only maintainers with emails listed in the [project.yaml](https://github.com/google/oss-fuzz/blob/master/projects/monero/project.yaml) have access to full details.

Coverage reports are built on a daily basis and data about this can be found at [introspector.oss-fuzz.com](https://introspector.oss-fuzz.com) [here](https://introspector.oss-fuzz.com/project-profile?project=monero).

### Build and run fuzzers by way of OSS-Fuzz

**Building Monero's fuzzers with OSS-Fuzz**

```sh
$ git clone https://github.com/google/oss-fuzz
$ cd oss-fuzz
$ python3 infra/helper.py build_fuzzers monero

# Display what was build
$ ls build/out/monero/
base58_fuzz_tests                       cold-outputs_fuzz_tests_seed_corpus.zip      llvm-symbolizer                              signature_fuzz_tests
base58_fuzz_tests_seed_corpus.zip       cold-transaction_fuzz_tests                  load-from-binary_fuzz_tests                  signature_fuzz_tests_seed_corpus.zip
block_fuzz_tests                        cold-transaction_fuzz_tests_seed_corpus.zip  load-from-binary_fuzz_tests_seed_corpus.zip  transaction_fuzz_tests
block_fuzz_tests_seed_corpus.zip        http-client_fuzz_tests                       load-from-json_fuzz_tests                    transaction_fuzz_tests_seed_corpus.zip
bulletproof_fuzz_tests                  http-client_fuzz_tests_seed_corpus.zip       load-from-json_fuzz_tests_seed_corpus.zip    tx-extra_fuzz_tests
bulletproof_fuzz_tests_seed_corpus.zip  levin_fuzz_tests                             parse-url_fuzz_tests                         tx-extra_fuzz_tests_seed_corpus.zip
cold-outputs_fuzz_tests                 levin_fuzz_tests_seed_corpus.zip             parse-url_fuzz_tests_seed_corpus.zip
```

**Run fuzzing harness with OSS-Fuzz**

Assuming you performed the above steps for building the fuzzers and are in the OSS-Fuzz root directory:

```sh
$ python3 infra/helper.py run_fuzzer monero base58_fuzz_tests
...
...
INFO: Loaded 1 modules   (9075 inline 8-bit counters): 9075 [0x55d1c3d6cfd8, 0x55d1c3d6f34b),
INFO: Loaded 1 PC tables (9075 PCs): 9075 [0x55d1c3d6f350,0x55d1c3d92a80),
INFO:        1 files found in /tmp/base58_fuzz_tests_corpus
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: seed corpus: files: 1 min: 95b max: 95b total: 95b rss: 33Mb
#2      INITED cov: 18 ft: 19 corp: 1/95b exec/s: 0 rss: 33Mb
#3      NEW    cov: 19 ft: 23 corp: 2/190b lim: 95 exec/s: 0 rss: 34Mb L: 95/95 MS: 1 ChangeByte-
#4      NEW    cov: 20 ft: 24 corp: 3/285b lim: 95 exec/s: 0 rss: 34Mb L: 95/95 MS: 1 ChangeByte-
#5      NEW    cov: 22 ft: 26 corp: 4/359b lim: 95 exec/s: 0 rss: 34Mb L: 74/95 MS: 1 EraseBytes-
#6      NEW    cov: 23 ft: 29 corp: 5/454b lim: 95 exec/s: 0 rss: 34Mb L: 95/95 MS: 1 ChangeByte-
#8      NEW    cov: 24 ft: 30 corp: 6/549b lim: 95 exec/s: 0 rss: 34Mb L: 95/95 MS: 2 CrossOver-ChangeBit-
#12     NEW    cov: 25 ft: 35 corp: 7/606b lim: 95 exec/s: 0 rss: 34Mb L: 57/95 MS: 4 ChangeBinInt-ShuffleBytes-ShuffleBytes-EraseBytes-
#14     NEW    cov: 26 ft: 38 corp: 8/655b lim: 95 exec/s: 0 rss: 34Mb L: 49/95 MS: 2 ChangeBinInt-EraseBytes-
#17     NEW    cov: 27 ft: 40 corp: 9/708b lim: 95 exec/s: 0 rss: 34Mb L: 53/95 MS: 3 ChangeASCIIInt-ChangeBit-EraseBytes-
#18     NEW    cov: 28 ft: 41 corp: 10/803b lim: 95 exec/s: 0 rss: 34Mb L: 95/95 MS: 1 ChangeByte-
#20     NEW    cov: 28 ft: 42 corp: 11/852b lim: 95 exec/s: 0 rss: 34Mb L: 49/95 MS: 2 ChangeASCIIInt-ShuffleBytes-
#22     REDUCE cov: 28 ft: 42 corp: 11/847b lim: 95 exec/s: 0 rss: 34Mb L: 90/95 MS: 2 ChangeBinInt-CrossOver-
#25     NEW    cov: 29 ft: 47 corp: 12/942b lim: 95 exec/s: 0 rss: 34Mb L: 95/95 MS: 3 ChangeBit-ChangeBit-CopyPart-
#39     REDUCE cov: 29 ft: 47 corp: 12/941b lim: 95 exec/s: 0 rss: 34Mb L: 94/95 MS: 4 ChangeByte-CopyPart-ChangeASCIIInt-EraseBytes-
#41     NEW    cov: 30 ft: 48 corp: 13/991b lim: 95 exec/s: 0 rss: 34Mb L: 50/95 MS: 2 CopyPart-CrossOver-
#57     NEW    cov: 31 ft: 49 corp: 14/1068b lim: 95 exec/s: 0 rss: 34Mb L: 77/95 MS: 1 InsertRepeatedBytes-
#63     NEW    cov: 32 ft: 50 corp: 15/1147b lim: 95 exec/s: 0 rss: 34Mb L: 79/95 MS: 1 CrossOver-
...
```


# Hash tests

Hash tests exist under `tests/hash`, and include a set of target hashes in text files.

To run only Monero's hash tests (after building):

```bash
cd build/debug/tests/hash
ctest
```

To run the same tests on a release build, replace `debug` with `release`.

To run specific hash test, you can use `ctest` `-R` parameter. For example to run only `blake2b` hash tests:

```
ctest -R hash-blake2b
```

# Libwallet API tests

The libwallet API tests are integration tests for the Monero wallet C++ API (`wallet/api/wallet2_api.h`). They validate wallet functionality including creation, opening, balance queries, transaction sending, and payment ID handling against a private testnet.

Tests are located in `tests/libwallet_api_tests/`.

- **main.cpp** — 36 test cases organized into 4 fixtures:
  - `WalletManagerTest` (14 tests) — wallet creation, opening, password management, currency conversion
  - `WalletTest1` (13 tests) — balance, block height, refresh, transactions, history, payment IDs, priority
  - `WalletTest2` (5 tests) — callbacks: refresh, sent/received transaction callbacks, block notifications
  - `WalletManagerMainnetTest` (4 tests) — mainnet wallet operations

- **scripts/** — Helper scripts for testnet setup: `create_wallets.sh`, `send_funds.sh`, `mining_start.sh`, `mining_stop.sh`

### Prerequisites

- A running Monero daemon (default: `localhost:38081` for testnet, configurable via `TESTNET_DAEMON_ADDRESS`)
- Pre-generated test wallets in `/var/monero/testnet_pvt/` (configurable via `WALLETS_ROOT_DIR`)

To run only Monero's libwallet API tests (after building):

```bash
cd build/debug/tests/libwallet_api_tests
ctest
```

To run the same tests on a release build, replace `debug` with `release`.

# Net Load tests

Net load tests stress-test the Monero P2P network layer under high-load conditions using the Levin protocol. They are located in `tests/net_load_tests/`.

- **net_load_tests.h** — Shared definitions: Levin command handler, open/close test helper, command IDs
- **srv.cpp** — Test TCP server accepting connections on port 36231, handling statistics, closure, and data forwarding
- **clt.cpp** — Client test suite with 4 GTest-based scenarios:
  1. Large-scale opens (100k connections) + client-initiated closes
  2. Large-scale opens (100k connections) + server-initiated closes
  3. Persistent open/close cycling + client-initiated closes
  4. Persistent open/close cycling + server-initiated closes

To run, start the server first, then the client:

```bash
cd build/debug/tests/net_load_tests
./net_load_tests_srv   # in one terminal
./net_load_tests_clt   # in another terminal
```

# Performance tests

Performance tests are located in `tests/performance_tests`, and test features for performance metrics on the host machine.

To run only Monero's performance tests (after building):

```bash
cd build/debug/tests/performance_tests
./performance_tests
```

The path may be build/Linux/master/debug (adapt as necessary for your platform).

If the `performance_tests` binary does not exist, try running `make` in the `build/debug/tests/performance_tests` directory.

To run the same tests on a release build, replace `debug` with `release`.

# Unit tests

Unit tests are defined under the `tests/unit_tests` directory. Independent components are tested individually to ensure they work properly on their own.

To run only Monero's unit tests (after building):

```bash
cd build/debug/tests/unit_tests
ctest
```

To run the same tests on a release build, replace `debug` with `release`.

# Block Weight tests

Block weight tests validate the dynamic block weight limit calculation algorithm that prevents maximal block attacks. Tests verify adjustment of block weight limits based on the long-term block weight median over a 5000-block window.

Tests are located in `tests/block_weight/`.

- **block_weight.cpp** — C++ test harness simulating three scenarios using a synthetic test database: maximum weight blocks (`test_max`), pseudo-random variation via LCG (`test_lcg`), and minimum weight blocks (`test_min`)
- **block_weight.py** — Python reference implementation of the same three scenarios
- **compare.py** — Runs both implementations and compares output for consistency
- **CMakeLists.txt** — Registers a test that runs `compare.py` to validate both implementations produce identical results

To run only Monero's block weight tests (after building):

```bash
cd build/debug/tests/block_weight
ctest
```

To run the same tests on a release build, replace `debug` with `release`.

# Difficulty tests

Difficulty tests validate Monero's difficulty adjustment algorithm, which determines mining difficulty based on block timestamps and cumulative difficulties.

Tests are located in `tests/difficulty/`.

- **difficulty.cpp** — Test executable validating both 64-bit and wide (arbitrary-precision) difficulty calculations against reference data
- **data.txt** — Pre-computed reference dataset (1000 blocks) for the 64-bit difficulty test
- **gen_wide_data.py** — Python implementation generating reference data for 100,000 blocks with extreme timing and difficulty variations
- **wide_difficulty.py** — Test runner for the wide difficulty test
- **generate-data** — Python data generator used by the build system

To run only Monero's difficulty tests (after building):

```bash
cd build/debug/tests/difficulty
ctest
```

To run a specific variant:

```bash
ctest -R "^difficulty$"       # 64-bit test
ctest -R "^wide_difficulty$"  # wide arithmetic test
```

# Trezor tests

Comprehensive integration tests for Trezor hardware wallet support, validating transaction signing, key image synchronization, wallet operations, and device interaction across multiple hardfork versions.

Tests are located in `tests/trezor/`.

- **trezor_tests.h** — 23 test generator classes, `gen_trezor_base` infrastructure, `tsx_builder` helper
- **trezor_tests.cpp** — Test logic: blockchain generation, transaction signing, wallet integration
- **daemon.h/cpp** — Mock in-process daemon
- **tools.h/cpp** — Configuration helpers

### Test scenarios (23 total)

- Key image sync (with/without refresh, live refresh)
- Transaction variations (1/4/16 UTXOs, 1-15 outputs, subaddresses, integrated addresses)
- Device features (passphrase, PIN, wallet-level encryption)
- RCT signature verification, fee/amount correctness, get-tx-key recovery

### Running

```bash
cd build/debug/tests/trezor
./trezor_tests                            # default run
./trezor_tests --filter "gen_trezor_4utxo" # filter by name
./trezor_tests --heavy-tests              # stress scenarios
```

Environment variables: `TEST_MIN_HF`, `TEST_MAX_HF`, `TEST_MINING_ENABLED`, `TEST_KI_SYNC`.

# Test data

The `tests/data/` directory stores fixture files and binary test data used across multiple test suites.

- **Wallet files** — Monero wallet + `.keys` pairs for testing serialization, encryption, password changes, and format conversions
- **Key encryption data** — Background wallet files for testing key encryption in wallet storage
- **Fuzz test corpora** — Binary seed inputs across 14 categories: `base58/`, `block/`, `bulletproof/`, `cold-outputs/`, `cold-transaction/`, `http-client/`, `levin/`, `load-from-binary/`, `load-from-json/`, `parse-url/`, `signature/`, `transaction/`, `tx-extra/`, `utf8/`
- **Hash test data** — SHA256 reference files in `sha256sum/`
- **Transaction data** — Serialized transaction binaries in `txs/`
- **Node config** — Banlist files in `node/`

# Writing new tests

## Test hygiene

When writing new tests, please implement all functions in `.cpp` or `.c` files, and only put function headers in `.h` files. This will help keep the fairly complex test suites somewhat sane going forward.
