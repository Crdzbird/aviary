# Interop: Go and Flutter run the same ledger

This example proves the engine is byte identical across bindings. A Go
server (full node over the Go binding) and a Flutter client (the `aviary`
package, native) each run the ledger independently and cross check each
other in both directions. A mismatch anywhere would be a consensus fork
between bindings.

```
Flutter client (native engine)  <--gRPC-->  Go server (:50053, native engine)
```

## What it shows

**Ledger tab**

| Direction | Check |
|---|---|
| Go and Flutter | Both compute the same canonical bytes for identical inputs: BLAKE3 hash, canonical transaction id, block header hash. |
| Go to Flutter | The Go node builds a chain and returns a light client proof; the Flutter client verifies it natively against the state root. Both apply the same deterministic scenario, so the state roots must match. |
| Flutter to Go | The Flutter client produces a proof; the Go node verifies it. |

Determinism is exact because the scenario shares the account secrets and
Ed25519 signatures are deterministic: identical inputs give identical
transaction ids, state roots and proofs on both sides. The client shows a
green panel when every check passes.

**Chain tree tab**

Both sides build the same family (a root, a child subchain, a grandchild)
with validator keys derived from one seed, run the same production schedule
and compare: the chain ids and the root's subchain root must be byte
identical; the client verifies Go's cross chain proof natively (`BuildTree`)
and Go verifies the client's proof (`VerifyTreeProof`).

## Run

```bash
# 1. Build the Aviary host library (once, from the repository root)
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Start the server and the client
./run.sh                       # macOS desktop
./run.sh -d <ios-simulator>    # the simulator reaches the host's localhost directly

# Android device: route the port to the host first
adb reverse tcp:50053 tcp:50053
./run.sh -d <android-device-id>

# Go only, no Flutter
( cd server && go run ./cmd/probe )
```

The client connects to `127.0.0.1:50053`.

## Where to look

| Piece | Path |
|---|---|
| Service definition | `proto/interop.proto` |
| Go node, scenario, proof exchange | `server/main.go` |
| Go side of the chain tree | `server/tree.go` |
| Flutter client with both tabs | `app/lib/main.dart` |
