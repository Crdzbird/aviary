# Atlas Settlement Network

A real-world demo: a **wholesale settlement & tokenized-asset network** built on
Aviary, with a **Go gRPC server** and a **Flutter operator console**.

```
Flutter console  --gRPC-->  Go server  --cgo-->  Aviary (C++ core)
```

The Flutter app is a thin gRPC client (no native blockchain code), so it runs on
any platform grpc-dart supports. All chain work happens server-side.

## What it demonstrates (real use cases, not toy transfers)

- **Custom settlement chains** - launch a BFT, instant-finality rail per currency
  (e.g. "EURx"), with a genesis reserve treasury.
- **Institutional custodial wallets** - each institution gets a wallet with a
  settlement-chain balance **and** cross-chain receive addresses
  (Ethereum / Bitcoin / Solana) for bridging external assets in.
- **Issuance & settlement with finality** - issue reserves to institutions,
  settle institution-to-institution; transactions batch in the mempool and
  finalize atomically when a block is produced (BFT).
- **Cryptographic auditability** - get a **light-client balance proof** for any
  wallet, re-verified against the chain's state root.
- **EVM smart contracts** - build + **sign a real ERC-20 transfer** (e.g. USDC)
  with a wallet's secp256k1 key, producing a raw tx ready for
  `eth_sendRawTransaction`.
- **Live ledger feed** - a server-streaming RPC pushes admissions / finalizations
  to the console in real time.

## Run

```bash
# 1. Build the aviary host library
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Start the server + console (macOS desktop, iOS sim, or Android)
./run.sh                 # defaults to -d macos
./run.sh -d <device-id>  # e.g. an iOS simulator (reaches the host's localhost)
```

Or separately: `( cd server && go run . )` then `( cd app && flutter run )`.

## API

`proto/atlas.proto` - `Atlas` service: CreateChain, CreateWallet, IssueFunds,
Settle, ProduceBlock, GetBalanceProof, BuildErc20Transfer, FunctionSelector,
GetMetrics, and `StreamEvents` (server-streaming). The server (`server/`) uses the
Aviary Go binding; the client (`app/`) uses generated grpc-dart stubs.

## Governance tab (validator rotation + auditor light client)

The settlement rail is governed by a **validator council** - a BFT chain whose
validator set the operator rotates: `CouncilAction(COUNCIL_ADD_VALIDATOR)`
schedules the next set, the next block **announces** it and is certified by the
*current* set, so the change is itself finalized and the new set rules from the
following block. `COUNCIL_AUDITOR_SYNC` runs an **auditor light client** that
only ever trusted the genesis set: it accepts a far header when that header's
own set certifies it *and* the signers hold more than 1/3 of the set the auditor
trusts (the overlap rule), bisecting through rotation headers when the overlap is
too thin - no header replay. `GetCouncil` returns the set in force, the auditor's
trusted height and a step log; the app shows it all on the **Governance** tab.
