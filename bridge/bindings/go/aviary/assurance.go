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

// AssuranceLevel names a network-wide security posture.
type AssuranceLevel uint16

const (
	Standard AssuranceLevel = 1
	Hardened AssuranceLevel = 2
	Fortress AssuranceLevel = 3
)

// AssuranceProfile is the consistent tier a posture mandates at every seam — a
// consumer wires the classes it names.
type AssuranceProfile struct {
	Ratcheted        bool   // transport tier: false = mutual-auth SIGMA, true = PQ ratchet
	VRF              bool   // consensus: false = stake-weighted, true = grind-proof VRF
	ThresholdCustody bool   // account/validator keys must use FROST threshold custody
	SealedMempool    bool   // transactions must be sealed to the proposer (anti-MEV)
	SigAlgos         []byte // the exact SigAlgo bytes permitted on-chain
}

// Describe returns the tiers the given posture mandates.
func Describe(level AssuranceLevel) (AssuranceProfile, error) {
	ensure()
	blob, err := takeBuf(C.aviary_assurance_describe(C.uint16_t(level)))
	if err != nil {
		return AssuranceProfile{}, err
	}
	if len(blob) < 5 || 5+int(blob[4]) > len(blob) {
		return AssuranceProfile{}, errors.New("assurance: bad descriptor blob")
	}
	return AssuranceProfile{
		Ratcheted:        blob[0] == 1,
		VRF:              blob[1] == 1,
		ThresholdCustody: blob[2] == 1,
		SealedMempool:    blob[3] == 1,
		SigAlgos:         append([]byte(nil), blob[5:5+int(blob[4])]...),
	}, nil
}

// EncodeTx encodes a transaction from its fields into the canonical wire form
// (e.g. to seal into a SealedMempool). fee is carried only when version >= 2.
func EncodeTx(version uint8, chainID uint64, sigAlgo uint8, fee uint64, body, author, signature []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_tx_encode(C.uint8_t(version), C.uint64_t(chainID), C.uint8_t(sigAlgo),
		C.uint64_t(fee), bp(body), C.size_t(len(body)), bp(author), C.size_t(len(author)),
		bp(signature), C.size_t(len(signature))))
}

// SealTx seals an encoded transaction to the block proposer's hybrid-KEM public
// key. The cleartext fee hint is the transaction's declared fee.
func SealTx(txEncoded, proposerKemPublic []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.aviary_sealed_tx_seal(bp(txEncoded), C.size_t(len(txEncoded)),
		bp(proposerKemPublic), C.size_t(len(proposerKemPublic))))
}

// SealedMempool is an encrypted mempool: submissions stay opaque (ordered by a
// cleartext fee hint) until the proposer opens the winners at build time.
type SealedMempool struct{ h C.AviarySealedMempool }

func NewSealedMempool(capacity int) *SealedMempool {
	ensure()
	return &SealedMempool{C.aviary_sealed_mempool_new(C.size_t(capacity))}
}

func (m *SealedMempool) Free() { C.aviary_sealed_mempool_free(m.h) }

// Add admits a sealed submission (no decryption). Returns false if full/rejected.
func (m *SealedMempool) Add(sealedTx []byte) bool {
	return C.aviary_sealed_mempool_add(m.h, bp(sealedTx), C.size_t(len(sealedTx))) == 1
}

func (m *SealedMempool) Size() int { return int(C.aviary_sealed_mempool_size(m.h)) }

// Reveal opens up to max highest-fee submissions with the proposer's KEM secret,
// returning the encoded transactions in descending-fee order. Entries that don't
// decrypt / decode, or whose real fee != the committed hint, are skipped.
func (m *SealedMempool) Reveal(proposerKemSecret []byte, max int) ([][]byte, error) {
	blob, err := takeBuf(C.aviary_sealed_mempool_reveal(m.h, bp(proposerKemSecret),
		C.size_t(len(proposerKemSecret)), C.size_t(max)))
	if err != nil {
		return nil, err
	}
	if len(blob) < 4 {
		return nil, errors.New("sealed mempool: short reveal blob")
	}
	count := int(binary.BigEndian.Uint32(blob[:4]))
	txs := make([][]byte, 0, count)
	off := 4
	for i := 0; i < count; i++ {
		if off+4 > len(blob) {
			return nil, errors.New("sealed mempool: truncated reveal blob")
		}
		l := int(binary.BigEndian.Uint32(blob[off : off+4]))
		off += 4
		tx := make([]byte, l)
		copy(tx, blob[off:off+l])
		txs = append(txs, tx)
		off += l
	}
	return txs, nil
}
