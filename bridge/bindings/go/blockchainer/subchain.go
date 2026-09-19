package blockchainer

/*
#include <stdlib.h>
#include "blockchainer_c.h"
*/
import "C"

import (
	"encoding/binary"
	"errors"
)

// Subchains (docs/design/SUBCHAINS.md): a parent block ANCHORS a child chain's
// finalized headers. A child is registered at the parent's genesis by its
// genesis header + fixed validator set; every parent validator verifies each
// anchored child header with the light-client rule; the child's tip is
// committed in the parent header's subchain root. Cross-chain proofs descend
// hop by hop from a parent header to a state entry on a descendant. v1 anchors
// BFT children only.

// encodeBlobList encodes items as [count u32]([len u32][bytes])×.
func encodeBlobList(items [][]byte) []byte {
	out := make([]byte, 4, 4+8*len(items))
	binary.BigEndian.PutUint32(out, uint32(len(items)))
	for _, it := range items {
		var l [4]byte
		binary.BigEndian.PutUint32(l[:], uint32(len(it)))
		out = append(out, l[:]...)
		out = append(out, it...)
	}
	return out
}

// GenesisHeader is a genesis's genesis HEADER — the identity a parent
// registers a child by (it does not depend on the child's parent binding).
func GenesisHeader(genesis []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_genesis_header(bp(genesis), C.size_t(len(genesis))))
}

// ChildRegistration registers one child at a parent's genesis.
type ChildRegistration struct {
	ChainID       uint64
	GenesisHeader []byte
	Validators    []GenesisValidator // the child's FIXED validator set
	MaxDepth      uint8              // the child's own subtree depth (0 = leaf)
}

// Encode canonically encodes the registration.
func (r ChildRegistration) Encode() ([]byte, error) {
	ensure()
	pks := make([]byte, 0, 48*len(r.Validators))
	powers := make([]uint64, 0, len(r.Validators))
	for _, v := range r.Validators {
		if len(v.PublicKey) != 48 {
			return nil, errors.New("registration: validator key must be 48 bytes")
		}
		pks = append(pks, v.PublicKey...)
		powers = append(powers, v.Power)
	}
	return takeBuf(C.blockchainer_child_registration_encode(C.uint64_t(r.ChainID),
		bp(r.GenesisHeader), C.size_t(len(r.GenesisHeader)),
		bp(pks), u64p(powers), C.size_t(len(r.Validators)), C.uint8_t(r.MaxDepth)))
}

// SubchainPolicy is a parent's genesis-committed policy. 0 = unlimited for the
// per-block / per-anchor bounds. Children must be ascending, unique by chain id.
type SubchainPolicy struct {
	MaxChildren         uint16
	MaxDepth            uint8
	MaxAnchorsPerBlock  uint16
	MaxHeadersPerAnchor uint16
	Children            []ChildRegistration
}

// Encode canonically encodes the policy.
func (p SubchainPolicy) Encode() ([]byte, error) {
	ensure()
	regs := make([][]byte, 0, len(p.Children))
	for _, c := range p.Children {
		e, err := c.Encode()
		if err != nil {
			return nil, err
		}
		regs = append(regs, e)
	}
	blob := encodeBlobList(regs)
	return takeBuf(C.blockchainer_subchain_policy_encode(C.uint16_t(p.MaxChildren), C.uint8_t(p.MaxDepth),
		C.uint16_t(p.MaxAnchorsPerBlock), C.uint16_t(p.MaxHeadersPerAnchor), bp(blob), C.size_t(len(blob))))
}

// CommitSubchainPolicy commits a policy into a genesis (this chain becomes a
// PARENT; its hash changes). Fails on a structurally invalid policy.
func CommitSubchainPolicy(genesis []byte, policy []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_genesis_commit_subchain_policy(bp(genesis), C.size_t(len(genesis)), bp(policy), C.size_t(len(policy))))
}

// CommitParentBinding binds a child genesis to its parent (id + 32-byte parent
// genesis hash); the child's hash changes so it can never be re-parented.
func CommitParentBinding(genesis []byte, parentChainID uint64, parentGenesisHash []byte) ([]byte, error) {
	ensure()
	if len(parentGenesisHash) != 32 {
		return nil, errors.New("binding: parent genesis hash must be 32 bytes")
	}
	return takeBuf(C.blockchainer_genesis_commit_parent_binding(bp(genesis), C.size_t(len(genesis)), C.uint64_t(parentChainID), bp(parentGenesisHash)))
}

// BlockHeader extracts the encoded header of an encoded block.
func BlockHeader(block []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_block_header(bp(block), C.size_t(len(block))))
}

// HeaderSubchainRoot is a header's 32-byte subchain root (zero below v3).
func HeaderSubchainRoot(header []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_header_subchain_root(bp(header), C.size_t(len(header))))
}

// CertifiedHeader pairs a child header with the certificate that finalized it.
func CertifiedHeader(header, certificate []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_certified_header_encode(bp(header), C.size_t(len(header)), bp(certificate), C.size_t(len(certificate))))
}

