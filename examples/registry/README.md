# Registry: application records, no money involved

An equipment registry on an Aviary chain whose state model is `record`
instead of `account`: every machine is a record whose value is a JSON
document, owned by the department that registered it. It is a pure Dart
program, no server and no Flutter, and runs in a few seconds. A second
program, `bin/network.dart`, runs the same registry on four validators that
talk only through messages.

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

## The networked registry

`bin/network.dart` replaces the single process chain with four sites (plant,
lab, warehouse, office), each a validator with its own `NetNode`, and an
auditor that follows the chain as an observer. A `PeerMesh` carries every
message between them, the same role a socket transport plays in production.

- **Genesis with the record model.** `buildGenesis(stateModel: 2)` with the
  four validator keys; every node starts from those exact bytes.
- **Writes through consensus.** Each height the app signs the record
  transactions with `signRecordTx`, hands them to every validator with
  `setTransactions` and then starts or advances the height, so whichever
  validator proposes includes them. The list is cleared when the height
  commits.
- **Nonces from the network.** `recordNonce(address)` on any node gives the
  next nonce, since every node holds the same state.
- **The same answer everywhere.** After each height `record(key)` returns
  the same owner, version and value on all five nodes, including the
  observer that never voted.
- **Proofs across nodes.** The auditor checks a proof produced by the office
  site against its own state root; a tampered proof is rejected and an
  unknown key reads as `null`.

## Run

```bash
# 1. Build the Aviary host library (once, from the repository root)
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Run the registry, then the networked one
dart pub get
dart run bin/registry.dart
dart run bin/network.dart
```

The example resolves `aviary_dart` from this repository and loads the engine
the package bundles for macOS arm64, or the one `AVIARY_DYLIB` points at.

## Where to look

| Piece | Path |
|---|---|
| The whole scenario | `bin/registry.dart` |
| The networked scenario | `bin/network.dart` |
| Networked record queries | `bridge/bindings/dart/lib/src/handles/net_node_queries.dart` |
| Record model in the engine | `include/aviary/record_state.hpp`, `src/state/record_state.cpp` |
| Dart surface | `bridge/bindings/dart/lib/src/handles/chain_records.dart`, `lib/src/api/records.dart` |
| C ABI | `aviary_record_*` and `aviary_chain_add_tx`, `aviary_chain_record`, `aviary_chain_prove_record`, `aviary_net_node_record`, `aviary_net_node_record_nonce`, `aviary_net_node_prove_record` in `bridge/aviary_c.h` |
