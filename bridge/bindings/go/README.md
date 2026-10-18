# Aviary - Go binding

Idiomatic Go bindings for Aviary, a distributed-ledger core (ledger, BFT
consensus, mempool, observability, light-client verification) plus
EVM/Bitcoin/Solana interop, over a stable C ABI (`libaviary_c`) via cgo.

```go
import bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"

func main() {
    bc.Preload()                 // optional background warm-up - returns at once

    node := bc.NewNode(1)        // self-initializes; no Init() needed
    defer node.Free()
    aSec, _, aAddr, _ := node.NewAccount()
    _, _, bAddr, _ := node.NewAccount()
    node.Credit(aAddr, 1_000_000)
    node.SubmitTransfer(aSec, bAddr, 250_000, node.Nonce(aAddr))
    node.ProduceBlock(1)         // BFT finalize -> height 1

    // Light-client proof
    bc.VerifyStateProof(node.StateRoot(), node.ProveAccount(bAddr)) // true
}
```

## Initialization - lazy, with an optional background warm-up

The native backend initializes **lazily on first use**, so you never have to
remember `Init()`. To move that one-time work off the calling goroutine, call
`Preload()` near program start - the Go analog of the Flutter/Node fire-and-forget
preload:

| API | Blocks? | Use |
|---|---|---|
| `bc.Preload()` | no | Background warm-up (`go ensure()`); returns immediately. |
| `bc.NewNode(...)` / `bc.Hash(...)` / ... | no | Self-initializing; works with or without `Preload()`. |
| `bc.Init() error` | yes (once) | Optional; runs init synchronously and surfaces any error. |
| `bc.Initialized() bool` | - | `true` once the backend is ready. |

`aviary_init` -> `cryptolib_init`, which is idempotent; init runs exactly
once (`sync.Once`), concurrency-safe.

## Crypto backend: cryptolib (depended on, not bundled)

Aviary's crypto backend is **cryptolib**, whose Go binding is a dependency
of this module - so the full crypto surface (hashing, AEAD, Ed25519/X25519,
post-quantum ML-KEM/ML-DSA, BLS, vaults, keyring, EVM/Bitcoin primitives) is
available alongside the ledger, from one `go get`:

```go
import (
    bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
    "github.com/Crdzbird/CipherBird/bridge/bindings/go/cryptolib"
)

digest, _ := cryptolib.Blake3(doc, 32)      // off-chain crypto
kp := cryptolib.Ed25519Keygen()
node := bc.NewNode(1)                         // on-chain ledger
```

`aviary_c` links `cryptolib_c` (CipherBird); importing its Go package provides
that library (the Go analog of the `aviary` Flutter package depending on
`cipherbird`). `bc.CryptoBackendVersion()` reports the linked backend
version (`3.0.0`), proving the link end to end.

## Building & linking

Requires `CGO_ENABLED=1` and the prebuilt native libraries.

- **Local (default)** - `cgo_local.go` links `libaviary_c` from the repo
  `build/` dir and bakes an rpath to it; cryptolib's own Go binding bakes the
  rpath to its `build/release`. So in-repo binaries run with no environment
  setup - just check out `cryptolib` next to `aviary`.
- **Static (`-tags "aviary_static cryptolib_static"`)** - the "just works
  everywhere" option: links the merged `build/static/libaviary_c.a` plus
  cryptolib's static archive. The binary has **no `libaviary_c` /
  `libcryptolib_c` shared-library dependency** - nothing to ship, no rpath, no
  `DYLD_LIBRARY_PATH`/`LD_LIBRARY_PATH`. Build the archives once:

  ```bash
  ( cd ../cryptolib && scripts/build_static_archive.sh )   # cryptolib archive
  scripts/build_static_archive.sh                          # aviary archive
  ( cd bridge/bindings/go && go build -tags "aviary_static cryptolib_static" ./... )
  ```
- **Installed (`-tags aviary_pkgconfig`)** - resolves flags from a
  `aviary_c.pc` on `PKG_CONFIG_PATH` (template: `aviary_c.pc.in`).
  Set `CGO_LDFLAGS`/`DYLD_LIBRARY_PATH`/`LD_LIBRARY_PATH` to your install.

```bash
go test ./aviary/      # unit tests + cross-binding determinism
go run  ./example            # ledger + cryptolib crypto together
go run  ./cmd/demo           # cross-language golden-vector determinism check
```

## Events, persistence, custody signing and networking

- Push events: `node.SetEventHook(func(json string))` (Go), `node.onEvent(cb)`
  (npm), `node.setEventHook(Consumer<String>)` (JVM), `node.setEventHook { }`
  (Swift) receive each chain event as one JSON object; a nil hook restores
  buffering for the drain call.
