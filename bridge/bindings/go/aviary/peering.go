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

// SignRecord is a record write signed by the stored account at from (record
// state model). The scheme follows the stored key; the secret stays in the
// engine. It fails when the store is locked or from is unknown.
func (k *KeyStore) SignRecord(chainID uint64, from []byte, nonce uint64, key []byte, op uint8, value []byte) ([]byte, error) {
	if len(from) != 32 || len(key) != 32 {
		return nil, errors.New("keystore: author and key must be 32 bytes")
	}
	return takeBuf(C.aviary_keystore_sign_record(k.h, C.uint64_t(chainID), bp(from), C.uint64_t(nonce),
		bp(key), C.uint8_t(op), bp(value), C.size_t(len(value))))
}

// WriteFromStore signs a record write with the key store account at from,
// taking the nonce from the chain, and queues it. Returns the txid.
func (c *Chain) WriteFromStore(store *KeyStore, from, key []byte, op uint8, value []byte) ([]byte, error) {
	tx, err := store.SignRecord(c.ChainID(), from, c.RecordNonce(from), key, op, value)
	if err != nil {
		return nil, err
	}
	return c.AddTx(tx)
}

// SignRecordTxHybrid is a record write signed by a hybrid Ed25519 + ML-DSA-65
// account (NewAccountHybrid), in the version 2 envelope.
func SignRecordTxHybrid(chainID, nonce uint64, key []byte, op uint8, value, publicKey, secret []byte) ([]byte, error) {
	ensure()
	if len(key) != 32 {
		return nil, errors.New("record: key must be 32 bytes")
	}
	return takeBuf(C.aviary_record_tx_sign_hybrid(C.uint64_t(chainID), C.uint64_t(nonce), bp(key), C.uint8_t(op),
		bp(value), C.size_t(len(value)), bp(publicKey), C.size_t(len(publicKey)), bp(secret), C.size_t(len(secret))))
}

// PeerRecordInfo is a peer record's fields.
type PeerRecordInfo struct {
	ID                           uint32
	Host                         string
	Port                         uint16
	KemPublic, SigPublic, Prekey []byte
	Signature                    []byte
}

// SignPeerRecord is a record for validator id at host:port, signed with the
// identity's hybrid signature key; prekey may be nil.
func SignPeerRecord(id uint32, host string, port uint16, identity SecureIdentity, prekey []byte) ([]byte, error) {
	ensure()
	ch := C.CString(host)
	defer C.free(unsafe.Pointer(ch))
	return takeBuf(C.aviary_peer_record_sign(C.uint32_t(id), ch, C.uint16_t(port),
		bp(identity.KemPublic), C.size_t(len(identity.KemPublic)), bp(identity.SigPublic), C.size_t(len(identity.SigPublic)),
		bp(prekey), C.size_t(len(prekey)), bp(identity.SigSecret), C.size_t(len(identity.SigSecret))))
}

// VerifyPeerRecord reports whether record decodes and its owner signature verifies.
func VerifyPeerRecord(record []byte) bool {
	ensure()
	return C.aviary_peer_record_verify(bp(record), C.size_t(len(record))) == 1
}

// ReadPeerRecord returns the fields of an encoded record.
func ReadPeerRecord(record []byte) (PeerRecordInfo, error) {
	ensure()
	b, err := takeBuf(C.aviary_peer_record_fields(bp(record), C.size_t(len(record))))
	if err != nil {
		return PeerRecordInfo{}, err
	}
	f, err := splitBlobList(b)
	if err != nil || len(f) != 7 || len(f[0]) != 4 || len(f[2]) != 2 {
		return PeerRecordInfo{}, errors.New("peer record: malformed fields")
	}
	return PeerRecordInfo{binary.BigEndian.Uint32(f[0]), string(f[1]), binary.BigEndian.Uint16(f[2]), f[3], f[4], f[5], f[6]}, nil
}

// AddressBook is the set of peer records a node knows. Add is the trusted
// local path; Merge is the peer exchange boundary, which verifies every record
// and never overwrites a known id.
type AddressBook struct{ h C.AviaryAddressBook }

// NewAddressBook is an empty book.
func NewAddressBook() *AddressBook { ensure(); return &AddressBook{C.aviary_address_book_new()} }

// DecodeAddressBook decodes a canonical book without verifying it; merge it
// into a book to verify.
func DecodeAddressBook(encoded []byte) (*AddressBook, error) {
	ensure()
	h := C.aviary_address_book_decode(bp(encoded), C.size_t(len(encoded)))
	if h == nil {
		return nil, errors.New("address book: malformed encoding")
	}
	return &AddressBook{h}, nil
}

// Add inserts an encoded record without verifying it.
func (b *AddressBook) Add(record []byte) error {
	if C.aviary_address_book_add(b.h, bp(record), C.size_t(len(record))) != 0 {
		return errors.New("address book: malformed record")
	}
	return nil
}

// Merge learns the verified records of an encoded book that are not yet known
// and returns how many were added.
func (b *AddressBook) Merge(encoded []byte) (int, error) {
	n := int64(C.aviary_address_book_merge(b.h, bp(encoded), C.size_t(len(encoded))))
	if n < 0 {
		return 0, errors.New("address book: malformed book")
	}
	return int(n), nil
}

// Record is the encoded record for id, or nil when unknown.
func (b *AddressBook) Record(id uint32) []byte {
	if !b.Contains(id) {
		return nil
	}
	r, _ := takeBuf(C.aviary_address_book_get(b.h, C.uint32_t(id)))
	return r
}

// Contains reports whether id is known.
func (b *AddressBook) Contains(id uint32) bool {
	return C.aviary_address_book_contains(b.h, C.uint32_t(id)) == 1
}

// Len is the number of known peers.
func (b *AddressBook) Len() int { return int(C.aviary_address_book_size(b.h)) }

// Records are every encoded record in ascending id order.
func (b *AddressBook) Records() ([][]byte, error) {
	r, err := takeBuf(C.aviary_address_book_records(b.h))
	if err != nil {
		return nil, err
	}
	return splitBlobList(r)
}

// Encode is the canonical encoding.
func (b *AddressBook) Encode() ([]byte, error) { return takeBuf(C.aviary_address_book_encode(b.h)) }

// Free releases the book.
func (b *AddressBook) Free() { C.aviary_address_book_free(b.h) }
