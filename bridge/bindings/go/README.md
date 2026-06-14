# Blockchainer — Go binding

Idiomatic Go bindings for Blockchainer, a distributed-ledger core (ledger, BFT
consensus, mempool, observability, light-client verification) plus
EVM/Bitcoin/Solana interop, over a stable C ABI (`libblockchainer_c`) via cgo.

```go
import bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"

func main() {
    bc.Preload()                 // optional background warm-up — returns at once

    node := bc.NewNode(1)        // self-initializes; no Init() needed
    defer node.Free()
    aSec, _, aAddr, _ := node.NewAccount()
    _, _, bAddr, _ := node.NewAccount()
    node.Credit(aAddr, 1_000_000)
    node.SubmitTransfer(aSec, bAddr, 250_000, node.Nonce(aAddr))
    node.ProduceBlock(1)         // BFT finalize → height 1

    // Light-client proof
    bc.VerifyStateProof(node.StateRoot(), node.ProveAccount(bAddr)) // true
}
```

## Initialization — lazy, with an optional background warm-up

The native backend initializes **lazily on first use**, so you never have to
remember `Init()`. To move that one-time work off the calling goroutine, call
`Preload()` near program start — the Go analog of the Flutter/Node fire-and-forget
preload:

| API | Blocks? | Use |
|---|---|---|
| `bc.Preload()` | no | Background warm-up (`go ensure()`); returns immediately. |
| `bc.NewNode(...)` / `bc.Hash(...)` / … | no | Self-initializing; works with or without `Preload()`. |
| `bc.Init() error` | yes (once) | Optional; runs init synchronously and surfaces any error. |
| `bc.Initialized() bool` | — | `true` once the backend is ready. |

`blockchainer_init` → `cryptolib_init`, which is idempotent; init runs exactly
once (`sync.Once`), concurrency-safe.

## Crypto backend: cryptolib (depended on, not bundled)

Blockchainer's crypto backend is **cryptolib**, whose Go binding is a dependency
of this module — so the full crypto surface (hashing, AEAD, Ed25519/X25519,
post-quantum ML-KEM/ML-DSA, BLS, vaults, keyring, EVM/Bitcoin primitives) is
available alongside the ledger, from one `go get`:

```go
import (
    bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
    "github.com/Crdzbird/CryptoLib/bridge/bindings/go/cryptolib"
)

digest, _ := cryptolib.Blake3(doc, 32)      // off-chain crypto
kp := cryptolib.Ed25519Keygen()
node := bc.NewNode(1)                         // on-chain ledger
```

`blockchainer_c` links `cryptolib_c`; importing cryptolib's Go package provides
that library (the Go analog of `blockchainer_flutter` depending on
`cryptolib_flutter`). `bc.CryptoVersion()` returns cryptolib's version via its Go
binding and equals `bc.CryptoBackendVersion()` — proving the link end to end.

## Building & linking

Requires `CGO_ENABLED=1` and the prebuilt native libraries.

- **Local (default)** — `cgo_local.go` links `libblockchainer_c` from the repo
  `build/` dir and bakes an rpath to it; cryptolib's own Go binding bakes the
  rpath to its `build/release`. So in-repo binaries run with no environment
  setup — just check out `cryptolib` next to `blockchainer`.
- **Static (`-tags "blockchainer_static cryptolib_static"`)** — the "just works
  everywhere" option: links the merged `build/static/libblockchainer_c.a` plus
  cryptolib's static archive. The binary has **no `libblockchainer_c` /
  `libcryptolib_c` shared-library dependency** — nothing to ship, no rpath, no
  `DYLD_LIBRARY_PATH`/`LD_LIBRARY_PATH`. Build the archives once:

  ```bash
  ( cd ../cryptolib && scripts/build_static_archive.sh )   # cryptolib archive
  scripts/build_static_archive.sh                          # blockchainer archive
  ( cd bridge/bindings/go && go build -tags "blockchainer_static cryptolib_static" ./... )
  ```
- **Installed (`-tags blockchainer_pkgconfig`)** — resolves flags from a
  `blockchainer_c.pc` on `PKG_CONFIG_PATH` (template: `blockchainer_c.pc.in`).
  Set `CGO_LDFLAGS`/`DYLD_LIBRARY_PATH`/`LD_LIBRARY_PATH` to your install.

```bash
go test ./blockchainer/      # unit tests + cross-binding determinism
go run  ./example            # ledger + cryptolib crypto together
go run  ./cmd/demo           # cross-language golden-vector determinism check
```
