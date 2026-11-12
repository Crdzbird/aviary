package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import (
	"encoding/binary"
	"errors"
)

// Subchains version 2 (docs/design/SUBCHAINS.md Part 2). A parent whose genesis
// commits a DYNAMIC policy registers children in any block through signed
// registry operations, commits its registry as a Merkle tree with one fixed
// size record per child, and can close a child. A proof for one child is its
// record and about log2(n) hashes however many children the parent holds.

// Signature schemes a registrar key may use (the transaction schemes).
const (
	SigAlgoEd25519 uint8 = 1
	SigAlgoHybrid  uint8 = 2
	SigAlgoSLHDSA  uint8 = 4
)

// Child record statuses.
const (
	ChildOpen   uint8 = 1
	ChildClosed uint8 = 2
)

// Proof hop kinds of a version 2 cross-chain proof.
const (
	HopFlat   uint8 = 1
	HopRecord uint8 = 2
)

// Registrar is a key allowed to authorise registry operations.
type Registrar struct {
	SigAlgo   uint8
	PublicKey []byte
}

// Encode canonically encodes the registrar.
func (r Registrar) Encode() ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_registrar_encode(C.uint8_t(r.SigAlgo), bp(r.PublicKey), C.size_t(len(r.PublicKey))))
}

func encodeRegistrars(registrars []Registrar) ([]byte, error) {
	if len(registrars) == 0 {
		return nil, nil
	}
	items := make([][]byte, 0, len(registrars))
	for _, r := range registrars {
		e, err := r.Encode()
		if err != nil {
			return nil, err
		}
		items = append(items, e)
	}
	return encodeBlobList(items), nil
}

// DynamicSubchainPolicy is a parent's genesis-committed policy of version 2.
// MaxChildren bounds OPEN children; 0 means unlimited for every bound. No
// registrars with threshold 0 is a registry sealed at genesis. Children are
// registered at genesis and must be ascending, unique by chain id.
type DynamicSubchainPolicy struct {
	MaxDepth            uint8
	MaxChildren         uint32
	MaxAnchorsPerBlock  uint16
	MaxHeadersPerAnchor uint16
	MaxOpsPerBlock      uint16
	Threshold           uint8
	Registrars          []Registrar
	Children            []ChildRegistration
}

// Encode canonically encodes the policy; commit it with CommitSubchainPolicy.
func (p DynamicSubchainPolicy) Encode() ([]byte, error) {
	ensure()
	registrars, err := encodeRegistrars(p.Registrars)
	if err != nil {
		return nil, err
	}
	var children []byte
	if len(p.Children) > 0 {
		regs := make([][]byte, 0, len(p.Children))
		for _, c := range p.Children {
			e, err := c.Encode()
			if err != nil {
				return nil, err
			}
			regs = append(regs, e)
		}
		children = encodeBlobList(regs)
	}
	return takeBuf(C.aviary_dynamic_subchain_policy_encode(C.uint8_t(p.MaxDepth), C.uint32_t(p.MaxChildren),
		C.uint16_t(p.MaxAnchorsPerBlock), C.uint16_t(p.MaxHeadersPerAnchor), C.uint16_t(p.MaxOpsPerBlock),
		C.uint8_t(p.Threshold), bp(registrars), C.size_t(len(registrars)), bp(children), C.size_t(len(children))))
}

// EncodedHeaderHash is the canonical 32 byte hash of an encoded header of any
// version: what a record's TipHash and GenesisHeaderHash hold.
func EncodedHeaderHash(header []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_encoded_header_hash(bp(header), C.size_t(len(header))))
}

// OpRegister is an UNSIGNED operation registering a child (an encoded
// ChildRegistration), valid in authorityEpoch up to parent height validUntil
// (0 = no expiry).
func OpRegister(authorityEpoch uint32, validUntil uint64, registration []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_subchain_op_register(C.uint32_t(authorityEpoch), C.uint64_t(validUntil),
		bp(registration), C.size_t(len(registration))))
}

// OpClose is an UNSIGNED operation closing chainID at its recorded tip.
func OpClose(authorityEpoch uint32, validUntil, chainID, finalHeight uint64, finalTipHash []byte) ([]byte, error) {
	ensure()
	if len(finalTipHash) != 32 {
		return nil, errors.New("operation: final tip hash must be 32 bytes")
	}
	return takeBuf(C.aviary_subchain_op_close(C.uint32_t(authorityEpoch), C.uint64_t(validUntil),
		C.uint64_t(chainID), C.uint64_t(finalHeight), bp(finalTipHash)))
}

