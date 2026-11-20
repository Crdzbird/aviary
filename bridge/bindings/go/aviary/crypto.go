package aviary

// Aviary's crypto backend is cipherbird. This package depends on cipherbird's
// Go binding so the full crypto surface - hashing, AEAD, Ed25519/X25519,
// post-quantum (ML-KEM/ML-DSA), BLS, vaults, keyring, EVM/Bitcoin primitives -
// is available to consumers ALONGSIDE the ledger, from one dependency. Importing
// cipherbird's package also links libcipherbird_c (which aviary_c needs),
// the Go analog of how aviary_flutter depends on cipherbird_flutter.
//
// Use it directly:
//
//	import (
//		bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
//		"github.com/Crdzbird/CipherBird/bridge/bindings/go/cipherbird"
//	)
//	digest, _ := cipherbird.Blake3(data, 32)
//	node := bc.NewNode(1)

import cipherbird "github.com/Crdzbird/CipherBird/bridge/bindings/go/cipherbird"

// cryptoInit initializes cipherbird's Go binding (called once during Init). This
// also links libcipherbird_c, which aviary_c needs. Use CryptoBackendVersion
// for the backend version (parity with the other bindings).
func cryptoInit() error { return cipherbird.Init() }
