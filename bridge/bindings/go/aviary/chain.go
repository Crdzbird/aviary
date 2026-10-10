package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import (
	"encoding/binary"
	"errors"
	"unsafe"
)

// Composable chains. A chain is a MIXTURE of pluggable components — a consensus
// engine, a state model, a proposer policy — each named by a stable id. The
// mixture is committed into the genesis hash (it IS chain identity); an id
// nobody registered, or a mix the assurance posture forbids, is rejected at
// assembly (fail-closed). Custom components are C++ subclasses registered
// in-process; from here you can DISCOVER what is registered (EngineIDs etc.)
// and DECLARE mixtures naming it.

// NewBLSValidator derives a validator BLS keypair (public 48 B, secret 32 B)
// from >= 32 bytes of key material.
func NewBLSValidator(ikm []byte) (*PortableAccount, error) {
	ensure()
	return unpackAccountBlob(takeBuf(C.aviary_bls_keygen(bp(ikm), C.size_t(len(ikm)))))
}

// Blueprint is a declared mixture: which engine / state model / proposer policy,
// plus component-interpreted params (e.g. a 32-byte PoW target).
type Blueprint struct {
	Consensus  string
	StateModel string
	Proposer   string // "" = the engine's default
	Params     []byte

	encoded []byte
}

// NewBlueprint canonically encodes a mixture.
func NewBlueprint(consensus, stateModel, proposer string, params []byte) (*Blueprint, error) {
	ensure()
	cc, cs, cp := C.CString(consensus), C.CString(stateModel), C.CString(proposer)
	defer C.free(unsafe.Pointer(cc))
	defer C.free(unsafe.Pointer(cs))
	defer C.free(unsafe.Pointer(cp))
	enc, err := takeBuf(C.aviary_blueprint_encode(cc, cs, cp, bp(params), C.size_t(len(params))))
	if err != nil {
		return nil, err
	}
	return &Blueprint{Consensus: consensus, StateModel: stateModel, Proposer: proposer, Params: params, encoded: enc}, nil
}

// Encoded returns the blueprint's canonical bytes.
func (b *Blueprint) Encoded() []byte { return b.encoded }

// Hash is the 32-byte commitment to the mixture.
func (b *Blueprint) Hash() ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_blueprint_hash(bp(b.encoded), C.size_t(len(b.encoded))))
}

// decodeIDs parses [count u32]([len u32][id])×.
func decodeIDs(blob []byte) ([]string, error) {
	if len(blob) < 4 {
		return nil, errors.New("registry: short id list")
	}
	n := int(binary.BigEndian.Uint32(blob[:4]))
	out := make([]string, 0, n)
	off := 4
	for i := 0; i < n; i++ {
		if off+4 > len(blob) {
			return nil, errors.New("registry: truncated id list")
		}
		l := int(binary.BigEndian.Uint32(blob[off : off+4]))
		off += 4
		if off+l > len(blob) {
			return nil, errors.New("registry: truncated id")
		}
		out = append(out, string(blob[off:off+l]))
		off += l
	}
	return out, nil
}

func registryIDs(kind uint8) ([]string, error) {
	ensure()
	blob, err := takeBuf(C.aviary_registry_ids(C.uint8_t(kind)))
	if err != nil {
		return nil, err
	}
	return decodeIDs(blob)
}

// EngineIDs lists the consensus engines a mixture may name (built-ins plus any
// registered in-process).
func EngineIDs() ([]string, error) { return registryIDs(0) }

// StateModelIDs lists the state models a mixture may name.
func StateModelIDs() ([]string, error) { return registryIDs(1) }

// ProposerIDs lists the proposer policies a mixture may name.
func ProposerIDs() ([]string, error) { return registryIDs(2) }

// GenesisAccount funds an address (32 B) at genesis (a genesis output under the
// UTXO model).
type GenesisAccount struct {
	Address []byte
	Balance uint64
}

// GenesisValidator is one validator: its 48-byte BLS public key and power.
type GenesisValidator struct {
	PublicKey []byte
	Power     uint64
}

// Genesis state models.
const (
	StateModelAccount uint8 = 0
	StateModelUTXO    uint8 = 1
)

func u64p(v []uint64) *C.uint64_t {
	if len(v) == 0 {
		return nil
	}
	return (*C.uint64_t)(unsafe.Pointer(&v[0]))
}

