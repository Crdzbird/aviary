package blockchainer

/*
#include <stdlib.h>
#include "blockchainer_c.h"

extern int blockchainerGoTxSource(void* ctx, uint64_t chainID, uint64_t ts, uint8_t* out, size_t cap, size_t* outLen);
extern void blockchainerGoFinalized(void* ctx, uint64_t chainID, const uint8_t* block, size_t len);
static void bc_tree_hooks(void* t, void* ctx) {
	blockchainer_chain_tree_set_tx_source((BlockchainerChainTree)t, (BlockchainerTxSourceFn)blockchainerGoTxSource, ctx);
	blockchainer_chain_tree_set_finalized_hook((BlockchainerChainTree)t, (BlockchainerFinalizedFn)blockchainerGoFinalized, ctx);
}
*/
import "C"

import (
	"encoding/binary"
	"errors"
	"runtime/cgo"
	"unsafe"
)

// ChainTreeNode declares one chain of a family: its mixture, posture, funded
// accounts, validators (Secret empty for validators this process does not
// hold), anchoring bounds and child chains (subchains — must be BFT).
type ChainTreeNode struct {
	ChainID             uint64
	Blueprint           *Blueprint // nil = bft/account
	AssuranceLevel      uint16     // 0 → 1 (standard)
	Accounts            []GenesisAccount
	Validators          []ChainTreeValidator
	MaxAnchorsPerBlock  uint16
	MaxHeadersPerAnchor uint16
	Children            []ChainTreeNode
}

// ChainTreeValidator is a validator with the secret this process holds (or none).
type ChainTreeValidator struct {
	PublicKey, Secret []byte
	Power             uint64
}

// Encode canonically encodes the node (children first).
func (n ChainTreeNode) Encode() ([]byte, error) {
	ensure()
	blueprint := n.Blueprint
	if blueprint == nil {
		var err error
		if blueprint, err = NewBlueprint("bft", "account", "", nil); err != nil {
			return nil, err
		}
	}
	level := n.AssuranceLevel
	if level == 0 {
		level = 1
	}
	addrs := make([]byte, 0, 32*len(n.Accounts))
	balances := make([]uint64, 0, len(n.Accounts))
	for _, a := range n.Accounts {
		if len(a.Address) != 32 {
			return nil, errors.New("tree: account address must be 32 bytes")
		}
		addrs = append(addrs, a.Address...)
		balances = append(balances, a.Balance)
	}
	pks := make([]byte, 0, 48*len(n.Validators))
	secrets := make([]byte, 0, 32*len(n.Validators))
	powers := make([]uint64, 0, len(n.Validators))
	for _, v := range n.Validators {
		if len(v.PublicKey) != 48 || (len(v.Secret) != 0 && len(v.Secret) != 32) {
			return nil, errors.New("tree: validator key must be 48 bytes and secret 32 or empty")
		}
		pks = append(pks, v.PublicKey...)
		if len(v.Secret) == 32 {
			secrets = append(secrets, v.Secret...)
		} else {
			secrets = append(secrets, make([]byte, 32)...)
		}
		powers = append(powers, v.Power)
	}
	kids := make([][]byte, 0, len(n.Children))
	for _, c := range n.Children {
		e, err := c.Encode()
		if err != nil {
			return nil, err
		}
		kids = append(kids, e)
	}
	blob := encodeBlobList(kids)
	return takeBuf(C.blockchainer_chain_tree_node_encode(C.uint64_t(n.ChainID), bp(blueprint.encoded), C.size_t(len(blueprint.encoded)), C.uint16_t(level),
		bp(addrs), u64p(balances), C.size_t(len(n.Accounts)),
		bp(pks), bp(secrets), u64p(powers), C.size_t(len(n.Validators)),
		C.uint16_t(n.MaxAnchorsPerBlock), C.uint16_t(n.MaxHeadersPerAnchor), bp(blob), C.size_t(len(blob))))
}

