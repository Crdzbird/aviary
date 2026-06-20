// Package blockchainer is a cgo binding for the Blockchainer C ABI.
//
// It is a thin marshaling layer (ARCHITECTURE §0): it never computes consensus
// bytes itself — it hands data to the C++ core and returns what the core
// produced. All hashing happens in C++. The crypto backend is cryptolib, whose
// Go binding is depended on (see crypto.go) so the full crypto surface is
// available to consumers alongside the ledger.
//
// The native library initializes lazily on first use (or eagerly via Preload),
// so callers never have to remember to call Init.
package blockchainer

/*
#include <stdlib.h>
#include "blockchainer_c.h"
*/
import "C"

import (
	"errors"
	"sync"
	"sync/atomic"
	"unsafe"
)

// ── Initialization (lazy + optional background warm-up) ───────────────────────
//
// blockchainer_init → cryptolib_init, which is idempotent. The native lib loads
// at process start (cgo), but the one-time backend init must be run once; we do
// it lazily on first use so callers cannot forget it.

var (
	initOnce    sync.Once
	initErr     error
	initialized atomic.Bool
)

func doInit() error {
	if C.blockchainer_init() != 0 {
		return errors.New("blockchainer: init failed")
	}
	// Also initialize cryptolib's Go binding so its crypto API is ready for
	// callers that use it alongside the ledger.
	if err := cryptoInit(); err != nil {
		return err
	}
	initialized.Store(true)
	return nil
}

// ensure runs the one-time init exactly once (concurrency-safe). Every entry
// point calls it, so the API works without an explicit Init.
func ensure() { initOnce.Do(func() { initErr = doInit() }) }

// Init initializes the crypto backend. Optional — the API self-initializes on
// first use — but call it once at start if you want the init error surfaced.
// Idempotent and concurrency-safe.
func Init() error { ensure(); return initErr }

// Preload warms the backend on a BACKGROUND goroutine and returns immediately.
//
// The Go analog of the Flutter/Node fire-and-forget preload: it runs the
// one-time init off the calling goroutine, so the first real call pays no init
// cost. You never need to wait on it — the synchronous API self-initializes
// regardless. Safe to call repeatedly and safe to never call.
func Preload() { go ensure() }

// Initialized reports whether the backend has completed initialization with no
// error (ready to use without any init work on the next call).
func Initialized() bool { return initialized.Load() }

// Version returns the Blockchainer library version.
func Version() string { return C.GoString(C.blockchainer_version()) }

// CryptoBackendVersion returns the linked cryptolib version (proves linkage).
func CryptoBackendVersion() string { return C.GoString(C.blockchainer_crypto_backend_version()) }

var emptyByte = [1]byte{}

// bp returns a valid C pointer to b's bytes; for empty b it points at a shared
// read-only byte so the borrowed-input contract holds (len 0 reads nothing).
func bp(b []byte) *C.uint8_t {
	if len(b) == 0 {
		return (*C.uint8_t)(unsafe.Pointer(&emptyByte[0]))
	}
	return (*C.uint8_t)(unsafe.Pointer(&b[0]))
}

// takeBuf extracts a returned buffer into a Go slice, freeing it via the C ABI.
func takeBuf(res C.BlockchainerBufferResult) ([]byte, error) {
	if res.buf.data == nil {
		if res.error != nil {
			m := C.GoString(res.error)
			C.blockchainer_str_free(res.error)
			return nil, errors.New(m)
		}
		return nil, errors.New("operation failed")
	}
	out := C.GoBytes(unsafe.Pointer(res.buf.data), C.int(res.buf.len))
	C.blockchainer_buffer_free(&res.buf)
	return out, nil
}

// Hash returns the BLAKE3-256 digest of msg.
func Hash(msg []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_hash(bp(msg), C.size_t(len(msg))))
}

// Txid returns the canonical transaction id (32 bytes) for the given fields.
func Txid(version uint8, chainID uint64, body, author, signature []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_txid(
		C.uint8_t(version), C.uint64_t(chainID),
		bp(body), C.size_t(len(body)),
		bp(author), C.size_t(len(author)),
		bp(signature), C.size_t(len(signature))))
}

// HeaderHash returns the canonical block-header hash (32 bytes). parentHash,
// txRoot and stateRoot must each be 32 bytes.
func HeaderHash(version uint8, chainID, height uint64, parentHash []byte,
	timestamp uint64, txRoot, stateRoot, consensus []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_header_hash(
		C.uint8_t(version), C.uint64_t(chainID), C.uint64_t(height),
		bp(parentHash), C.uint64_t(timestamp),
		bp(txRoot), bp(stateRoot),
		bp(consensus), C.size_t(len(consensus))))
}

// VerifyStateProof verifies a state-inclusion proof against a 32-byte stateRoot.
func VerifyStateProof(stateRoot, proof []byte) bool {
	ensure()
	return C.blockchainer_verify_state_proof(bp(stateRoot), bp(proof), C.size_t(len(proof))) == 1
}

// VerifyTxInclusion verifies a tx-inclusion proof against a 32-byte txRoot.
func VerifyTxInclusion(txRoot, proof []byte) bool {
	ensure()
	return C.blockchainer_verify_tx_inclusion(bp(txRoot), bp(proof), C.size_t(len(proof))) == 1
}

// VerifyCommit verifies a BFT commit certificate against an encoded validator
// set (both produced by the C++ core). Returns false for any malformed input.
func VerifyCommit(chainID uint64, validatorSet, cert []byte) bool {
	ensure()
	return C.blockchainer_verify_commit(C.uint64_t(chainID),
		bp(validatorSet), C.size_t(len(validatorSet)),
		bp(cert), C.size_t(len(cert))) == 1
}
