package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import "errors"

// SignTransfer signs a transfer from the stored account at from without
// revealing its secret: the engine uses the sealed key and wipes it. The scheme
// follows the stored public key (Ed25519 or hybrid); a zero fee gives the
// version 1 envelope. Returns the encoded transaction.
func (k *KeyStore) SignTransfer(chainID uint64, from, to []byte, amount, nonce, fee uint64) ([]byte, error) {
	if len(from) != 32 || len(to) != 32 {
		return nil, errors.New("keystore: addresses must be 32 bytes")
	}
	return takeBuf(C.aviary_keystore_sign_transfer(k.h, C.uint64_t(chainID), bp(from), bp(to),
		C.uint64_t(amount), C.uint64_t(nonce), C.uint64_t(fee)))
}

// PublicKey is the stored public key for address.
func (k *KeyStore) PublicKey(address []byte) ([]byte, error) {
	return takeBuf(C.aviary_keystore_public_key(k.h, bp(address)))
}

// Has reports whether the store holds address.
func (k *KeyStore) Has(address []byte) bool {
	return len(address) == 32 && C.aviary_keystore_has(k.h, bp(address)) == 1
}

// Addresses lists every stored address.
func (k *KeyStore) Addresses() ([][]byte, error) {
	b, err := takeBuf(C.aviary_keystore_addresses(k.h))
	if err != nil {
		return nil, err
	}
	return splitBlobList(b)
}

// Lock forgets the master key until the next Unlock.
func (k *KeyStore) Lock() { C.aviary_keystore_lock(k.h) }

// SubmitTx admits an already signed, encoded transaction and returns its id.
func (n *Node) SubmitTx(encoded []byte) ([]byte, error) {
	return takeBuf(C.aviary_node_submit_tx(n.h, bp(encoded), C.size_t(len(encoded))))
}

// SubmitTransferFromStore signs a transfer from the key store account at from
// and admits it; the secret never leaves the engine. Returns the txid.
func (n *Node) SubmitTransferFromStore(ks *KeyStore, from, to []byte, amount, nonce, fee uint64) ([]byte, error) {
	if len(from) != 32 || len(to) != 32 {
		return nil, errors.New("node: addresses must be 32 bytes")
	}
	return takeBuf(C.aviary_node_submit_transfer_keystore(n.h, ks.h, bp(from), bp(to),
		C.uint64_t(amount), C.uint64_t(nonce), C.uint64_t(fee)))
}
