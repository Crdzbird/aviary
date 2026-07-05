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

// Node is a running single-validator account chain (ledger + BFT + mempool +
// observability), exposed over the C ABI.
type Node struct{ h C.BlockchainerNode }

// NewNode creates a chain with the given id. Returns nil on failure. The native
// backend self-initializes on first use, so no explicit Init is required.
func NewNode(chainID uint64) *Node {
	ensure()
	h := C.blockchainer_node_new(C.uint64_t(chainID))
	if h == nil {
		return nil
	}
	return &Node{h}
}

func (n *Node) Free() { C.blockchainer_node_free(n.h) }

// NewAccount returns (secret64, public32, address32).
func (n *Node) NewAccount() (secret, public, address []byte, err error) {
	secret, err = takeBuf(C.blockchainer_node_new_account(n.h))
	if err != nil {
		return nil, nil, nil, err
	}
	public = secret[32:64]
	address = AccountAddress(public)
	return secret, public, address, nil
}

// AccountAddress = BLAKE3-256(public key).
func AccountAddress(public []byte) []byte {
	b, _ := takeBuf(C.blockchainer_account_address(bp(public), C.size_t(len(public))))
	return b
}

// ErrGenesisSealed is returned by Credit once a block has been produced —
// genesis credits are no longer valid; fund accounts with transfers instead.
var ErrGenesisSealed = errors.New("blockchainer: genesis is sealed (a block was produced); fund accounts with transfers")

// Credit credits an address at genesis. Valid until the first block is produced
// (reads do NOT seal genesis). Returns ErrGenesisSealed afterwards — never a
// silent no-op.
func (n *Node) Credit(address []byte, amount uint64) error {
	switch C.blockchainer_node_credit(n.h, bp(address), C.uint64_t(amount)) {
	case C.BLOCKCHAINER_OK:
		return nil
	case C.BLOCKCHAINER_ERR_GENESIS_SEALED:
		return ErrGenesisSealed
	default:
		return errors.New("blockchainer: credit failed")
	}
}

func (n *Node) SubmitTransfer(secret, to []byte, amount, nonce uint64) ([]byte, error) {
	return takeBuf(C.blockchainer_node_submit_transfer(
		n.h, bp(secret), C.size_t(len(secret)), bp(to),
		C.uint64_t(amount), C.uint64_t(nonce)))
}

func (n *Node) ProduceBlock(ts uint64) (uint64, error) {
	var h C.uint64_t
	if C.blockchainer_node_produce_block(n.h, C.uint64_t(ts), &h) != 0 {
		return 0, errors.New("produce block failed")
	}
	return uint64(h), nil
}

func (n *Node) Height() uint64      { return uint64(C.blockchainer_node_height(n.h)) }
func (n *Node) MempoolSize() uint64 { return uint64(C.blockchainer_node_mempool_size(n.h)) }
func (n *Node) Balance(a []byte) uint64 {
	return uint64(C.blockchainer_node_balance(n.h, bp(a)))
}
func (n *Node) Nonce(a []byte) uint64 { return uint64(C.blockchainer_node_nonce(n.h, bp(a))) }
func (n *Node) StateRoot() []byte    { b, _ := takeBuf(C.blockchainer_node_state_root(n.h)); return b }
func (n *Node) DrainEvents() string  { b, _ := takeBuf(C.blockchainer_node_drain_events(n.h)); return string(b) }
func (n *Node) Metrics() string      { b, _ := takeBuf(C.blockchainer_node_metrics(n.h)); return string(b) }
func (n *Node) ProveAccount(a []byte) []byte {
	b, _ := takeBuf(C.blockchainer_node_prove_account(n.h, bp(a)))
	return b
}

// ── Interop ───────────────────────────────────────────────────────────────────

func EthAddress(secret32 []byte) string {
	ensure()
	b, _ := takeBuf(C.blockchainer_eth_address(bp(secret32)))
	return string(b)
}
func BtcAddress(secret32 []byte) string {
	ensure()
	b, _ := takeBuf(C.blockchainer_btc_address(bp(secret32)))
	return string(b)
}
func SolAddress(edSecret64 []byte) string {
	ensure()
	b, _ := takeBuf(C.blockchainer_sol_address(bp(edSecret64)))
	return string(b)
}

// EthErc20TransferCalldata returns ABI calldata for transfer(address,uint256).
func EthErc20TransferCalldata(to20 []byte, amount uint64) []byte {
	ensure()
	b, _ := takeBuf(C.blockchainer_eth_erc20_transfer(bp(to20), C.uint64_t(amount)))
	return b
}

// EthSelector returns the 4-byte selector for a function signature.
func EthSelector(signature string) []byte {
	ensure()
	cs := C.CString(signature)
	defer C.free(unsafe.Pointer(cs))
	b, _ := takeBuf(C.blockchainer_eth_selector(cs))
	return b
}

// EthSignLegacy signs an EIP-155 transaction; pass nil `to` for contract creation.
func EthSignLegacy(nonce, gasPrice, gasLimit uint64, to []byte, value uint64,
	data []byte, chainID uint64, secret32 []byte) ([]byte, error) {
	ensure()
	var toPtr *C.uint8_t
	hasTo := C.int(0)
	if len(to) == 20 {
		toPtr = bp(to)
		hasTo = 1
	}
	return takeBuf(C.blockchainer_eth_sign_legacy(
		C.uint64_t(nonce), C.uint64_t(gasPrice), C.uint64_t(gasLimit),
		toPtr, hasTo, C.uint64_t(value),
		bp(data), C.size_t(len(data)), C.uint64_t(chainID), bp(secret32)))
}
