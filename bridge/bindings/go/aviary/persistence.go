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

// Tip is this chain's current tip header (the genesis header before any block).
func (c *Chain) Tip() ([]byte, error) { return takeBuf(C.aviary_chain_tip(c.h)) }

// Snapshot is everything this chain needs to resume after a restart: the
// ledger (tip, state, subchain registry), the receipts of the tip block, the
// engine's consensus state and, when enabled, the block archive. It holds no
// secret key and is bound to the genesis hash. Transactions queued with AddTx
// are not included.
func (c *Chain) Snapshot() ([]byte, error) { return takeBuf(C.aviary_chain_snapshot(c.h)) }

// RestoreChain rebuilds a chain from the genesis it was assembled from and a
// Snapshot, as validator me. A restored chain produces byte for byte what the
// original would. On a refused snapshot the error carries the reason.
func RestoreChain(genesis []byte, me uint32, blsPub, blsSec, snapshot []byte) (*Chain, error) {
	ensure()
	h := C.aviary_chain_restore(bp(genesis), C.size_t(len(genesis)), C.uint32_t(me),
		bp(blsPub), C.size_t(len(blsPub)), bp(blsSec), C.size_t(len(blsSec)),
		bp(snapshot), C.size_t(len(snapshot)))
	if h != nil {
		return &Chain{h: h}, nil
	}
	if reason := ChainSnapshotError(genesis, snapshot); reason != "" {
		return nil, errors.New("chain restore refused: " + reason)
	}
	return nil, errors.New("chain restore refused")
}

// ChainSnapshotError says why RestoreChain would refuse snapshot under
// genesis; the empty string means it would be accepted.
func ChainSnapshotError(genesis, snapshot []byte) string {
	ensure()
	b, err := takeBuf(C.aviary_chain_snapshot_error(bp(genesis), C.size_t(len(genesis)), bp(snapshot), C.size_t(len(snapshot))))
	if err != nil {
		return err.Error()
	}
	return string(b)
}

// EnableArchive starts keeping every block this chain commits from now on,
// with its commit certificate, inside the handle and its snapshots. BFT
// chains only.
func (c *Chain) EnableArchive() error {
	if C.aviary_chain_archive_enable(c.h) != 1 {
		return errors.New("chain: the archive needs a BFT engine")
	}
	return nil
}

// ArchiveRange is the archived height range; ok is false while it is empty.
func (c *Chain) ArchiveRange() (first, last uint64, ok bool) {
	var lo, hi C.uint64_t
	if C.aviary_chain_archive_range(c.h, &lo, &hi) != 1 {
		return 0, 0, false
	}
	return uint64(lo), uint64(hi), true
}

// ArchivedBlock is the archived encoded block at height.
func (c *Chain) ArchivedBlock(height uint64) ([]byte, error) {
	return takeBuf(C.aviary_chain_archived_block(c.h, C.uint64_t(height)))
}

// ArchivedCertificate is the commit certificate of the archived block at height.
func (c *Chain) ArchivedCertificate(height uint64) ([]byte, error) {
	return takeBuf(C.aviary_chain_archived_certificate(c.h, C.uint64_t(height)))
}

// PruneArchive drops archived blocks below height and returns how many.
func (c *Chain) PruneArchive(height uint64) uint64 {
	return uint64(C.aviary_chain_archive_prune(c.h, C.uint64_t(height)))
}
