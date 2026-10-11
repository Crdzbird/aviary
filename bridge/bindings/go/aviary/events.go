package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"

extern void aviaryGoEvent(void* ctx, const char* json, size_t len);
static void bc_node_hook(void* n, void* ctx) {
	aviary_node_set_event_hook((AviaryNode)n, (AviaryEventFn)aviaryGoEvent, ctx);
}
static void bc_chain_hook(void* c, void* ctx) {
	aviary_chain_set_event_hook((AviaryChain)c, (AviaryEventFn)aviaryGoEvent, ctx);
}
static void bc_net_hook(void* n, void* ctx) {
	aviary_net_node_set_event_hook((AviaryNetNode)n, (AviaryEventFn)aviaryGoEvent, ctx);
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

// eventSink is what every hook handle points at: one callback.
type eventSink struct{ fn func(json string) }

// SetEventHook receives every block event of the chain (block_finalized or
// block_rejected per Submit) as one JSON object; nil removes the hook.
func (c *Chain) SetEventHook(fn func(json string)) {
	if c.handle != 0 {
		c.handle.Delete()
		c.handle = 0
	}
	if fn == nil {
		C.aviary_chain_set_event_hook(c.h, nil, nil)
		return
	}
	c.handle = cgo.NewHandle(&eventSink{fn})
	C.bc_chain_hook(unsafe.Pointer(c.h), unsafe.Pointer(c.handle))
}

// DrainEvents returns the chain's buffered events as a JSON array.
func (c *Chain) DrainEvents() (string, error) {
	b, err := takeBuf(C.aviary_chain_drain_events(c.h))
	return string(b), err
}

// Metrics is a Prometheus text snapshot of the chain.
func (c *Chain) Metrics() (string, error) {
	b, err := takeBuf(C.aviary_chain_metrics(c.h))
	return string(b), err
}

// SetEventHook receives a block_finalized event per committed height; nil
// removes the hook.
func (n *NetNode) SetEventHook(fn func(json string)) {
	if n.handle != 0 {
		n.handle.Delete()
		n.handle = 0
	}
	if fn == nil {
		C.aviary_net_node_set_event_hook(n.h, nil, nil)
		return
	}
	n.handle = cgo.NewHandle(&eventSink{fn})
	C.bc_net_hook(unsafe.Pointer(n.h), unsafe.Pointer(n.handle))
}

// DrainEvents returns the networked node's buffered events as a JSON array.
func (n *NetNode) DrainEvents() (string, error) {
	b, err := takeBuf(C.aviary_net_node_drain_events(n.h))
	return string(b), err
}

// Metrics is a Prometheus text snapshot: chain height, blocks finalized,
// gossip received, relayed and dropped, timeouts fired, peers banned.
func (n *NetNode) Metrics() (string, error) {
	b, err := takeBuf(C.aviary_net_node_metrics(n.h))
	return string(b), err
}
