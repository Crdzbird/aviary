package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"

extern int aviaryGoJustify(void*, uint8_t*, size_t, uint8_t*, size_t, size_t*);
extern int aviaryGoVerify(void*, uint8_t*, size_t, uint8_t*, size_t);
extern uint32_t aviaryGoProposer(void*, uint64_t, uint32_t);

static int bc_register_go_engine(const char* id, void* ctx, int grind) {
	return aviary_registry_add_engine(id, (AviaryJustifyFn)aviaryGoJustify,
	                                        (AviaryVerifyFn)aviaryGoVerify, ctx, grind);
}
static int bc_register_go_proposer(const char* id, void* ctx) {
	return aviary_registry_add_proposer(id, (AviaryProposerFn)aviaryGoProposer, ctx);
}
*/
import "C"

import (
	"errors"
	"runtime/cgo"
	"unsafe"
)

// ForeignEngine is a consensus engine implemented in Go and plugged into a chain
// mixture by id. The consensus pipeline stays in C++ (propose/apply go through
// the Ledger); Go supplies the JUSTIFICATION policy. Both methods are invoked
// synchronously inside consensus and MUST be deterministic — no clock, no
// randomness, no map-iteration order — and must not call back into the chain.
type ForeignEngine interface {
	// Justify returns the justification for a block this node proposed (≤ 4096
	// bytes), or nil to refuse.
	Justify(block []byte) []byte
	// Verify reports whether justification is valid for block.
	Verify(block, justification []byte) bool
}

// ForeignProposer is a leader-election policy implemented in Go: the proposer
// index for a height among validators. Must be deterministic.
type ForeignProposer func(height uint64, validators uint32) uint32

// RegisterEngine registers a Go engine under id (chain identity — never
// rename). grindProof declares whether its leader election is unpredictable.
// The engine is retained for the life of the process.
func RegisterEngine(id string, e ForeignEngine, grindProof bool) error {
	ensure()
	cid := C.CString(id)
	defer C.free(unsafe.Pointer(cid))
	h := cgo.NewHandle(e) // never deleted: the registry holds it for the process lifetime
	g := 0
	if grindProof {
		g = 1
	}
	if C.bc_register_go_engine(cid, unsafe.Pointer(uintptr(h)), C.int(g)) != 1 {
		return errors.New("chain: could not register foreign engine")
	}
	return nil
}

// RegisterProposer registers a Go proposer policy under id.
func RegisterProposer(id string, p ForeignProposer) error {
	ensure()
	cid := C.CString(id)
	defer C.free(unsafe.Pointer(cid))
	h := cgo.NewHandle(p)
	if C.bc_register_go_proposer(cid, unsafe.Pointer(uintptr(h))) != 1 {
		return errors.New("chain: could not register foreign proposer")
	}
	return nil
}
