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
	switch v := cgo.Handle(uintptr(ctx)).Value().(type) {
	case *Node:
		if v.onEvent != nil {
			v.onEvent(C.GoStringN(json, C.int(n)))
		}
	case *eventSink:
		if v.fn != nil {
			v.fn(C.GoStringN(json, C.int(n)))
		}
	}
}
