package blockchainer

/*
#include <stdlib.h>
#include "blockchainer_c.h"
*/
import "C"

import (
	"encoding/binary"
	"errors"
	"unsafe"
)

// ShareCard is one participant's share of a threshold (FROST t-of-n) account —
// an opaque, self-describing blob. Persist it sealed (SealShare); never store it
// plaintext. The account's secret key never exists: shares sign, the group key
// signs nothing.
type ShareCard []byte

// Commitment is a participant's round-1 public commitment (66 bytes: id + two
// points), safe to broadcast to the coordinator.
type Commitment []byte

// Nonces are a participant's round-1 SECRET nonces (64 bytes). Use once, discard.
type Nonces []byte

// CreateThresholdAccount deals a t-of-n account: n share cards, any t of which
// can sign. The dealt group secret is zeroised before this returns.
func CreateThresholdAccount(n, t uint16) ([]ShareCard, error) {
	ensure()
	blob, err := takeBuf(C.blockchainer_threshold_create(C.uint16_t(n), C.uint16_t(t)))
	if err != nil {
		return nil, err
	}
	if len(blob) < 2 {
		return nil, errors.New("threshold: short create blob")
	}
	count := int(binary.BigEndian.Uint16(blob[:2]))
	cards := make([]ShareCard, 0, count)
	off := 2
	for i := 0; i < count; i++ {
		if off+4 > len(blob) {
			return nil, errors.New("threshold: truncated create blob")
		}
		l := int(binary.BigEndian.Uint32(blob[off : off+4]))
		off += 4
		if off+l > len(blob) {
			return nil, errors.New("threshold: bad card length")
		}
		card := make(ShareCard, l)
		copy(card, blob[off:off+l])
		cards = append(cards, card)
		off += l
	}
	return cards, nil
}

// Index, GroupPublicKey (the account's on-chain author key) and PublicShare of a
// card, read via the C ABI so no codec is re-implemented here.
func (c ShareCard) info() (index uint16, groupPK, publicShare []byte, err error) {
	ensure()
	blob, e := takeBuf(C.blockchainer_threshold_share_info(bp(c), C.size_t(len(c))))
	if e != nil {
		return 0, nil, nil, e
	}
	if len(blob) != 66 { // [id u16][group_pk 32][public 32]
		return 0, nil, nil, errors.New("threshold: bad share-info blob")
	}
	return binary.BigEndian.Uint16(blob[:2]), blob[2:34], blob[34:66], nil
}

func (c ShareCard) Index() uint16          { i, _, _, _ := c.info(); return i }
func (c ShareCard) GroupPublicKey() []byte { _, g, _, _ := c.info(); return g }
func (c ShareCard) PublicShare() []byte    { _, _, p, _ := c.info(); return p }

// SealShare seals the card for rest under a passphrase (ops=mem=0 → SENSITIVE).
func (c ShareCard) Seal(passphrase string, ops uint64, mem uint) ([]byte, error) {
	ensure()
	cp := C.CString(passphrase)
	defer C.free(unsafe.Pointer(cp))
	return takeBuf(C.blockchainer_threshold_share_seal(bp(c), C.size_t(len(c)), cp, C.uint64_t(ops), C.size_t(mem)))
}

// UnsealShare opens a sealed card. Fails closed on wrong passphrase / tamper.
func UnsealShare(envelope []byte, passphrase string) (ShareCard, error) {
	ensure()
	cp := C.CString(passphrase)
	defer C.free(unsafe.Pointer(cp))
	b, err := takeBuf(C.blockchainer_threshold_share_unseal(bp(envelope), C.size_t(len(envelope)), cp))
	return ShareCard(b), err
}

// Commit runs round 1 for a card: returns the SECRET nonces (keep) and the public
// commitment (broadcast to the coordinator).
func (c ShareCard) Commit() (Nonces, Commitment, error) {
	ensure()
	blob, err := takeBuf(C.blockchainer_threshold_commit(bp(c), C.size_t(len(c))))
	if err != nil {
		return nil, nil, err
	}
	if len(blob) != 130 { // 32 + 32 nonces, 66 commitment
		return nil, nil, errors.New("threshold: bad commit blob")
	}
	return Nonces(blob[:64]), Commitment(blob[64:]), nil
}

// Ceremony is a two-round FROST signing ceremony over a fixed message and roster.
type Ceremony struct{ h C.BlockchainerCeremony }

// NewCeremony opens a ceremony over [message] and the participating commitments.
func NewCeremony(groupPK, message []byte, commitments []Commitment) (*Ceremony, error) {
	ensure()
	var roster []byte
	for _, cm := range commitments {
		roster = append(roster, cm...)
	}
	h := C.blockchainer_threshold_ceremony_new(bp(groupPK), bp(message), C.size_t(len(message)),
		bp(roster), C.size_t(len(roster)))
	if h == nil {
		return nil, errors.New("threshold: ceremony creation failed")
	}
	return &Ceremony{h}, nil
}

func (c *Ceremony) Free() { C.blockchainer_threshold_ceremony_free(c.h) }

// Sign runs round 2 for [card] using its round-1 [nonces]: a 32-byte share.
func (c *Ceremony) Sign(card ShareCard, nonces Nonces) ([]byte, error) {
	return takeBuf(C.blockchainer_threshold_sign(c.h, bp(card), C.size_t(len(card)), bp(nonces)))
}

// VerifyShare attributes a share to participant [id] (its public share): a lying
// cosigner is named, not just rejected.
func (c *Ceremony) VerifyShare(id uint16, publicShare, sigShare []byte) bool {
	return C.blockchainer_threshold_verify_share(c.h, C.uint16_t(id), bp(publicShare), bp(sigShare)) == 1
}

// Aggregate combines (id → share) pairs into a standard 64-byte Ed25519 signature
// under the group public key. The pairs may be in any order.
func (c *Ceremony) Aggregate(ids []uint16, sigShares [][]byte) ([]byte, error) {
	if len(ids) != len(sigShares) {
		return nil, errors.New("threshold: ids/shares length mismatch")
	}
	flat := make([]byte, 0, len(sigShares)*32)
	for _, s := range sigShares {
		flat = append(flat, s...)
	}
	cids := make([]C.uint16_t, len(ids))
	for i, id := range ids {
		cids[i] = C.uint16_t(id)
	}
	var idPtr *C.uint16_t
	if len(cids) > 0 {
		idPtr = &cids[0]
	}
	return takeBuf(C.blockchainer_threshold_aggregate(c.h, idPtr, bp(flat), C.size_t(len(ids))))
}
