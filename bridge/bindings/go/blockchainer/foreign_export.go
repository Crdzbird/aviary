package blockchainer

/*
#include "blockchainer_c.h"
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

//export blockchainerGoJustify
func blockchainerGoJustify(ctx unsafe.Pointer, block *C.uint8_t, blockLen C.size_t,
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

//export blockchainerGoVerify
func blockchainerGoVerify(ctx unsafe.Pointer, block *C.uint8_t, blockLen C.size_t,
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

//export blockchainerGoProposer
func blockchainerGoProposer(ctx unsafe.Pointer, height C.uint64_t, validators C.uint32_t) C.uint32_t {
	p, ok := cgo.Handle(uintptr(ctx)).Value().(ForeignProposer)
	if !ok {
		return 0
	}
	return C.uint32_t(p(uint64(height), uint32(validators)))
}
