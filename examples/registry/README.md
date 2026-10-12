# Registry: application records, no money involved

An equipment registry on an Aviary chain whose state model is `record`
instead of `account`: every machine is a record whose value is a JSON
document, owned by the department that registered it. It is a pure Dart
program, no server and no Flutter, and runs in a few seconds.

## What it shows

- **Records instead of balances.** A chain assembled with
  `assembleRecordChain` stores 32 byte keys with an owner, a version and an
  opaque value of up to 64 KiB. The example stores JSON; protobuf, CBOR or a
  content hash work the same way.
- **Signed, finalized writes.** `chain.write(secret, key, RecordOp.put,
  value)` signs a transaction with the author's Ed25519 key and queues it;
  `produceBlock` finalizes the queue in one BFT round. Several writes from
  one author can be queued before a block; the nonce is taken from the chain.
- **Ownership rules enforced by consensus.** Only the owner may update,
  transfer or delete a record. The lab's attempt to edit a microscope it does
  not own is refused when it is queued, with the reason. After a
  `RecordOp.transfer` the lab owns it and facilities is the one refused.
- **Versions.** Every write increments the record's version, so a service
  log entry produces version 2, the transfer version 3 and the lab's update
  version 4.
- **Verification without trust.** An `Auditor` that never ran the chain
  holds only a state root (what a light client would give it), verifies a
  record proof against it with `verifyStateProof` and reads the record out
  of the proof with `recordFromProof`. A tampered proof is rejected.
- **Deletion.** A deleted record disappears from state and its key is free
  for anyone to register again.
- **Observability.** The same `events()` and `metricValues()` as any chain:
  one `BlockFinalized` per block with its transaction count.

## Run

```bash
# 1. Build the Aviary host library (once, from the repository root)
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Run the registry
dart pub get
dart run bin/registry.dart
```

The example resolves `aviary_dart` from this repository and loads the engine
the package bundles for macOS arm64, or the one `AVIARY_DYLIB` points at.

## Where to look

| Piece | Path |
|---|---|
| The whole scenario | `bin/registry.dart` |
| Record model in the engine | `include/aviary/record_state.hpp`, `src/state/record_state.cpp` |
| Dart surface | `bridge/bindings/dart/lib/src/handles/chain_records.dart`, `lib/src/api/records.dart` |
| C ABI | `aviary_record_*` and `aviary_chain_add_tx`, `aviary_chain_record`, `aviary_chain_prove_record` in `bridge/aviary_c.h` |
