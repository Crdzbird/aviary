package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import (
	"errors"
	"runtime/cgo"
	"unsafe"
)

// Node is a running single-validator account chain (ledger + BFT + mempool +
// observability), exposed over the C ABI.
type Node struct {
	h       C.AviaryNode
	handle  cgo.Handle
	onEvent func(json string)
}

// NewNode creates a chain with the given id. Returns nil on failure. The native
// backend self-initializes on first use, so no explicit Init is required.
func NewNode(chainID uint64) *Node {
	ensure()
	h := C.aviary_node_new(C.uint64_t(chainID))
	if h == nil {
		return nil
	}
	return &Node{h: h}
}

func (n *Node) Free() {
	if n.h == nil {
		return
	}
	if n.handle != 0 {
		C.aviary_node_set_event_hook(n.h, nil, nil)
		n.handle.Delete()
		n.handle = 0
	}
	C.aviary_node_free(n.h)
	n.h = nil
}

// NewAccount returns (secret64, public32, address32).
func (n *Node) NewAccount() (secret, public, address []byte, err error) {
	secret, err = takeBuf(C.aviary_node_new_account(n.h))
	if err != nil {
		return nil, nil, nil, err
	}
	public = secret[32:64]
	address = AccountAddress(public)
	return secret, public, address, nil
}

// AccountAddress = BLAKE3-256(public key).
func AccountAddress(public []byte) []byte {
	b, _ := takeBuf(C.aviary_account_address(bp(public), C.size_t(len(public))))
	return b
}

// NewAccountHybrid generates a fresh hybrid (Ed25519 + ML-DSA-65) post-quantum
// account. Returns (secret, public, address). Use SubmitTransferHybrid to spend.
func (n *Node) NewAccountHybrid() (secret, public, address []byte, err error) {
	blob, e := takeBuf(C.aviary_node_new_account_hybrid(n.h))
	if e != nil {
		return nil, nil, nil, e
	}
	// Layout: [pub_len: 4 bytes big-endian][public][secret].
	if len(blob) < 4 {
		return nil, nil, nil, errors.New("hybrid account: short buffer")
	}
	pl := int(blob[0])<<24 | int(blob[1])<<16 | int(blob[2])<<8 | int(blob[3])
	if 4+pl > len(blob) {
		return nil, nil, nil, errors.New("hybrid account: bad public length")
	}
	public = blob[4 : 4+pl]
	secret = blob[4+pl:]
	address = AccountAddress(public)
	return secret, public, address, nil
}

// SubmitTransferHybrid builds, signs, and admits a post-quantum transfer using
// the hybrid key blobs from NewAccountHybrid.
func (n *Node) SubmitTransferHybrid(secret, public, to []byte, amount, nonce uint64) ([]byte, error) {
	return takeBuf(C.aviary_node_submit_transfer_hybrid(
		n.h, bp(secret), C.size_t(len(secret)), bp(public), C.size_t(len(public)),
		bp(to), C.uint64_t(amount), C.uint64_t(nonce)))
}

// ErrGenesisSealed is returned by Credit once a block has been produced -
// genesis credits are no longer valid; fund accounts with transfers instead.
var ErrGenesisSealed = errors.New("aviary: genesis is sealed (a block was produced); fund accounts with transfers")

// Credit credits an address at genesis. Valid until the first block is produced
// (reads do NOT seal genesis). Returns ErrGenesisSealed afterwards - never a
// silent no-op.
func (n *Node) Credit(address []byte, amount uint64) error {
	switch C.aviary_node_credit(n.h, bp(address), C.uint64_t(amount)) {
	case C.AVIARY_OK:
		return nil
	case C.AVIARY_ERR_GENESIS_SEALED:
		return ErrGenesisSealed
	default:
		return errors.New("aviary: credit failed")
	}
}

func (n *Node) SubmitTransfer(secret, to []byte, amount, nonce uint64) ([]byte, error) {
	return takeBuf(C.aviary_node_submit_transfer(
		n.h, bp(secret), C.size_t(len(secret)), bp(to),
		C.uint64_t(amount), C.uint64_t(nonce)))
}

// SubmitTransferFee submits an Ed25519 transfer carrying a priority-fee tip (a
// version-2 envelope), used for fee-market mempool priority.
func (n *Node) SubmitTransferFee(secret, to []byte, amount, nonce, fee uint64) ([]byte, error) {
	return takeBuf(C.aviary_node_submit_transfer_fee(
		n.h, bp(secret), C.size_t(len(secret)), bp(to),
		C.uint64_t(amount), C.uint64_t(nonce), C.uint64_t(fee)))
}

func (n *Node) ProduceBlock(ts uint64) (uint64, error) {
	var h C.uint64_t
	if C.aviary_node_produce_block(n.h, C.uint64_t(ts), &h) != 0 {
		return 0, errors.New("produce block failed")
	}
	return uint64(h), nil
}

func (n *Node) Height() uint64      { return uint64(C.aviary_node_height(n.h)) }
func (n *Node) MempoolSize() uint64 { return uint64(C.aviary_node_mempool_size(n.h)) }
func (n *Node) Balance(a []byte) uint64 {
	return uint64(C.aviary_node_balance(n.h, bp(a)))
}
func (n *Node) Nonce(a []byte) uint64 { return uint64(C.aviary_node_nonce(n.h, bp(a))) }
func (n *Node) StateRoot() []byte     { b, _ := takeBuf(C.aviary_node_state_root(n.h)); return b }
func (n *Node) DrainEvents() string {
	b, _ := takeBuf(C.aviary_node_drain_events(n.h))
	return string(b)
}
func (n *Node) Metrics() string { b, _ := takeBuf(C.aviary_node_metrics(n.h)); return string(b) }
func (n *Node) ProveAccount(a []byte) []byte {
	b, _ := takeBuf(C.aviary_node_prove_account(n.h, bp(a)))
	return b
}

// -- Interop -------------------------------------------------------------------

func EthAddress(secret32 []byte) string {
	ensure()
	b, _ := takeBuf(C.aviary_eth_address(bp(secret32)))
	return string(b)
}
func BtcAddress(secret32 []byte) string {
	ensure()
	b, _ := takeBuf(C.aviary_btc_address(bp(secret32)))
	return string(b)
}
func SolAddress(edSecret64 []byte) string {
	ensure()
	b, _ := takeBuf(C.aviary_sol_address(bp(edSecret64)))
	return string(b)
}

// EthErc20TransferCalldata returns ABI calldata for transfer(address,uint256).
func EthErc20TransferCalldata(to20 []byte, amount uint64) []byte {
	ensure()
	b, _ := takeBuf(C.aviary_eth_erc20_transfer(bp(to20), C.uint64_t(amount)))
	return b
}

// EthSelector returns the 4-byte selector for a function signature.
func EthSelector(signature string) []byte {
	ensure()
	cs := C.CString(signature)
	defer C.free(unsafe.Pointer(cs))
	b, _ := takeBuf(C.aviary_eth_selector(cs))
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
	return takeBuf(C.aviary_eth_sign_legacy(
		C.uint64_t(nonce), C.uint64_t(gasPrice), C.uint64_t(gasLimit),
		toPtr, hasTo, C.uint64_t(value),
		bp(data), C.size_t(len(data)), C.uint64_t(chainID), bp(secret32)))
}
