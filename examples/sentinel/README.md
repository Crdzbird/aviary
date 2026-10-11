# Sentinel: a device provenance ledger

Sentinel is a tamper evident provenance ledger for data captured by devices:
bodycam and dashcam footage, field sensor readings, supply chain telemetry,
mobile device attestation. A Go gRPC server runs the chains through the Go
binding; a Flutter recorder streams data to it and holds no native code.

```
Flutter recorder  --gRPC (bidi stream)-->  Go server (:50052)  --cgo-->  Aviary (C++ core)
```

## What it shows

**Recorder tab**

- **A chain born from the device.** `ProvisionDevice` takes the hardware
  info and a credential and derives a chain whose genesis is bound to that
  exact device: its on-chain birth certificate.
- **Anchoring with finality.** The device streams photos, video chunks, GPS
  locations and sensor readings over `StreamData`. Each item's BLAKE3
  content hash is anchored on the device's chain and the server streams back
  an `AnchorReceipt` that moves from pending to finalized when the BFT
  block seals. Raw media never goes on chain; only the hash and metadata do.
- **Proof of existence.** `VerifyRecord` re-hashes the original bytes and
  checks a light client state inclusion proof against the chain root, which
  proves the item existed at that point and has not been altered.
- **Ledger view.** `GetLedger` returns the height, the record counts by kind
  and the state root.

Anchoring needs no special opcode: the server submits a one unit transfer
whose recipient address is the data's 32 byte hash. After the block seals,
the chain state holds an account at that address, and the ordinary account
proof proves the anchor. Media hashes are `BLAKE3(payload)`, so anyone can
recompute them from the file; location and sensor records hash their
canonical metadata (sorted keys, deterministic bytes).

**Fleet tab**

- **A fleet as a chain tree.** `BuildFleet(labels)` declares a root chain
  plus one BFT subchain per device, each funding an identity account at
  `BLAKE3(label)`, and builds the whole tree at once.
- **Anchoring upward.** `FleetProduce(chain)` finalizes a block on a device
  chain or on the root; a root block anchors every device's finalized
  headers.
- **Cross chain proof.** `FleetProveDevice(chain)` proves a device's identity
  from the fleet root header, verified against the root's subchain root.

## Run

```bash
# 1. Build the Aviary host library (once, from the repository root)
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Start the server and the recorder
./run.sh                 # macOS desktop
./run.sh -d <device-id>  # an iOS simulator reaches the host's 127.0.0.1 directly

# End to end without Flutter: provision, stream, verify
( cd server && go run ./cmd/probe )
```

The server listens on `:50052`.

## Where to look

| Piece | Path |
|---|---|
| Service definition | `proto/sentinel.proto` |
| Provisioning, anchoring, receipts, verification | `server/server.go` |
| Fleet chain tree | `server/fleet.go` |
| Headless end to end check | `server/cmd/probe/main.go` |
| Recorder and Fleet tabs | `app/lib/main.dart` |
