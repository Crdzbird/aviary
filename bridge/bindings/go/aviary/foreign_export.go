package aviary

/*
#include "aviary_c.h"
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

//export aviaryGoTxSource
func aviaryGoTxSource(ctx unsafe.Pointer, chainID C.uint64_t, ts C.uint64_t, out *C.uint8_t, cap C.size_t, outLen *C.size_t) C.int {
	t, ok := cgo.Handle(uintptr(ctx)).Value().(*ChainTree)
	if !ok || t.TxSource == nil {
		return 0
	}
	blob := encodeBlobList(t.TxSource(uint64(chainID), uint64(ts)))
	if len(blob) > int(cap) {
		return 0
	}
	copy(unsafe.Slice((*byte)(unsafe.Pointer(out)), int(cap)), blob)
	*outLen = C.size_t(len(blob))
	return 1
}

//export aviaryGoFinalized
func aviaryGoFinalized(ctx unsafe.Pointer, chainID C.uint64_t, block *C.uint8_t, n C.size_t) {
	t, ok := cgo.Handle(uintptr(ctx)).Value().(*ChainTree)
	if !ok || t.OnFinalized == nil {
		return
	}
	t.OnFinalized(uint64(chainID), C.GoBytes(unsafe.Pointer(block), C.int(n)))
}

//export aviaryGoJustify
func aviaryGoJustify(ctx unsafe.Pointer, block *C.uint8_t, blockLen C.size_t,
	out *C.uint8_t, outCap C.size_t, outLen *C.size_t) C.int {
	e, ok := cgo.Handle(uintptr(ctx)).Value().(ForeignEngine)
	if !ok {
		return 0
	}
	j := e.Justify(C.GoBytes(unsafe.Pointer(block), C.int(blockLen)))
	if j == nil || len(j) > int(outCap) {
		return 0
	}
	copy(unsafe.Slice((*byte)(unsafe.Pointer(out)), int(outCap)), j)
	*outLen = C.size_t(len(j))
	return 1
}

//export aviaryGoVerify
func aviaryGoVerify(ctx unsafe.Pointer, block *C.uint8_t, blockLen C.size_t,
	just *C.uint8_t, justLen C.size_t) C.int {
	e, ok := cgo.Handle(uintptr(ctx)).Value().(ForeignEngine)
	if !ok {
		return 0
	}
	if e.Verify(C.GoBytes(unsafe.Pointer(block), C.int(blockLen)), C.GoBytes(unsafe.Pointer(just), C.int(justLen))) {
		return 1
	}
	return 0
}

//export aviaryGoProposer
func aviaryGoProposer(ctx unsafe.Pointer, height C.uint64_t, validators C.uint32_t) C.uint32_t {
	p, ok := cgo.Handle(uintptr(ctx)).Value().(ForeignProposer)
	if !ok {
		return 0
	}
	return C.uint32_t(p(uint64(height), uint32(validators)))
}