// OpSetRegistrars is an UNSIGNED operation replacing the registrar set.
// Applying it increments the authority epoch, which voids every operation
// signed for the old one.
func OpSetRegistrars(authorityEpoch uint32, validUntil uint64, threshold uint8, registrars []Registrar) ([]byte, error) {
	ensure()
	blob, err := encodeRegistrars(registrars)
	if err != nil {
		return nil, err
	}
	return takeBuf(C.aviary_subchain_op_set_registrars(C.uint32_t(authorityEpoch), C.uint64_t(validUntil),
		C.uint8_t(threshold), bp(blob), C.size_t(len(blob))))
}

// OpDigest is the 32 byte digest registrars sign for op on the parent
// identified by its chain id and genesis hash.
func OpDigest(op []byte, parentChainID uint64, parentGenesisHash []byte) ([]byte, error) {
	ensure()
	if len(parentGenesisHash) != 32 {
		return nil, errors.New("operation: parent genesis hash must be 32 bytes")
	}
	return takeBuf(C.aviary_subchain_op_digest(bp(op), C.size_t(len(op)), C.uint64_t(parentChainID), bp(parentGenesisHash)))
}

// SignOp signs op as registrar registrarIndex with secret under sigAlgo and
// returns the operation with that approval added.
func SignOp(op []byte, parentChainID uint64, parentGenesisHash []byte, registrarIndex uint16, sigAlgo uint8, secret []byte) ([]byte, error) {
	ensure()
	if len(parentGenesisHash) != 32 {
		return nil, errors.New("operation: parent genesis hash must be 32 bytes")
	}
	return takeBuf(C.aviary_subchain_op_sign(bp(op), C.size_t(len(op)), C.uint64_t(parentChainID),
		bp(parentGenesisHash), C.uint16_t(registrarIndex), C.uint8_t(sigAlgo), bp(secret), C.size_t(len(secret))))
}

// AddOpApproval adds an approval whose signature over OpDigest was made
// elsewhere.
func AddOpApproval(op []byte, registrarIndex uint16, signature []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_subchain_op_add_approval(bp(op), C.size_t(len(op)), C.uint16_t(registrarIndex),
		bp(signature), C.size_t(len(signature))))
}

func optionalList(items [][]byte) []byte {
	if len(items) == 0 {
		return nil
	}
	return encodeBlobList(items)
}

// ProposeSubchain proposes the next block on a parent with a dynamic policy:
// anchors (ascending child id) and signed operations, applied operations
// first. Errors with the reason if any anchor or operation is invalid.
func (c *Chain) ProposeSubchain(timestamp uint64, anchors, ops [][]byte) ([]byte, error) {
	a, o := optionalList(anchors), optionalList(ops)
	return takeBuf(C.aviary_chain_propose_subchain(c.h, C.uint64_t(timestamp), bp(a), C.size_t(len(a)), bp(o), C.size_t(len(o))))
}

// RegistryCounts are the sizes and the authority epoch of a dynamic registry.
type RegistryCounts struct {
	Records        uint32
	Open           uint32
	AuthorityEpoch uint32
}

// RegistryCounts reports the dynamic registry's counts; ok is false when this
// chain has no dynamic policy.
func (c *Chain) RegistryCounts() (counts RegistryCounts, ok bool) {
	var records, open, epoch C.uint32_t
	if C.aviary_chain_registry_counts(c.h, &records, &open, &epoch) != 1 {
		return RegistryCounts{}, false
	}
	return RegistryCounts{Records: uint32(records), Open: uint32(open), AuthorityEpoch: uint32(epoch)}, true
}

// ChildRecord is one child's entry in a dynamic registry: what the parent
// commits about it and what a proof carries.
type ChildRecord struct {
	Index             uint32
	ChainID           uint64
	Status            uint8
	MaxDepth          uint8
	GenesisHeaderHash []byte
	RegisteredHeight  uint64
	ClosedHeight      uint64
	TipHeight         uint64
	TipHash           []byte
	TipStateRoot      []byte
	TipSubchainRoot   []byte
	VsetHash          []byte
	TipVsetHash       []byte
	Encoded           []byte
}

