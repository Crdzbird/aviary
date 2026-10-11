package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import (
	"errors"
	"unsafe"
)

// KeyStore is a custodial container that seals account signing secrets at rest.
// A node never needs to hold a plaintext key: add accounts, persist the
// (encrypted) blob, reload it locked, unlock, then reveal a secret only to sign.
type KeyStore struct{ h C.AviaryKeyStore }

// NewKeyStore creates a store protected by passphrase (returned unlocked).
func NewKeyStore(passphrase string) (*KeyStore, error) {
	ensure()
	cp := C.CString(passphrase)
	defer C.free(unsafe.Pointer(cp))
	h := C.aviary_keystore_new(cp)
	if h == nil {
		return nil, errors.New("keystore create failed")
	}
	return &KeyStore{h}, nil
}

// LoadKeyStore parses a serialised store. It comes back LOCKED - call Unlock
// before revealing secrets.
func LoadKeyStore(blob []byte) (*KeyStore, error) {
	ensure()
	h := C.aviary_keystore_load(bp(blob), C.size_t(len(blob)))
	if h == nil {
		return nil, errors.New("keystore load failed")
	}
	return &KeyStore{h}, nil
}

func (k *KeyStore) Free() { C.aviary_keystore_free(k.h) }

// AddAccount seals an account's secret and returns its 32-byte address. Requires
// the store to be unlocked.
func (k *KeyStore) AddAccount(publicKey, secret []byte) ([]byte, error) {
	addr := make([]byte, 32)
	rc := C.aviary_keystore_add_account(k.h, bp(publicKey), C.size_t(len(publicKey)),
		bp(secret), C.size_t(len(secret)), bp(addr))
	if rc != 0 {
		return nil, errors.New("keystore add account failed (locked?)")
	}
	return addr, nil
}

// Reveal decrypts and returns the sealed secret for address. Requires unlocked.
func (k *KeyStore) Reveal(address []byte) ([]byte, error) {
	return takeBuf(C.aviary_keystore_reveal(k.h, bp(address)))
}

// Serialise returns the persistable (encrypted) blob - no plaintext key.
func (k *KeyStore) Serialise() ([]byte, error) {
	return takeBuf(C.aviary_keystore_serialise(k.h))
}

// Unlock recovers the master key from the passphrase. Returns an error on a
// wrong passphrase.
func (k *KeyStore) Unlock(passphrase string) error {
	cp := C.CString(passphrase)
	defer C.free(unsafe.Pointer(cp))
	if C.aviary_keystore_unlock(k.h, cp) != 0 {
		return errors.New("keystore unlock failed")
	}
	return nil
}

// Unlocked reports whether secrets can currently be revealed.
func (k *KeyStore) Unlocked() bool { return C.aviary_keystore_unlocked(k.h) == 1 }
