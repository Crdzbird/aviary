# Atlas Settlement Network

A real-world demo: a **wholesale settlement & tokenized-asset network** built on
Blockchainer, with a **Go gRPC server** and a **Flutter operator console**.

```
Flutter console  ──gRPC──▶  Go server  ──cgo──▶  Blockchainer (C++ core)
```

The Flutter app is a thin gRPC client (no native blockchain code), so it runs on
any platform grpc-dart supports. All chain work happens server-side.

## What it demonstrates (real use cases, not toy transfers)

- **Custom settlement chains** — launch a BFT, instant-finality rail per currency
  (e.g. "EURx"), with a genesis reserve treasury.
- **Institutional custodial wallets** — each institution gets a wallet with a
  settlement-chain balance **and** cross-chain receive addresses
  (Ethereum / Bitcoin / Solana) for bridging external assets in.
- **Issuance & settlement with finality** — issue reserves to institutions,
  settle institution-to-institution; transactions batch in the mempool and
  finalize atomically when a block is produced (BFT).
- **Cryptographic auditability** — get a **light-client balance proof** for any
  wallet, re-verified against the chain's state root.
- **EVM smart contracts** — build + **sign a real ERC-20 transfer** (e.g. USDC)
  with a wallet's secp256k1 key, producing a raw tx ready for
  `eth_sendRawTransaction`.
- **Live ledger feed** — a server-streaming RPC pushes admissions / finalizations
  to the console in real time.

## Run

```bash
# 1. Build the blockchainer host library
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Start the server + console (macOS desktop, iOS sim, or Android)
./run.sh                 # defaults to -d macos
./run.sh -d <device-id>  # e.g. an iOS simulator (reaches the host's localhost)
```

Or separately: `( cd server && go run . )` then `( cd app && flutter run )`.

## API

`proto/atlas.proto` — `Atlas` service: CreateChain, CreateWallet, IssueFunds,
Settle, ProduceBlock, GetBalanceProof, BuildErc20Transfer, FunctionSelector,
GetMetrics, and `StreamEvents` (server-streaming). The server (`server/`) uses the
Blockchainer Go binding; the client (`app/`) uses generated grpc-dart stubs.
