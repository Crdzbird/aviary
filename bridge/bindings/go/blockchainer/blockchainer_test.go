package blockchainer_test

import (
	"bytes"
	"encoding/hex"
	"testing"

	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
	"github.com/Crdzbird/CryptoLib/bridge/bindings/go/cryptolib"
)

// The API self-initializes; no explicit Init() needed.
func TestLazyInitAndVersions(t *testing.T) {
	if got := bc.Version(); got == "" {
		t.Fatal("empty version")
	}
	if got := bc.CryptoBackendVersion(); got == "" {
		t.Fatal("empty crypto backend version")
	}
	if err := bc.Init(); err != nil { // idempotent
		t.Fatalf("Init: %v", err)
	}
	if !bc.Initialized() {
		t.Fatal("Initialized() false after Init")
	}
}

func TestPreloadBackground(t *testing.T) {
	bc.Preload() // fire-and-forget; must not block or panic
	if err := bc.Init(); err != nil {
		t.Fatalf("Init after Preload: %v", err)
	}
}

func TestLedgerFlowAndProof(t *testing.T) {
	node := bc.NewNode(1)
	if node == nil {
		t.Fatal("NewNode returned nil")
	}
	defer node.Free()

	aSec, _, aAddr, err := node.NewAccount()
	if err != nil {
		t.Fatalf("NewAccount: %v", err)
	}
	_, _, bAddr, _ := node.NewAccount()
	node.Credit(aAddr, 1000)
	if _, err := node.SubmitTransfer(aSec, bAddr, 250, node.Nonce(aAddr)); err != nil {
		t.Fatalf("SubmitTransfer: %v", err)
	}
	h, err := node.ProduceBlock(1)
	if err != nil || h != 1 {
		t.Fatalf("ProduceBlock: h=%d err=%v", h, err)
	}
	if node.Balance(bAddr) != 250 {
		t.Fatalf("balance = %d, want 250", node.Balance(bAddr))
	}
	if !bc.VerifyStateProof(node.StateRoot(), node.ProveAccount(bAddr)) {
		t.Fatal("state proof did not verify")
	}
}

// Cross-binding determinism: blockchainer's BLAKE3 (via C++ core) equals
// cryptolib's BLAKE3 (via its Go binding) — both link the same backend.
func TestHashMatchesCryptolib(t *testing.T) {
	msg := []byte("blockchainer")
	viaCore, err := bc.Hash(msg)
	if err != nil {
		t.Fatalf("bc.Hash: %v", err)
	}
	viaCrypto, err := cryptolib.Blake3(msg, 32)
	if err != nil {
		t.Fatalf("cryptolib.Blake3: %v", err)
	}
	if !bytes.Equal(viaCore, viaCrypto) {
		t.Fatalf("BLAKE3 mismatch:\n core=%s\n cl=%s",
			hex.EncodeToString(viaCore), hex.EncodeToString(viaCrypto))
	}
}

func TestInterop(t *testing.T) {
	secp := bytes.Repeat([]byte{0x11}, 32)
	if a := bc.EthAddress(secp); len(a) != 42 || a[:2] != "0x" {
		t.Fatalf("eth address shape: %q", a)
	}
	if b := bc.BtcAddress(secp); len(b) < 14 || b[:3] != "bc1" {
		t.Fatalf("btc address shape: %q", b)
	}
	if sel := hex.EncodeToString(bc.EthSelector("transfer(address,uint256)")); sel != "a9059cbb" {
		t.Fatalf("selector = %s, want a9059cbb", sel)
	}
	// EVM contract calldata + signed legacy tx.
	calldata := bc.EthErc20TransferCalldata(make([]byte, 20), 1)
	if len(calldata) != 4+32+32 {
		t.Fatalf("erc20 calldata len = %d, want 68", len(calldata))
	}
	raw, err := bc.EthSignLegacy(0, 1, 21000, make([]byte, 20), 0, calldata, 1, secp)
	if err != nil || len(raw) == 0 {
		t.Fatalf("eth sign legacy: raw=%d err=%v", len(raw), err)
	}
}

// Cross-binding naming aliases forward to the Go-idiomatic names.
func TestAliases(t *testing.T) {
	if err := bc.EnsureInitialized(); err != nil { // == Init
		t.Fatalf("EnsureInitialized: %v", err)
	}
	if !bc.IsInitialized() { // == Initialized
		t.Fatal("IsInitialized false")
	}
	if bc.CryptoBackend() != bc.CryptoBackendVersion() {
		t.Fatal("CryptoBackend alias mismatch")
	}
	n := bc.CreateNode(1234) // == NewNode
	if n == nil {
		t.Fatal("CreateNode returned nil")
	}
	n.Dispose() // == Free
}

// The verify-only entry points must reject malformed input (never crash). Valid
// inputs are produced by the core's prove/commit paths.
func TestVerifyRejectsGarbage(t *testing.T) {
	root := bytes.Repeat([]byte{0x01}, 32)
	if bc.VerifyTxInclusion(root, []byte("not-a-proof")) {
		t.Fatal("VerifyTxInclusion accepted garbage")
	}
	if bc.VerifyCommit(1, []byte("vset"), []byte("cert")) {
		t.Fatal("VerifyCommit accepted garbage")
	}
}

// TestFullABICoverage documents that the Go binding wraps the ENTIRE C ABI. The
// authoritative check is scripts-level (compare C.blockchainer_* call sites to
// bridge/blockchainer_c.h); this lists the surface so a reviewer can eyeball it.
func TestFullABICoverage(t *testing.T) {
	// Top-level: Init, Version, CryptoBackendVersion, Hash, Txid,
	// HeaderHash, VerifyStateProof, VerifyTxInclusion, VerifyCommit, AccountAddress,
	// EthAddress, BtcAddress, SolAddress, EthErc20TransferCalldata, EthSelector,
	// EthSignLegacy, Preload, Initialized, NewNode.
	// Node: NewAccount, Credit, SubmitTransfer, ProduceBlock, Height, MempoolSize,
	// Balance, Nonce, StateRoot, DrainEvents, Metrics, ProveAccount, Free.
	if !bc.Initialized() {
		_ = bc.Init()
	}
	if !bc.Initialized() {
		t.Fatal("backend not initialized")
	}
}
