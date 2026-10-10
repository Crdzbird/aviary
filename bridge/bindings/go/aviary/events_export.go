package aviary

/*
#include "aviary_c.h"
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

//export aviaryGoEvent
func aviaryGoEvent(ctx unsafe.Pointer, json *C.char, n C.size_t) {
	node, ok := cgo.Handle(uintptr(ctx)).Value().(*Node)
	if !ok || node.onEvent == nil {
		return
	}
	node.onEvent(C.GoStringN(json, C.int(n)))
}
