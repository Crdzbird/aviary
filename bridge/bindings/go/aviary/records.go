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

// Record operations: what a record transaction does.
const (
	RecordPut      uint8 = 0 // create or update; the value is the new content
	RecordTransfer uint8 = 1 // hand over; the value is the new 32 byte owner address
	RecordDelete   uint8 = 2 // remove; the value is empty
)

// RecordEntry is a committed application record on a chain assembled with the
// "record" state model: its owner, how many writes it has seen and its bytes.
type RecordEntry struct {
	Owner   []byte
	Version uint64
	Value   []byte
}

// RecordLeaf is a record read out of a verified state proof, with its key.
type RecordLeaf struct {
	Key     []byte
	Owner   []byte
	Version uint64
	Value   []byte
}

func decodeRecord(b []byte, withKey bool) (RecordLeaf, error) {
	var r RecordLeaf
	if withKey {
		if len(b) < 32 {
			return r, errors.New("record: short key")
		}
		r.Key, b = b[:32], b[32:]
	}
	if len(b) < 32+8+4 {
		return r, errors.New("record: short header")
	}
	r.Owner = b[:32]
	r.Version = binary.BigEndian.Uint64(b[32:40])
	n := binary.BigEndian.Uint32(b[40:44])
	if len(b) != 44+int(n) {
		return r, errors.New("record: bad value length")
	}
	r.Value = b[44:]
	return r, nil
}

// ChainID is the chain id this assembled chain runs.
func (c *Chain) ChainID() uint64 { return uint64(C.aviary_chain_chain_id(c.h)) }

// StateRoot is the committed 32 byte state root.
func (c *Chain) StateRoot() ([]byte, error) { return takeBuf(C.aviary_chain_state_root(c.h)) }

// AddTx queues a signed, encoded transaction for the next proposal after
// checking it against the chain and the state the earlier queued transactions
// leave; returns the txid. Works for every state model.
func (c *Chain) AddTx(tx []byte) ([]byte, error) {
	return takeBuf(C.aviary_chain_add_tx(c.h, bp(tx), C.size_t(len(tx))))
}

// PendingCount is the number of transactions waiting for the next proposal.
func (c *Chain) PendingCount() uint64 { return uint64(C.aviary_chain_pending_count(c.h)) }

// Record returns the committed record at key, or nil when none exists.
func (c *Chain) Record(key []byte) (*RecordEntry, error) {
	if len(key) != 32 {
		return nil, errors.New("record: key must be 32 bytes")
	}
	b, err := takeBuf(C.aviary_chain_record(c.h, bp(key)))
	if err != nil {
		if errors.Is(err, errNoRecord) || containsNoRecord(err) {
			return nil, nil
		}
		return nil, err
	}
	leaf, err := decodeRecord(b, false)
	if err != nil {
		return nil, err
	}
	return &RecordEntry{Owner: leaf.Owner, Version: leaf.Version, Value: leaf.Value}, nil
}

var errNoRecord = errors.New("chain: no such record")

func containsNoRecord(err error) bool {
	s := err.Error()
	return len(s) >= len("no such record") && indexOf(s, "no such record") >= 0
}

func indexOf(s, sub string) int {
	for i := 0; i+len(sub) <= len(s); i++ {
		if s[i:i+len(sub)] == sub {
			return i
		}
	}
	return -1
}

// RecordNonce is the next nonce address must use, counting queued writes.
func (c *Chain) RecordNonce(address []byte) uint64 {
	if len(address) != 32 {
		return 0
	}
	return uint64(C.aviary_chain_record_nonce(c.h, bp(address)))
}

// ProveRecord is a state inclusion proof for the record at key; verify it with
// VerifyStateProof against StateRoot and read it with RecordFromProof.
func (c *Chain) ProveRecord(key []byte) ([]byte, error) {
	if len(key) != 32 {
		return nil, errors.New("prove: key must be 32 bytes")
	}
	return takeBuf(C.aviary_chain_prove_record(c.h, bp(key)))
}

// Write signs a record write with a 64 byte Ed25519 secret, taking the nonce
// from the chain, and queues it. Returns the txid.
func (c *Chain) Write(secret, key []byte, op uint8, value []byte) ([]byte, error) {
	if len(secret) != 64 {
		return nil, errors.New("write: ed25519 secret must be 64 bytes")
	}
	author, err := AddressOf(secret[32:])
	if err != nil {
		return nil, err
	}
	tx, err := SignRecordTx(c.ChainID(), c.RecordNonce(author), key, op, value, secret)
	if err != nil {
		return nil, err
	}
	return c.AddTx(tx)
}

// SignRecordTx is a signed, encoded record transaction from a 64 byte Ed25519
// secret (seed || public).
func SignRecordTx(chainID, nonce uint64, key []byte, op uint8, value, secret []byte) ([]byte, error) {
	ensure()
	if len(key) != 32 {
		return nil, errors.New("record: key must be 32 bytes")
	}
	return takeBuf(C.aviary_record_tx_sign(C.uint64_t(chainID), C.uint64_t(nonce), bp(key), C.uint8_t(op),
		bp(value), C.size_t(len(value)), bp(secret), C.size_t(len(secret))))
}

// AddressOf is the 32 byte chain address of a public key.
func AddressOf(publicKey []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_address_of(bp(publicKey), C.size_t(len(publicKey))))
}

// RecordFromProof reads the record a verified state proof commits to.
func RecordFromProof(proof []byte) (RecordLeaf, error) {
	ensure()
	b, err := takeBuf(C.aviary_record_from_proof(bp(proof), C.size_t(len(proof))))
	if err != nil {
		return RecordLeaf{}, err
	}
	return decodeRecord(b, true)
}
