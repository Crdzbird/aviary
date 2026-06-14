// Cross-language determinism check (Go side). Links libblockchainer_c and
// asserts the SAME golden vectors the C++ test and the Dart demo assert.
package main

import (
	"encoding/hex"
	"fmt"
	"os"

	blockchainer "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
)

func main() {
	if err := blockchainer.Init(); err != nil {
		panic(err)
	}
	fmt.Println("blockchainer  version :", blockchainer.Version())
	fmt.Println("crypto backend version:", blockchainer.CryptoBackendVersion())

	failures := 0
	check := func(cond bool, what string) {
		if cond {
			fmt.Println("  ok  ", what)
		} else {
			fmt.Println("  FAIL", what)
			failures++
		}
	}

	empty, err := blockchainer.Hash([]byte{})
	if err != nil {
		panic(err)
	}
	check(hex.EncodeToString(empty) ==
		"af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262",
		"BLAKE3('') golden vector")

	tag, err := blockchainer.Hash([]byte("blockchainer"))
	if err != nil {
		panic(err)
	}
	check(hex.EncodeToString(tag) ==
		"c9ab2bfe296411d91948b0afc12eda63d2a7ef470e95eb1cb390c58821d8c8ce",
		"BLAKE3('blockchainer') cross-language vector")

	// Replay the canonical txid / header_hash golden vectors through the C ABI.
	author := make([]byte, 32)
	for i := range author {
		author[i] = 0x11
	}
	sig := make([]byte, 64)
	for i := range sig {
		sig[i] = 0x22
	}
	txid, err := blockchainer.Txid(1, 7, []byte("hello"), author, sig)
	if err != nil {
		panic(err)
	}
	check(hex.EncodeToString(txid) ==
		"be99a206b50a87b74e8359ab3a2895ed2f082169089b7ff9ec416d9df18d36dd",
		"canonical txid golden vector")

	parent := make([]byte, 32)
	txRoot := make([]byte, 32)
	stateRoot := make([]byte, 32)
	for i := range txRoot {
		txRoot[i] = 0xAA
		stateRoot[i] = 0xBB
	}
	hh, err := blockchainer.HeaderHash(1, 7, 42, parent, 1000, txRoot, stateRoot, nil)
	if err != nil {
		panic(err)
	}
	check(hex.EncodeToString(hh) ==
		"0ad7e2e233a9ebf827717d33b72a685eb993d8bac70b3cee517ba8023dd79793",
		"canonical header_hash golden vector")

	// Verify a state-inclusion proof (golden from vectors/proof_vectors.txt).
	proofRoot, _ := hex.DecodeString(
		"9918413a2ad25fc3ec37457f522bcf091f0aaefc14af15d56b3cb2100632bce3")
	proofBytes, _ := hex.DecodeString(
		"01000000381111111111111111111111111111111111111111111111111111111111111111000000000000000000000000000001f40000000000000000000000020000000000000001bb53fe611991a376f327f60748faf0ceaae4671df4d5fb7a89b8f56cbce17033")
	check(blockchainer.VerifyStateProof(proofRoot, proofBytes), "state proof verifies")
	tampered := make([]byte, len(proofBytes))
	copy(tampered, proofBytes)
	tampered[40] ^= 0x01
	check(!blockchainer.VerifyStateProof(proofRoot, tampered), "tampered state proof rejected")

	if failures == 0 {
		fmt.Println("\nALL PASS")
	} else {
		fmt.Printf("\n%d FAILURE(S)\n", failures)
		os.Exit(1)
	}
}
