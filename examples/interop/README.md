# Go ↔ Flutter interoperability

Proves the Blockchainer ledger is **byte-identical across language bindings**.
A **Go** server (blockchainer Go binding, full node) and a **Flutter** client
(`blockchainer_flutter`, native) each run the ledger independently and cross-check
three directions:

| Direction | Check |
|---|---|
| **go ↔ flutter** | Both compute the **same canonical bytes** — BLAKE3 hash, canonical txid, block-header hash — for identical inputs. A mismatch would be a consensus fork between bindings. |
| **go → flutter** | The Go node builds a chain and returns a light-client **proof**; the Flutter client **verifies it natively** against the state root. Both also apply the same deterministic scenario → **state roots must match**. |
| **flutter → go** | The Flutter client produces a proof; the **Go node verifies it**. |

Determinism is exact because the scenario shares the account secrets, and Ed25519
signatures are deterministic — so identical inputs ⇒ identical txids, state roots,
and proofs on both sides.

## Run

```bash
# 1. build the native host lib (once)
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. server + client
./run.sh                       # client on macOS desktop (server on :50053)
./run.sh -d <ios-simulator>    # iOS sim reaches the host's localhost directly

# Android device: route localhost to the host first, then run:
#   adb reverse tcp:50053 tcp:50053
#   ./run.sh -d <android-device-id>
```

The Flutter client connects to `127.0.0.1:50053`, runs all three direction
checks natively, and shows a green panel ("Ledger is byte-identical across Go and
Flutter") when every check passes. `server/cmd/probe` runs the same checks
Go-only.
