# Sentinel — Device Provenance Ledger

A tamper-evident **provenance ledger for device-captured data**, built on
Blockchainer with a **Go gRPC server** and a **Flutter device recorder**.

```
Flutter recorder ──gRPC (bidi stream)──▶  Go server  ──cgo──▶  Blockchainer (C++ core)
```

A device **enrolls** by sending its hardware info + a credential; the server derives
a chain whose **genesis/origin block is cryptographically bound to that exact device**
(its on-chain birth certificate). The device then **streams data** — photos, video
chunks, GPS locations, sensor readings — and each item's BLAKE3 content hash is
**anchored** on the device's own chain with BFT finality. The raw media never goes
on-chain; only its hash + metadata are anchored, so anyone can later prove a given
photo/clip/reading **existed at a point in time and has not been altered**.

Real use cases: bodycam/dashcam evidence integrity, field-sensor audit trails,
supply-chain telemetry, mobile-device attestation.

## How anchoring works (no special on-chain opcode needed)

Anchoring reuses the account state machine: the server submits a 1-unit transfer
whose **recipient address *is* the data's 32-byte BLAKE3 hash**. After a block seals,
the chain state contains an account at `address = hash`, and a **light-client
state-inclusion proof** for that address proves the data was anchored. Verification
re-hashes the original bytes and checks the proof against the chain root.

- Media (`PHOTO`/`VIDEO`): the anchored hash is `BLAKE3(payload)` — independently
  recomputable by hashing the file.
- `LOCATION`/`SENSOR`: the anchored hash is `BLAKE3(canonical metadata)` (sorted
  keys → deterministic).

## RPCs (`proto/sentinel.proto`)

| RPC | Purpose |
|---|---|
| `ProvisionDevice` | Device info + credential → genesis/origin block; returns the device chain |
| `StreamData` (bidi stream) | Device streams `DataRecord`s; server streams back `AnchorReceipt`s (pending → finalized) |
| `VerifyRecord` | Prove a content hash is committed in the chain (anchored + verified) |
| `GetLedger` | Height, record counts by kind, state root |

## Run

```bash
# 1. Build the blockchainer host library
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Start the server + recorder (macOS desktop, iOS sim, or Android)
./run.sh                 # defaults to -d macos
./run.sh -d <device-id>  # e.g. an iOS simulator (reaches the host's 127.0.0.1)

# Quick end-to-end check without Flutter (provision → stream → verify):
( cd server && go run ./cmd/probe )
```

The server listens on `:50052`. The Flutter client is a thin gRPC client (no native
blockchain code), so it runs anywhere grpc-dart does; all ledger work is server-side.

## Fleet tab (chain tree: a fleet root anchoring one subchain per device)

`BuildFleet(labels)` declares the whole fleet at once — a root chain plus a BFT
**subchain per device** whose genesis funds an identity account at
`BLAKE3(label)` — and builds it in one go (genesis bottom-up, bindings top-down).
`FleetProduce(chain)` finalizes a block on a device chain or on the root; a root
block **anchors** every device's finalized headers. `FleetProveDevice(chain)` is
a cross-chain proof of the device's identity **from the fleet root header**,
verified against its `subchain_root`. The app's **Fleet** tab drives it.