// BuildGenesis builds a minimal genesis: chain params, one assurance posture
// (1 standard / 2 hardened / 3 fortress), seeded state, and validators.
func BuildGenesis(chainID, timestamp uint64, assuranceLevel uint16, stateModel uint8,
	accounts []GenesisAccount, validators []GenesisValidator) ([]byte, error) {
	ensure()
	addrs := make([]byte, 0, 32*len(accounts))
	balances := make([]uint64, 0, len(accounts))
	for _, a := range accounts {
		if len(a.Address) != 32 {
			return nil, errors.New("genesis: account address must be 32 bytes")
		}
		addrs = append(addrs, a.Address...)
		balances = append(balances, a.Balance)
	}
	pks := make([]byte, 0, 48*len(validators))
	powers := make([]uint64, 0, len(validators))
	for _, v := range validators {
		if len(v.PublicKey) != 48 {
			return nil, errors.New("genesis: validator key must be 48 bytes")
		}
		pks = append(pks, v.PublicKey...)
		powers = append(powers, v.Power)
	}
	return takeBuf(C.aviary_genesis_build(C.uint64_t(chainID), C.uint64_t(timestamp),
		C.uint16_t(assuranceLevel), C.uint8_t(stateModel),
		bp(addrs), u64p(balances), C.size_t(len(accounts)),
		bp(pks), u64p(powers), C.size_t(len(validators))))
}

// CommitBlueprint commits a mixture into a genesis; the returned genesis has a
// different hash (the mixture is chain identity).
func CommitBlueprint(genesis []byte, b *Blueprint) ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_genesis_commit_blueprint(bp(genesis), C.size_t(len(genesis)),
		bp(b.encoded), C.size_t(len(b.encoded))))
}

// GenesisHash is the 32-byte chain identity.
func GenesisHash(genesis []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_genesis_hash(bp(genesis), C.size_t(len(genesis))))
}

// Chain is an assembled node: the engine its mixture named, behind the uniform
// Propose → (Seal | Mine) → Submit flow.
type Chain struct{ h C.AviaryChain }

// ErrRejected is returned when assembly is refused: an unknown component id, a
// state id that does not match the genesis seeding, or an engine the assurance
// posture forbids. Fail-closed — never a silent substitution.
var ErrRejected = errors.New("chain: assembly rejected (unknown component id or a mix the posture forbids)")

// AssembleChain assembles a node for validator me from a genesis, using its
// committed mixture (or the posture-driven default).
func AssembleChain(genesis []byte, me uint32, blsPub, blsSec []byte) (*Chain, error) {
	ensure()
	h := C.aviary_chain_assemble(bp(genesis), C.size_t(len(genesis)), C.uint32_t(me),
		bp(blsPub), C.size_t(len(blsPub)), bp(blsSec), C.size_t(len(blsSec)))
	if h == nil {
		return nil, ErrRejected
	}
	return &Chain{h}, nil
}

func (c *Chain) Free()          { C.aviary_chain_free(c.h) }
func (c *Chain) Height() uint64 { return uint64(C.aviary_chain_height(c.h)) }

// Blueprint is the mixture this chain runs (encoded).
func (c *Chain) Blueprint() ([]byte, error) {
	return takeBuf(C.aviary_chain_blueprint(c.h))
}

// Propose the next (empty) block. Errors if this node may not propose now.
func (c *Chain) Propose(timestamp uint64) ([]byte, error) {
	return takeBuf(C.aviary_chain_propose(c.h, C.uint64_t(timestamp)))
}

// Seal returns the leader's justification (PoS / VRF-PoS), or an empty slice
// for engines that do not seal (PoW, BFT, custom).
func (c *Chain) Seal(block []byte) ([]byte, error) {
	return takeBuf(C.aviary_chain_seal(c.h, bp(block), C.size_t(len(block))))
}

// Mine searches a solving nonce (PoW) and returns the solved block.
func (c *Chain) Mine(block []byte) ([]byte, error) {
	return takeBuf(C.aviary_chain_mine(c.h, bp(block), C.size_t(len(block))))
}

// Submit verifies the justification and applies the block atomically.
func (c *Chain) Submit(block, justification []byte) bool {
	return C.aviary_chain_submit(c.h, bp(block), C.size_t(len(block)),
		bp(justification), C.size_t(len(justification))) == 1
}