// childRecordSize is the fixed size of an encoded record.
const childRecordSize = 230

// DecodeChildRecord parses the fixed 230 byte record layout.
func DecodeChildRecord(b []byte) (*ChildRecord, error) {
	if len(b) != childRecordSize {
		return nil, errors.New("child record: must be 230 bytes")
	}
	cut := func(from int) []byte { return append([]byte(nil), b[from:from+32]...) }
	return &ChildRecord{
		Index:             binary.BigEndian.Uint32(b[0:4]),
		ChainID:           binary.BigEndian.Uint64(b[4:12]),
		Status:            b[12],
		MaxDepth:          b[13],
		GenesisHeaderHash: cut(14),
		RegisteredHeight:  binary.BigEndian.Uint64(b[46:54]),
		ClosedHeight:      binary.BigEndian.Uint64(b[54:62]),
		TipHeight:         binary.BigEndian.Uint64(b[62:70]),
		TipHash:           cut(70),
		TipStateRoot:      cut(102),
		TipSubchainRoot:   cut(134),
		VsetHash:          cut(166),
		TipVsetHash:       cut(198),
		Encoded:           append([]byte(nil), b...),
	}, nil
}

// ChildRecord is the current record of a child; an error when this chain has
// no dynamic policy or the child was never registered.
func (c *Chain) ChildRecord(childChainID uint64) (*ChildRecord, error) {
	b, err := takeBuf(C.aviary_chain_child_record(c.h, C.uint64_t(childChainID)))
	if err != nil {
		return nil, err
	}
	return DecodeChildRecord(b)
}

// ChildProof proves that a child's record is committed by this parent's
// CURRENT subchain root.
func (c *Chain) ChildProof(childChainID uint64) ([]byte, error) {
	return takeBuf(C.aviary_chain_child_proof(c.h, C.uint64_t(childChainID)))
}

// VerifyChildProof checks a child proof against a 32 byte version 4 subchain
// root taken from a parent header the caller trusts, and returns the proven
// record. Read it: its tip, and whether the child is open.
func VerifyChildProof(subchainRoot, proof []byte) (*ChildRecord, error) {
	ensure()
	if len(subchainRoot) != 32 {
		return nil, errors.New("child proof: subchain root must be 32 bytes")
	}
	b, err := takeBuf(C.aviary_child_proof_verify(bp(subchainRoot), bp(proof), C.size_t(len(proof))))
	if err != nil {
		return nil, err
	}
	return DecodeChildRecord(b)
}

// PendingAnchor is the next anchor parent can take for child, built from the
// child's archive (EnableArchive) and the parent's recorded tip, at most limit
// headers (0 = the policy bound). It is nil when nothing is pending.
func PendingAnchor(parent, child *Chain, limit uint16) ([]byte, error) {
	b, err := takeBuf(C.aviary_chain_pending_anchor(parent.h, child.h, C.uint16_t(limit)))
	if err != nil {
		return nil, err
	}
	if len(b) == 0 {
		return nil, nil
	}
	return b, nil
}

// SubchainHopV2 is a proof hop from this parent's current registry into a
// child, tagged with its kind (HopFlat or HopRecord) in its first byte.
func (c *Chain) SubchainHopV2(childChainID uint64) ([]byte, error) {
	return takeBuf(C.aviary_chain_subchain_hop_v2(c.h, C.uint64_t(childChainID)))
}

// EncodeSubchainProofV2 builds a cross-chain proof through any mixture of
// parents from tagged hops (top parent first) and the last chain's state proof.
func EncodeSubchainProofV2(hops [][]byte, stateProof []byte) ([]byte, error) {
	ensure()
	blob := optionalList(hops)
	return takeBuf(C.aviary_subchain_proof_v2_encode(bp(blob), C.size_t(len(blob)), bp(stateProof), C.size_t(len(stateProof))))
}

// VerifySubchainProofV2 reports whether proof is committed under the parent's
// 32 byte subchain root all the way down and its state proof holds.
func VerifySubchainProofV2(parentSubchainRoot, proof []byte) bool {
	ensure()
	if len(parentSubchainRoot) != 32 {
		return false
	}
	return C.aviary_subchain_proof_v2_verify(bp(parentSubchainRoot), bp(proof), C.size_t(len(proof))) == 1
}
