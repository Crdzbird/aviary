package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import "errors"

// Snapshot is the whole chain as one blob: chain id, genesis credits and every
// finalized block with its commit certificate. Pending mempool transactions are
// not included.
func (n *Node) Snapshot() ([]byte, error) { return takeBuf(C.aviary_node_snapshot(n.h)) }

// RestoreNode rebuilds a node from Snapshot, replaying every block through
// consensus so each certificate is verified. A malformed blob is an error.
func RestoreNode(snapshot []byte) (*Node, error) {
	ensure()
	h := C.aviary_node_restore(bp(snapshot), C.size_t(len(snapshot)))
	if h == nil {
		return nil, errors.New("node restore refused (malformed snapshot)")
	}
	return &Node{h: h}, nil
}

// BlockAt is the encoded finalized block at height (1 based).
func (n *Node) BlockAt(height uint64) ([]byte, error) {
	return takeBuf(C.aviary_node_block_at(n.h, C.uint64_t(height)))
}