// EncodeAnchor builds an anchor: one child's CONSECUTIVE certified headers.
func EncodeAnchor(childChainID uint64, certifiedHeaders [][]byte) ([]byte, error) {
	ensure()
	blob := encodeBlobList(certifiedHeaders)
	return takeBuf(C.blockchainer_anchor_encode(C.uint64_t(childChainID), bp(blob), C.size_t(len(blob))))
}

// ProposeAnchored proposes the next block on a PARENT chain, anchoring the
// given anchors (ascending child id). Errors if any anchor is invalid.
func (c *Chain) ProposeAnchored(timestamp uint64, anchors [][]byte) ([]byte, error) {
	blob := encodeBlobList(anchors)
	return takeBuf(C.blockchainer_chain_propose_anchored(c.h, C.uint64_t(timestamp), bp(blob), C.size_t(len(blob))))
}

// Certify (BFT engines) aggregates this validator's precommit into a commit
// certificate for block — the justification for Submit and what a parent
// anchors. Errors if the engine is not BFT or this validator alone lacks quorum.
func (c *Chain) Certify(block []byte) ([]byte, error) {
	return takeBuf(C.blockchainer_chain_certify(c.h, bp(block), C.size_t(len(block))))
}

// ScheduleValidators (BFT) schedules a coordinated validator-set change: the
// next block this node proposes announces it; Certify refuses an unscheduled
// change; a certified rotation takes effect for the next block.
func (c *Chain) ScheduleValidators(validators []GenesisValidator) error {
	pks := make([]byte, 0, 48*len(validators))
	powers := make([]uint64, 0, len(validators))
	for _, v := range validators {
		if len(v.PublicKey) != 48 {
			return errors.New("validators: key must be 48 bytes")
		}
		pks = append(pks, v.PublicKey...)
		powers = append(powers, v.Power)
	}
	if C.blockchainer_chain_schedule_validators(c.h, bp(pks), u64p(powers), C.size_t(len(validators))) != 1 {
		return errors.New("chain: cannot schedule (not BFT, or malformed set)")
	}
	return nil
}

// Validators is the set in force for the next block (BFT).
func (c *Chain) Validators() ([]GenesisValidator, error) {
	blob, err := takeBuf(C.blockchainer_chain_validators(c.h))
	if err != nil {
		return nil, err
	}
	if len(blob) < 4 {
		return nil, errors.New("validators: short blob")
	}
	n := int(binary.BigEndian.Uint32(blob[:4]))
	out := make([]GenesisValidator, 0, n)
	off := 4
	for i := 0; i < n; i++ {
		if off+4 > len(blob) {
			return nil, errors.New("validators: truncated")
		}
		l := int(binary.BigEndian.Uint32(blob[off : off+4]))
		off += 4
		if off+l+8 > len(blob) {
			return nil, errors.New("validators: truncated")
		}
		pk := append([]byte(nil), blob[off:off+l]...)
		off += l
		out = append(out, GenesisValidator{PublicKey: pk, Power: binary.BigEndian.Uint64(blob[off : off+8])})
		off += 8
	}
	return out, nil
}

// ChildTip is a parent's last anchored header for a child.
func (c *Chain) ChildTip(childChainID uint64) ([]byte, error) {
	return takeBuf(C.blockchainer_chain_child_tip(c.h, C.uint64_t(childChainID)))
}

// SubchainHop is a proof hop from this parent's current registry into a child.
func (c *Chain) SubchainHop(childChainID uint64) ([]byte, error) {
	return takeBuf(C.blockchainer_chain_subchain_hop(c.h, C.uint64_t(childChainID)))
}

// ProveAccount is a state-inclusion proof for a 32-byte address on this
// account-model chain.
func (c *Chain) ProveAccount(address []byte) ([]byte, error) {
	if len(address) != 32 {
		return nil, errors.New("prove: address must be 32 bytes")
	}
	return takeBuf(C.blockchainer_chain_prove_account(c.h, bp(address)))
}

// EncodeSubchainProof builds a cross-chain proof from hops (parent → … → the
// chain holding the entry) and that chain's state proof.
func EncodeSubchainProof(hops [][]byte, stateProof []byte) ([]byte, error) {
	ensure()
	blob := encodeBlobList(hops)
	return takeBuf(C.blockchainer_subchain_proof_encode(bp(blob), C.size_t(len(blob)), bp(stateProof), C.size_t(len(stateProof))))
}

// VerifySubchainProof reports whether proof is committed under the parent's
// 32-byte subchain root all the way down and its state proof holds.
func VerifySubchainProof(parentSubchainRoot, proof []byte) bool {
	ensure()
	if len(parentSubchainRoot) != 32 {
		return false
	}
	return C.blockchainer_subchain_proof_verify(bp(parentSubchainRoot), bp(proof), C.size_t(len(proof))) == 1
}
