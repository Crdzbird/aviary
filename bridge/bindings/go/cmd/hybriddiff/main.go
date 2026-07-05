// hybriddiff — the Go half of the cross-binding hybrid determinism check.
// Builds a post-quantum scenario and emits the shared keys + resulting state
// root as JSON; the Dart half replays it and asserts the same root.
package main

import (
	"encoding/hex"
	"encoding/json"
	"fmt"

	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
)

const (
	chainID  = 0x9001
	credit   = 1_000_000
	transfer = 250_000
)

func main() {
	node := bc.NewNode(chainID)
	defer node.Free()
	aSec, aPub, aAddr, err := node.NewAccountHybrid()
	if err != nil {
		panic(err)
	}
	_, _, bAddr, _ := node.NewAccountHybrid()
	if err := node.Credit(aAddr, credit); err != nil {
		panic(err)
	}
	if _, err := node.SubmitTransferHybrid(aSec, aPub, bAddr, transfer, 0); err != nil {
		panic(err)
	}
	if _, err := node.ProduceBlock(1); err != nil {
		panic(err)
	}
	out, _ := json.Marshal(map[string]string{
		"aliceSecret": hex.EncodeToString(aSec),
		"alicePublic": hex.EncodeToString(aPub),
		"bobAddress":  hex.EncodeToString(bAddr),
		"stateRoot":   hex.EncodeToString(node.StateRoot()),
	})
	fmt.Println(string(out))
}
