package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"

extern void aviaryGoEvent(void* ctx, const char* json, size_t len);
static void bc_node_hook(void* n, void* ctx) {
	aviary_node_set_event_hook((AviaryNode)n, (AviaryEventFn)aviaryGoEvent, ctx);
}
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

// SetEventHook delivers every chain event as one JSON object, synchronously
// from inside the node call that produced it. While a hook is set the events
// are not buffered for DrainEvents. A nil fn removes the hook.
func (n *Node) SetEventHook(fn func(json string)) {
	if n.handle != 0 {
		n.handle.Delete()
		n.handle = 0
	}
	n.onEvent = fn
	if fn == nil {
		C.aviary_node_set_event_hook(n.h, nil, nil)
		return
	}
	n.handle = cgo.NewHandle(n)
	C.bc_node_hook(unsafe.Pointer(n.h), unsafe.Pointer(n.handle))
}
