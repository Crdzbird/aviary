// Example: the Blockchainer Go binding driving a chain, alongside cryptolib's Go
// binding for off-chain crypto — both from one dependency.
//
//	go run ./example
package main

import (
	"encoding/hex"
	"fmt"

	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
	"github.com/Crdzbird/CryptoLib/bridge/bindings/go/cryptolib"
)

func main() {
	// Background warm-up — fire-and-forget, returns immediately. The API below
	// self-initializes regardless, so you never have to wait on it.
	bc.Preload()

	fmt.Printf("blockchainer %s · crypto backend %s (cryptolib Go: %s)\n\n",
		bc.Version(), bc.CryptoBackendVersion(), bc.CryptoVersion())

	// ── Off-chain crypto via cryptolib (the same backend blockchainer links) ──
	doc := []byte("settlement-instruction:2026-06-08:EURx:1000000")
	digest, _ := cryptolib.Blake3(doc, 32)
	kp := cryptolib.Ed25519Keygen()
	sig, _ := cryptolib.Ed25519Sign(digest, kp.Secret)
	fmt.Println("cryptolib BLAKE3(doc):", hex.EncodeToString(digest))
	fmt.Println("cryptolib Ed25519 sig valid:", cryptolib.Ed25519Verify(digest, sig, kp.Public))

	// ── On-chain ledger via blockchainer ──
	node := bc.NewNode(1)
	defer node.Free()
	aSec, _, aAddr, _ := node.NewAccount()
	_, _, bAddr, _ := node.NewAccount()
	node.Credit(aAddr, 1_000_000)
	node.SubmitTransfer(aSec, bAddr, 250_000, node.Nonce(aAddr))
	height, _ := node.ProduceBlock(1)

	fmt.Printf("\nledger: height %d · alice %d · bob %d\n",
		height, node.Balance(aAddr), node.Balance(bAddr))

	// Light-client proof.
	proof := node.ProveAccount(bAddr)
	fmt.Println("state proof verifies:", bc.VerifyStateProof(node.StateRoot(), proof))

	// EVM contract: build + sign an ERC-20 transfer.
	secp, _ := cryptolib.RandomBytes(32)
	secp[0] |= 1
	calldata := bc.EthErc20TransferCalldata(make([]byte, 20), 1_000_000)
	raw, _ := bc.EthSignLegacy(0, 30_000_000_000, 60_000, make([]byte, 20), 0, calldata, 1, secp)
	fmt.Printf("eth %s · selector 0x%x · signed tx %d bytes\n",
		bc.EthAddress(secp), bc.EthSelector("transfer(address,uint256)"), len(raw))
}
