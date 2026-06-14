# Blockchainer demo (Flutter)

A desktop Flutter dashboard that drives the **entire** Blockchainer stack through
the C ABI — one screen showing every capability live:

- **Chain creation** — an in-memory single-validator account chain; funds Alice.
- **Transactions & consensus** — send Alice → Bob, then produce + finalize a block
  (BFT instant finality). "Auto-mine" finalizes blocks as transactions arrive.
- **Live event stream** — BlockFinalized / TxAdmitted / TxRemoved events streamed
  off the chain's event bus as they happen.
- **Observability** — Prometheus metrics (height, blocks, transactions, mempool)
  updating live.
- **Verification** — a light-client state-inclusion proof for Alice, verified
  against the chain's state root.
- **Multi-chain interop** — Ethereum, Bitcoin, and Solana addresses derived from
  keys by the in-process interop layer.

The Flutter UI calls the high-level Dart binding
(`package:blockchainer_dart/blockchainer.dart`), which marshals over the C ABI
(`libblockchainer_c`) into the C++ core.

## Run it

1. Build the native library from the repo root:
   `cd ../.. && cmake -S . -B build && cmake --build build`
   (Prereq: cryptolib's `cryptolib_c` built — see the top-level README.)
2. Run on macOS desktop:
   `flutter pub get && flutter run -d macos`

The app finds `libblockchainer_c.dylib` in the repo `build/` dir, or set
`BLOCKCHAINER_LIB`. The macOS sandbox is disabled here so the dev dylib loads by
path; a production app would bundle the binary as a plugin.

## How it maps to the library

| UI section | Library layer |
|---|---|
| Chain / accounts | DemoNode → Ledger + account StateMachine |
| Produce block | BftEngine (BLS-aggregated commit) |
| Live events | observe::EventBus |
| Metrics | observe::MetricsRegistry (Prometheus) |
| Prove & verify | prove_account + verify_state_proof (light client) |
| Interop addresses | interop::evm / bitcoin / solana |
