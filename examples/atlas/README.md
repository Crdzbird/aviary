# Atlas: a wholesale settlement network

Atlas is a settlement and tokenized asset network for institutions, built
on Aviary. A Go gRPC server runs the chains through the Go binding; a
Flutter operator console drives it over gRPC and holds no native code, so it
runs anywhere grpc-dart does.

```
Flutter console  --gRPC-->  Go server (:50051)  --cgo-->  Aviary (C++ core)
```

## What it shows

**Settlement tab**

- **One BFT rail per currency.** `CreateChain` launches an instant finality
  chain (for example "EURx") with a genesis reserve treasury.
- **Institutional wallets.** `CreateWallet` gives each institution a
  settlement balance and cross chain receive addresses on Ethereum, Bitcoin
  and Solana derived from the same key, for bridging external assets in.
- **Issuance and settlement with finality.** `IssueFunds` and `Settle` queue
  transfers in the mempool; `ProduceBlock` finalizes them atomically in one
  BFT round. Nothing is settled until it is final, and nothing final is ever
  reverted.
- **Auditable balances.** `GetBalanceProof` returns a light client state
  inclusion proof for any wallet, re-verified against the chain's state
  root.
- **Real ERC-20 transfers.** `BuildErc20Transfer` ABI encodes and signs a
  transfer (for example USDC) with the wallet's secp256k1 key; the raw
  transaction is ready for `eth_sendRawTransaction`. `FunctionSelector`
  shows the ABI selector computation.
- **Live feed.** `StreamEvents` pushes admissions and finalizations to the
  console as they happen; `GetMetrics` returns the Prometheus text.

**Governance tab**

- **A validator council.** The rail is governed by a BFT chain whose
  validator set the operator rotates. `CouncilAction(COUNCIL_ADD_VALIDATOR)`
  schedules the next set; the next block announces it and is certified by
  the current set, so the change is itself finalized and the new set rules
  from the following block.
- **An auditor light client.** `COUNCIL_AUDITOR_SYNC` runs a light client
  that only ever trusted the genesis set. It accepts a far header when that
  header's own set certifies it and the signers hold more than a third of
  the set it trusts (the overlap rule), bisecting through rotation headers
  when the overlap is too thin. `GetCouncil` returns the set in force, the
  auditor's trusted height and a step log, which the tab renders.

## Run

```bash
# 1. Build the Aviary host library (once, from the repository root)
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Start the server and the console
./run.sh                 # macOS desktop
./run.sh -d <device-id>  # an iOS simulator reaches the host's localhost directly
```

Or separately: `( cd server && go run . )` then `( cd app && flutter run )`.
The server listens on `:50051`.

## Where to look

| Piece | Path |
|---|---|
| Service definition | `proto/atlas.proto` (`Atlas` service) |
| Chains, wallets, settlement, proofs, ERC-20 | `server/server.go` |
| Council rotation and the auditor light client | `server/council.go` |
| Operator console (Settlement and Governance tabs) | `app/lib/main.dart` |
| Generated stubs | `server/gen`, `app/lib/gen` |
