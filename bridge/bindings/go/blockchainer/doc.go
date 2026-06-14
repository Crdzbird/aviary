// Package blockchainer provides idiomatic Go bindings for Blockchainer, a
// distributed-ledger core (ledger, BFT consensus, mempool, observability,
// light-client verification) plus EVM/Bitcoin/Solana interop, exposed through a
// stable C ABI (libblockchainer_c).
//
// # Initialization
//
// The native backend self-initializes lazily on first use, so callers never
// have to remember to call [Init]. To move that one-time work off the calling
// goroutine, call [Preload] (fire-and-forget) near program start:
//
//	blockchainer.Preload() // background warm-up; returns immediately
//	node := blockchainer.NewNode(1) // synchronous; self-initializes if needed
//
// # Requirements
//
// This package uses cgo and links against libblockchainer_c, which in turn links
// cryptolib's C ABI (libcryptolib_c) — provided by the depended-on cryptolib Go
// module. CGO_ENABLED=1 is required. Build the native libraries once and make
// them discoverable at link and run time (see cgo_local.go / cgo_pkgconfig.go).
package blockchainer
