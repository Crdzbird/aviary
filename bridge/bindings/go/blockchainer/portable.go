package blockchainer

/*
#include <stdlib.h>
#include "blockchainer_c.h"
*/
import "C"

import (
	"errors"
	"unsafe"
)

// PortableAccount is one signing account — its public key and secret — that can
// be moved off the node under a chosen trust tier. Every seal is authenticated
// and fails closed on tamper / wrong key / wrong passphrase.
type PortableAccount struct {
	Public []byte
	Secret []byte
}

// unpackAccountBlob decodes the C ABI account layout [pub_len:4 BE][pub][secret].
func unpackAccountBlob(blob []byte, err error) (*PortableAccount, error) {
	if err != nil {
		return nil, err
	}
	if len(blob) < 4 {
		return nil, errors.New("portable: short buffer")
	}
	pl := int(blob[0])<<24 | int(blob[1])<<16 | int(blob[2])<<8 | int(blob[3])
	if 4+pl > len(blob) {
		return nil, errors.New("portable: bad public length")
	}
	return &PortableAccount{Public: blob[4 : 4+pl], Secret: blob[4+pl:]}, nil
}

// HybridKEMKeypair generates a device key (X25519 + ML-KEM-768) for the
// recipient-bound export tiers. Returns (public, secret).
func HybridKEMKeypair() (public, secret []byte, err error) {
	ensure()
	a, e := unpackAccountBlob(takeBuf(C.blockchainer_hybrid_kem_keygen()))
	if e != nil {
		return nil, nil, e
	}
	return a.Public, a.Secret, nil
}

// SealPassphrase seals the account under a passphrase (MolecularVault + Argon2id).
// Pass ops=0, mem=0 for the SENSITIVE preset.
func (a PortableAccount) SealPassphrase(passphrase string, ops uint64, mem uint) ([]byte, error) {
	ensure()
	cp := C.CString(passphrase)
	defer C.free(unsafe.Pointer(cp))
	return takeBuf(C.blockchainer_portable_seal_passphrase(
		bp(a.Public), C.size_t(len(a.Public)), bp(a.Secret), C.size_t(len(a.Secret)),
		cp, C.uint64_t(ops), C.size_t(mem)))
}

// OpenPassphrase opens a passphrase envelope. Errors on wrong passphrase / tamper.
func OpenPassphrase(envelope []byte, passphrase string) (*PortableAccount, error) {
	ensure()
	cp := C.CString(passphrase)
	defer C.free(unsafe.Pointer(cp))
	return unpackAccountBlob(takeBuf(C.blockchainer_portable_open_passphrase(
		bp(envelope), C.size_t(len(envelope)), cp)))
}

// SealTo seals the account to a device's hybrid-KEM public key (recipient-bound,
// PQ). Only that device's KEM secret can open it.
func (a PortableAccount) SealTo(recipientKEMPublic []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_portable_seal_to(
		bp(a.Public), C.size_t(len(a.Public)), bp(a.Secret), C.size_t(len(a.Secret)),
		bp(recipientKEMPublic), C.size_t(len(recipientKEMPublic))))
}

// OpenTo opens a recipient-bound envelope with the device's KEM secret.
func OpenTo(envelope, recipientKEMSecret []byte) (*PortableAccount, error) {
	ensure()
	return unpackAccountBlob(takeBuf(C.blockchainer_portable_open_to(
		bp(envelope), C.size_t(len(envelope)), bp(recipientKEMSecret), C.size_t(len(recipientKEMSecret)))))
}

// SealSignedTo seals the account to a device's KEM key AND signs it with the
// sender's hybrid signature secret (Fortress-grade: recipient-bound + authentic).
func (a PortableAccount) SealSignedTo(recipientKEMPublic, senderSigSecret []byte) ([]byte, error) {
	ensure()
	return takeBuf(C.blockchainer_portable_seal_signed_to(
		bp(a.Public), C.size_t(len(a.Public)), bp(a.Secret), C.size_t(len(a.Secret)),
		bp(recipientKEMPublic), C.size_t(len(recipientKEMPublic)),
		bp(senderSigSecret), C.size_t(len(senderSigSecret))))
}

// OpenSignedFrom opens a signed-recipient envelope: decrypts with the device KEM
// secret AND verifies it was signed by senderSigPublic. Errors otherwise.
func OpenSignedFrom(envelope, recipientKEMSecret, senderSigPublic []byte) (*PortableAccount, error) {
	ensure()
	return unpackAccountBlob(takeBuf(C.blockchainer_portable_open_signed_from(
		bp(envelope), C.size_t(len(envelope)),
		bp(recipientKEMSecret), C.size_t(len(recipientKEMSecret)),
		bp(senderSigPublic), C.size_t(len(senderSigPublic)))))
}
