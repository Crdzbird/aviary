# Aviary API

A small, production-shaped **HTTP/JSON API** over [Aviary](https://github.com/Crdzbird/aviary) —
a distributed-ledger core (ledger, BFT consensus, mempool, observability,
light-client verification) plus EVM/Bitcoin/Solana interop. This service embeds
the Aviary **Go binding** (cgo) and exposes it as a stateless-ish REST API:
create chains, manage accounts, submit transfers, finalize blocks, serve
light-client proofs, and derive/sign cross-chain transactions.

```
HTTP client ──REST/JSON──▶ aviary-api (Go) ──cgo──▶ libaviary_c
                                                            └─▶ libcryptolib_c
```

---

## Features

- **Chains & accounts** — create in-memory chains, generate Ed25519 accounts.
- **Transactions & finality** — submit signed transfers, produce blocks (BFT).
- **Light-client proofs** — state-inclusion / tx-inclusion / commit verification.
- **Observability** — per-chain event stream + Prometheus metrics; service-level
  `/metrics` and health probes.
- **Interop** — derive Ethereum / Bitcoin / Solana addresses; build + **sign
  ERC-20 / legacy EVM transactions** server-side.
- **Deterministic** — canonical bytes (hash, txid, header hash) are byte-identical
  to every other Aviary binding (Dart/Flutter, Node, Swift).

---

## Requirements

| | |
|---|---|
| Go | 1.24+ |
| cgo | **`CGO_ENABLED=1`** (default unless cross-compiling) — this service links C |
| Toolchain | a C/C++ toolchain (clang/gcc) and `cmake`/`ninja` to build the native libs |
| Native libs | `libaviary_c` + `libcryptolib_c` (built once; see below) |

> Aviary's crypto backend is **cryptolib**. Until both are published, you
> consume them from **sibling checkouts** via Go `replace` directives.

Lay the repos out side by side:

```
~/code/
├── aviary/        # github.com/Crdzbird/aviary
├── cryptolib/           # the cryptolib repo (private/sibling)
└── aviary-api/    # THIS service
```

---

## 1. Build the native libraries (once)

```bash
# cryptolib (provides libcryptolib_c)
( cd ../cryptolib   && cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release && cmake --build build/release )

# aviary (provides libaviary_c, which links cryptolib)
( cd ../aviary && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build )
```

The Aviary Go binding's default (`cgo_local`) linkage bakes the rpaths to
these build dirs, so dev binaries run with **no `LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH`**.

## 2. Wire up the dependency (`go.mod`)

```go
module github.com/Crdzbird/aviary-api

go 1.24

require github.com/Crdzbird/aviary/bridge/bindings/go v0.0.0-00010101000000-000000000000

// Sibling checkouts (neither library is published yet).
replace github.com/Crdzbird/aviary/bridge/bindings/go => ../aviary/bridge/bindings/go
replace github.com/Crdzbird/CipherBird/bridge/bindings/go    => ../cryptolib/bridge/bindings/go
```

Import the package:

```go
import bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
```

## 3. Run

```bash
go run ./cmd/server        # listens on :8080 by default
# or
make run
```

```bash
curl -s localhost:8080/version
# {"version":"0.0.1","cryptoBackend":"3.0.0"}
```

---

## Configuration

All via environment variables (12-factor):

| Var | Default | Description |
|---|---|---|
| `HTTP_ADDR` | `:8080` | listen address |
| `LOG_LEVEL` | `info` | `debug` \| `info` \| `warn` \| `error` |
| `READ_TIMEOUT` | `15s` | HTTP read timeout |
| `WRITE_TIMEOUT` | `30s` | HTTP write timeout |
| `MAX_BODY_BYTES` | `1048576` | request body cap |

---

## API

Base path `/v1`. All bodies are JSON; byte fields are **hex** strings. A chain is
addressed by its `chainId` (uint64).

### Service

| Method | Path | Description |
|---|---|---|
| `GET` | `/healthz` | liveness |
| `GET` | `/readyz` | readiness (native backend initialized) |
| `GET` | `/version` | `{version, cryptoBackend}` |
| `GET` | `/metrics` | service Prometheus metrics |

### Chains & accounts

| Method | Path | Body → Response |
|---|---|---|
| `POST` | `/v1/chains` | `{chainId}` → `{chainId}` |
| `GET`  | `/v1/chains/{chainId}` | → `{height, mempool, stateRoot}` |
| `POST` | `/v1/chains/{chainId}/accounts` | → `{secret, publicKey, address}` (hex) |
| `POST` | `/v1/chains/{chainId}/credit` | `{address, amount}` → `204` *(genesis only, before the first block)* |
| `GET`  | `/v1/chains/{chainId}/accounts/{address}` | → `{balance, nonce}` |

### Transactions & finality

| Method | Path | Body → Response |
|---|---|---|
| `POST` | `/v1/chains/{chainId}/transfers` | `{secret, to, amount, nonce}` → `{txid}` |
| `POST` | `/v1/chains/{chainId}/blocks` | `{timestamp}` → `{height, stateRoot}` |

### Proofs & verification

| Method | Path | Body → Response |
|---|---|---|
| `GET`  | `/v1/chains/{chainId}/accounts/{address}/proof` | → `{stateRoot, proof, verified}` |
| `POST` | `/v1/verify/state` | `{stateRoot, proof}` → `{ok}` |
| `POST` | `/v1/verify/tx` | `{txRoot, proof}` → `{ok}` |
| `POST` | `/v1/verify/commit` | `{chainId, validatorSet, cert}` → `{ok}` |

### Observability

| Method | Path | Description |
|---|---|---|
| `GET` | `/v1/chains/{chainId}/events` | drain buffered chain events (JSON array) |
| `GET` | `/v1/chains/{chainId}/metrics` | per-chain Prometheus snapshot |

### Interop (addresses + EVM contracts)

| Method | Path | Body → Response |
|---|---|---|
| `POST` | `/v1/interop/addresses` | `{secret}` → `{eth, btc, sol}` |
| `POST` | `/v1/interop/eth/selector` | `{signature}` → `{selector}` |
| `POST` | `/v1/interop/eth/erc20-transfer` | `{token, to, amount, nonce, gasPrice, gasLimit, chainId, secret}` → `{rawTx, calldata}` |

### Examples

```bash
# create chain 42 + an account
curl -s -XPOST localhost:8080/v1/chains -d '{"chainId":42}'
ACC=$(curl -s -XPOST localhost:8080/v1/chains/42/accounts)
ALICE_SECRET=$(jq -r .secret <<<"$ACC"); ALICE=$(jq -r .address <<<"$ACC")

# genesis credit, transfer, finalize
curl -s -XPOST localhost:8080/v1/chains/42/credit   -d "{\"address\":\"$ALICE\",\"amount\":1000000}"
curl -s -XPOST localhost:8080/v1/chains/42/transfers -d "{\"secret\":\"$ALICE_SECRET\",\"to\":\"$BOB\",\"amount\":250000,\"nonce\":0}"
curl -s -XPOST localhost:8080/v1/chains/42/blocks    -d '{"timestamp":1}'
# {"height":1,"stateRoot":"7d7f97bc…"}

# light-client proof (server verifies it for you)
curl -s localhost:8080/v1/chains/42/accounts/$BOB/proof
# {"stateRoot":"7d7f97bc…","proof":"…","verified":true}

# sign an ERC-20 transfer (USDC), ready for eth_sendRawTransaction
curl -s -XPOST localhost:8080/v1/interop/eth/erc20-transfer \
  -d '{"token":"0xA0b8…","to":"0x…","amount":250000000,"nonce":0,"gasPrice":30000000000,"gasLimit":60000,"chainId":1,"secret":"…"}'
```

---

## Wiring it up (handler sketch)

The binding self-initializes on first use; call `Preload()` at startup to warm
it off the request path. **Each node is not safe for concurrent use** — guard it
with a mutex (one per chain).

```go
package main

import (
	"net/http"
	"sync"

	bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
)

type chain struct {
	mu   sync.Mutex
	node *bc.Node
}

type server struct {
	mu     sync.RWMutex
	chains map[uint64]*chain
}

func main() {
	bc.Preload() // background warm-up; the API self-initializes regardless

	s := &server{chains: map[uint64]*chain{}}
	mux := http.NewServeMux()
	mux.HandleFunc("GET /version", func(w http.ResponseWriter, _ *http.Request) {
		writeJSON(w, map[string]string{
			"version":       bc.Version(),
			"cryptoBackend": bc.CryptoBackendVersion(),
		})
	})
	mux.HandleFunc("POST /v1/chains/{id}/blocks", s.produceBlock)
	// … register the rest …

	http.ListenAndServe(":8080", mux)
}

func (s *server) produceBlock(w http.ResponseWriter, r *http.Request) {
	c := s.chain(chainID(r))      // lookup + create-on-demand under s.mu
	c.mu.Lock()                   // serialize all native calls on this node
	defer c.mu.Unlock()
	height, err := c.node.ProduceBlock(timestamp(r))
	// … marshal {height, stateRoot:hex(c.node.StateRoot())} …
}
```

> **Concurrency contract:** the C++ core behind a `*bc.Node` is single-writer.
> Serialize every call on a given node with its mutex; different chains run in
> parallel. Never share a node pointer across a write without the lock.

> **Genesis ordering:** `Credit` is only valid **before the first block and
> before any state read** on a fresh chain — the node's lazy genesis is finalized
> by the first read (`Height`/`Balance`/`StateRoot`) or block, after which credits
> are silently dropped. So fund all genesis accounts first, then read/produce
> blocks; afterwards, fund via `transfers`. The ledger layer enforces this:
> `Credit` never reads node state first, and returns `409` once a block is sealed.

---

## Build & deploy

### Dynamic (default, dev)

```bash
CGO_ENABLED=1 go build -o aviary-api ./cmd/server
```

Ship `libaviary_c` + `libcryptolib_c` alongside, or rely on the baked dev
rpaths / set `LD_LIBRARY_PATH`.

### Static (recommended for containers) — one self-contained binary

Build the merged static archives once, then link them in:

```bash
( cd ../cryptolib    && scripts/build_static_archive.sh )   # build/static/libcryptolib_c.a
( cd ../aviary && scripts/build_static_archive.sh )   # build/static/libaviary_c.a

CGO_ENABLED=1 go build -tags "aviary_static cryptolib_static" -o aviary-api ./cmd/server
```

The result has **no `libaviary_c` / `libcryptolib_c` shared-library
dependency** — nothing to ship, no rpath, no `LD_LIBRARY_PATH` (only the system
C++ runtime). Verify:

```bash
otool -L aviary-api | grep -iE 'aviary|cryptolib' || echo "self-contained ✓"   # macOS
ldd      aviary-api | grep -iE 'aviary|cryptolib' || echo "self-contained ✓"   # Linux
```

### Docker (multi-stage, static)

```dockerfile
FROM golang:1.24 AS build
RUN apt-get update && apt-get install -y cmake ninja-build clang
WORKDIR /src
# COPY the three repos (aviary-api, aviary, cryptolib) side by side,
# build the static archives, then:
RUN CGO_ENABLED=1 go build -tags "aviary_static cryptolib_static" \
    -o /out/aviary-api ./cmd/server

FROM gcc:14-slim          # provides libstdc++ for the C++ runtime
COPY --from=build /out/aviary-api /usr/local/bin/
EXPOSE 8080
ENTRYPOINT ["aviary-api"]
```

---

## Project layout

```
aviary-api/
├── cmd/server/main.go     # wiring, http.Server, graceful shutdown
├── internal/api/          # handlers, routing, request/response types
├── internal/ledger/       # thin service over *bc.Node (mutex-guarded registry)
├── internal/obs/          # Prometheus, structured logging
├── go.mod
├── Makefile
└── README.md
```

---

## Safety notes

- **Determinism is consensus.** The canonical bytes this service returns (txids,
  state roots, header hashes) are byte-identical across every Aviary
  binding — don't post-process them; pass them through verbatim.
- **Secrets.** Transfer/signing endpoints accept raw private keys for demo
  ergonomics. In production, keep keys server-side (a custodial keyring or HSM)
  and never accept or log them over the wire.
- **In-memory state.** Nodes are in-memory; persistence/replication is out of
  scope for this service. Front it with whatever durability your deployment needs.

## License

MIT (match Aviary).
