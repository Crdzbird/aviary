package blockchainer

// Blockchainer's crypto backend is cryptolib. This package depends on cryptolib's
// Go binding so the full crypto surface — hashing, AEAD, Ed25519/X25519,
// post-quantum (ML-KEM/ML-DSA), BLS, vaults, keyring, EVM/Bitcoin primitives —
// is available to consumers ALONGSIDE the ledger, from one dependency. Importing
// cryptolib's package also links libcryptolib_c (which blockchainer_c needs),
// the Go analog of how blockchainer_flutter depends on cryptolib_flutter.
//
// Use it directly:
//
//	import (
//		bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
//		"github.com/Crdzbird/CryptoLib/bridge/bindings/go/cryptolib"
//	)
//	digest, _ := cryptolib.Blake3(data, 32)
//	node := bc.NewNode(1)

import cryptolib "github.com/Crdzbird/CryptoLib/bridge/bindings/go/cryptolib"

// cryptoInit initializes cryptolib's Go binding (called once during Init).
func cryptoInit() error { return cryptolib.Init() }

// CryptoVersion returns the cryptolib version reported by its Go binding. A
// non-empty value matching CryptoBackendVersion proves the dependency is wired
// end to end.
func CryptoVersion() string {
	ensure()
	return cryptolib.Version()
}