- The same events and metrics on an assembled chain and a networked node:
  `Chain` and `NetNode` expose drain events, set event hook and metrics in
  every binding. A chain reports block_finalized or block_rejected per submit;
  a networked node reports block_finalized per committed height and counts
  gossip received, relayed and dropped, timeouts fired and peers banned.
- Persistence: `Snapshot` / `RestoreNode` / `BlockAt` rebuild a node from one
  blob by replaying its blocks through consensus.
- Custody signing: `KeyStore.SignTransfer`, `Node.SubmitTransferFromStore` and
  `Node.SubmitTx` sign from the sealed store without the secret ever leaving
  the engine; `PublicKey`, `Has`, `Addresses`, `Lock` round out the store.
- Networking: `NetNode` runs BFT consensus over a gossip mesh with no socket
  and no clock; every input returns the relays to send and the timeouts to
  arm (`Start`, `OnGossip`, `OnTimeout`, `Advance`, `SetTransactions`,
  `ReplayLog`). `SecureSession` protects each link with the mutually
  authenticated post quantum hybrid handshake (`GenerateSecureIdentity`,
  `InitiateSecureSession`, `AcceptSecureSession`, `Encrypt`, `Decrypt`).
  Names follow each language's casing.

## Records, peering and admission

- Records: a chain built from a genesis with state model 2 stores 32 byte
  keys, each with an owner, a version and a value of up to 64 KiB.
  `Chain.Write(secret, key, op, value)` signs and queues a write with the
  nonce taken from the chain; the ops are `RecordPut`, `RecordTransfer` and
  `RecordDelete`, and only the owner may update, transfer or delete.
  `Chain.Record`, `Chain.RecordNonce` and `Chain.ProveRecord` read the
  committed state, and `RecordFromProof(proof)` reads the record out of a
  state proof that verified. `SignRecordTx` signs a write without a chain
  handle.
- Custody and post quantum authors: `Chain.WriteFromStore(store, from, key,
  op, value)` and `KeyStore.SignRecord` sign record writes from the sealed key
  store, with the scheme taken from the stored key. `Node.NewAccountHybrid`,
  `Node.SubmitTransferHybrid` and `SignRecordTxHybrid` sign with Ed25519 and
  ML-DSA-65 together in the version 2 envelope.
- Networked records: on a record model network `NetNode.Record`,
  `NetNode.RecordNonce` and `NetNode.ProveRecord` answer on every node,
  observers included. A proposer includes the list given to `SetTransactions`;
  the list is cleared when the height commits, so set the next height's list
  after the commit and before `Advance`.
- In process meshes: `NewPeerMesh(nodes...)` with `Link`, `LinkAll`, `Start`,
  `Advance`, `Step` and `Settle(validators...)` relay in emission order and
  fire timeouts only when nothing is pending, for tests and single process
  networks.
- Peer exchange: `SignPeerRecord`, `VerifyPeerRecord` and `ReadPeerRecord`
  handle self signed peer records (id, host, port, KEM and signature keys,
  optional prekey). `NewAddressBook` / `DecodeAddressBook` give an
  `AddressBook` with `Add`, `Merge`, `Record(id)`, `Records` and `Encode`; a
  merge verifies every record and never overwrites a known id.
- Noise links: `GenerateNoiseIdentity`, `InitiateNoiseSession(me, prologue)`
  and `AcceptNoiseSession(me, msg0, prologue)`; the session runs
  `HandshakeMessage`, `CompleteInitiator` / `CompleteResponder`, exchanges
  `IdentityProof` / `VerifyIdentityProof`, then `Encrypt` and `Decrypt`. Pin
  `PeerIdentity` so a reconnecting peer cannot change keys.
- Admission: `NewAdmissionChallenge(epoch)` gives a nonce to keep and a wire
  to send; the peer answers with `CredentialPolicy.AdmissionAnswer(credential,
  wire)` and the verifier checks it with
  `CredentialGate.AdmissionGranted(epoch, nonce, answer, now)`, so only
  credential holders get past the handshake.

## Install

```bash
go get github.com/Crdzbird/aviary/bridge/bindings/go@bridge/bindings/go/v0.1.1
```

The module depends on CipherBird's Go module (`github.com/Crdzbird/CipherBird/
bridge/bindings/go`) at a pinned pseudo version. Both bindings are cgo
wrappers over C libraries, so a fetched copy needs those libraries available
through pkg-config: install `libaviary_c` and `libcryptolib_c` with their
`.pc` files (`bridge/bindings/go/aviary/aviary_c.pc.in` is the template) and
build with

```bash
go build -tags aviary_pkgconfig,cryptolib_pkgconfig ./...
```

Inside this repository the root `go.work` puts the binding, the example
servers and the sibling CipherBird checkout in one workspace, so plain
`go build` and `go test` link the development build in `build/` without any
tags.
