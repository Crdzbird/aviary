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

// RestoreNodeReplay is RestoreNode but always replays the archived blocks
// through consensus, verifying every certificate (slow; for audits and tests).
func RestoreNodeReplay(snapshot []byte) (*Node, error) {
	ensure()
	h := C.aviary_node_restore_replay(bp(snapshot), C.size_t(len(snapshot)))
	if h == nil {
		return nil, errors.New("node restore refused (malformed snapshot)")
	}
	return &Node{h: h}, nil
}

// NextNonce is the nonce the next transfer from address must carry: the
// committed nonce advanced past every transfer from it pending in the mempool.
func (n *Node) NextNonce(address []byte) uint64 {
	if len(address) != 32 {
		return 0
	}
	return uint64(C.aviary_node_next_nonce(n.h, bp(address)))
}