// ChainTreeSpecError explains why an encoded spec is structurally invalid ("" = valid).
func ChainTreeSpecError(spec []byte) string {
	ensure()
	b, err := takeBuf(C.blockchainer_chain_tree_spec_error(bp(spec), C.size_t(len(spec))))
	if err != nil {
		return err.Error()
	}
	return string(b)
}

// ChainTree is a built family of chains driven in-process: Produce anchors
// children upward automatically; ProveAccount proves any descendant from the root.
type ChainTree struct {
	h      C.BlockchainerChainTree
	handle cgo.Handle
	// TxSource, if set, supplies transactions when a chain produces.
	TxSource func(chainID, timestamp uint64) [][]byte
	// OnFinalized, if set, observes every finalized block.
	OnFinalized func(chainID uint64, block []byte)
}

// BuildChainTree builds the whole family from a root node. Fails closed with
// the structural reason, or a generic reject when the registry / posture refuses.
func BuildChainTree(root ChainTreeNode) (*ChainTree, error) {
	spec, err := root.Encode()
	if err != nil {
		return nil, err
	}
	if e := ChainTreeSpecError(spec); e != "" {
		return nil, errors.New("tree: " + e)
	}
	h := C.blockchainer_chain_tree_build(bp(spec), C.size_t(len(spec)))
	if h == nil {
		return nil, errors.New("tree: build rejected (unknown mixture or posture-forbidden engine)")
	}
	t := &ChainTree{h: h}
	t.handle = cgo.NewHandle(t)
	C.bc_tree_hooks(unsafe.Pointer(h), unsafe.Pointer(t.handle))
	return t, nil
}

func (t *ChainTree) Free() {
	if t.h != nil {
		C.blockchainer_chain_tree_free(t.h)
		t.h = nil
		t.handle.Delete()
	}
}

// ChainIDs lists the family in pre-order (parents first).
func (t *ChainTree) ChainIDs() ([]uint64, error) {
	b, err := takeBuf(C.blockchainer_chain_tree_chain_ids(t.h))
	if err != nil {
		return nil, err
	}
	n := int(binary.BigEndian.Uint32(b[:4]))
	out := make([]uint64, n)
	for i := 0; i < n; i++ {
		out[i] = binary.BigEndian.Uint64(b[4+8*i:])
	}
	return out, nil
}
func (t *ChainTree) Height(chainID uint64) uint64 { return uint64(C.blockchainer_chain_tree_height(t.h, C.uint64_t(chainID))) }
func (t *ChainTree) Genesis(chainID uint64) ([]byte, error) {
	return takeBuf(C.blockchainer_chain_tree_genesis(t.h, C.uint64_t(chainID)))
}
func (t *ChainTree) Tip(chainID uint64) ([]byte, error) { return takeBuf(C.blockchainer_chain_tree_tip(t.h, C.uint64_t(chainID))) }
func (t *ChainTree) PendingAnchors(chainID uint64) uint32 {
	return uint32(C.blockchainer_chain_tree_pending_anchors(t.h, C.uint64_t(chainID)))
}

// Produce finalizes the next block on chainID (anchoring its children's progress).
func (t *ChainTree) Produce(chainID, timestamp uint64) ([]byte, error) {
	return takeBuf(C.blockchainer_chain_tree_produce(t.h, C.uint64_t(chainID), C.uint64_t(timestamp)))
}

// ProveAccount is a cross-chain proof from the root header (no hops for the root).
func (t *ChainTree) ProveAccount(chainID uint64, address []byte) ([]byte, error) {
	if len(address) != 32 {
		return nil, errors.New("tree: address must be 32 bytes")
	}
	return takeBuf(C.blockchainer_chain_tree_prove_account(t.h, C.uint64_t(chainID), bp(address)))
}

// Verify reports whether proof is committed by the root's current header.
func (t *ChainTree) Verify(proof []byte) bool {
	return C.blockchainer_chain_tree_verify(t.h, bp(proof), C.size_t(len(proof))) == 1
}
