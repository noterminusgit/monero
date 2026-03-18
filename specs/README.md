# Monero Codebase Specification

Comprehensive specification documentation for the Monero codebase, generated from source code analysis. These specs document existing logic, data structures, APIs, and known issues across all major modules.

**Total**: 21 spec files + [bugs.md](bugs.md) + this index = 23 files, ~10,500 lines.

> **Read-only documentation** — no source code was modified. For existing protocol docs, see `docs/` (LEVIN_PROTOCOL.md, ZMQ.md, PORTABLE_STORAGE.md, ANONYMITY_NETWORKS.md).

---

## Tier 1: Consensus & Core

These modules are security-critical and define Monero's consensus behavior.

| # | Spec | Description |
|---|------|-------------|
| 01 | [cryptonote-core](01-cryptonote-core.md) | Central coordinator: block/tx validation, blockchain state machine, miner integration |
| 02 | [blockchain-db](02-blockchain-db.md) | LMDB storage layer, BlockchainDB interface, batch operations, schema migrations |
| 03 | [tx-pool](03-tx-pool.md) | Transaction pool management, relay, pruning, Dandelion++ state machine |
| 04 | [difficulty](04-difficulty.md) | Difficulty calculation algorithms (v1 windowed average, 64/128-bit hash checks) |
| 05 | [consensus-rules](05-consensus-rules.md) | Hardfork schedule (v1-v16), block rewards, emission curve, tx validation rules |

## Tier 2: Cryptographic Primitives

Low-level cryptography underpinning all of Monero's privacy features.

| # | Spec | Description |
|---|------|-------------|
| 06 | [ringct](06-ringct.md) | RingCT types, CLSAG, Bulletproofs, Bulletproofs+, proving & verification |
| 07 | [multisig](07-multisig.md) | M-of-N multisig protocol, DH key exchange rounds, MuSig2-style CLSAG signing |
| 08 | [crypto-primitives](08-crypto-primitives.md) | Ed25519 ops, key derivation, hash functions, CSPRNG, ChaCha encryption |

## Tier 3: Wallet

The wallet layer handles key management, transaction construction, and user-facing operations.

| # | Spec | Description |
|---|------|-------------|
| 09 | [wallet2](09-wallet2.md) | wallet2 class (~80 public methods), key management, refresh/sync, store/load |
| 10 | [wallet-transfers](10-wallet-transfers.md) | TX construction pipeline, input selection, fee calculation, ring member selection |
| 11 | [wallet-rpc](11-wallet-rpc.md) | wallet_rpc_server: 75+ RPC endpoints, authentication, command dispatch |
| 12 | [multisig-wallet](12-multisig-wallet.md) | Multisig wallet operations: KEX setup, info sync, signing flows |

## Tier 4: Network & P2P

Peer-to-peer networking, protocol handling, and RPC interfaces.

| # | Spec | Description |
|---|------|-------------|
| 13 | [p2p-protocol](13-p2p-protocol.md) | Peer discovery, handshake, block sync, ban scoring, Dandelion++ relay |
| 14 | [levin-protocol](14-levin-protocol.md) | Levin framing, async handler, fragment reassembly (extends docs/LEVIN_PROTOCOL.md) |
| 15 | [rpc-server](15-rpc-server.md) | core_rpc_server: 60+ endpoints, JSON-RPC/binary, restricted mode, RPC payments |
| 16 | [zmq-rpc](16-zmq-rpc.md) | ZMQ pub/sub, REP socket, 5 topic types (extends docs/ZMQ.md) |

## Tier 5: Infrastructure & Utilities

Supporting frameworks, serialization, mining, and CLI tools.

| # | Spec | Description |
|---|------|-------------|
| 17 | [epee-framework](17-epee-framework.md) | portable_storage, string_tools, HTTP server, net_utils, logging |
| 18 | [serialization](18-serialization.md) | Binary/JSON serialization, BEGIN_SERIALIZE macros, archive types |
| 19 | [mining](19-mining.md) | Embedded miner, block templates, RandomX integration, background mining |
| 20 | [net-anonymity](20-net-anonymity.md) | Tor/I2P support, Dandelion++ stem/fluff (extends docs/ANONYMITY_NETWORKS.md) |
| 21 | [daemon-cli](21-daemon-cli.md) | Daemon startup/commands (40+), simplewallet commands (90+) |

## Cross-Cutting

| File | Description |
|------|-------------|
| [bugs.md](bugs.md) | All discovered issues: 9 critical/security, 8 bugs, 100+ tech debt items, 23 design concerns |

---

## Module Dependency Graph

```
                    ┌─────────────┐
                    │   daemon    │ (21)
                    │ simplewallet│
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ RPC (15) │ │wallet2(9)│ │ P2P (13) │
        │ ZMQ (16) │ │xfer (10) │ │levin (14)│
        └────┬─────┘ │ rpc (11) │ └────┬─────┘
             │       │msig (12) │      │
             │       └────┬─────┘      │
             │            │            │
             └────────────┼────────────┘
                          ▼
                 ┌────────────────┐
                 │  core (01)     │
                 │  tx_pool (03)  │
                 │  mining (19)   │
                 └───────┬────────┘
                         │
              ┌──────────┼──────────┐
              ▼          ▼          ▼
        ┌──────────┐┌────────┐┌──────────┐
        │blockchain││consensu││ difficulty│
        │  db (02) ││rules(5)││   (04)   │
        └──────────┘└────────┘└──────────┘
                         │
              ┌──────────┼──────────┐
              ▼          ▼          ▼
        ┌──────────┐┌────────┐┌──────────┐
        │ringct (6)││msig (7)││crypto (8) │
        └──────────┘└────────┘└──────────┘
                         │
              ┌──────────┼──────────┐
              ▼          ▼          ▼
        ┌──────────┐┌────────┐┌──────────┐
        │epee (17) ││serial  ││anonymity │
        │          ││  (18)  ││  (20)    │
        └──────────┘└────────┘└──────────┘
```

---

## Related Resources

- [Test Coverage Plan](../plan/test-coverage-plan.md) — Multi-phase test coverage initiative (Phases 0-4 done, 5-7 remaining)
- [docs/LEVIN_PROTOCOL.md](../docs/LEVIN_PROTOCOL.md) — Levin wire protocol specification
- [docs/ZMQ.md](../docs/ZMQ.md) — ZMQ RPC protocol specification
- [docs/PORTABLE_STORAGE.md](../docs/PORTABLE_STORAGE.md) — Portable storage binary format
- [docs/ANONYMITY_NETWORKS.md](../docs/ANONYMITY_NETWORKS.md) — Tor/I2P setup and privacy analysis
