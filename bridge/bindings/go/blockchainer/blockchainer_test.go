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
	if bc.CryptoBackendVersion() != bc.CryptoVersion() {
		t.Fatalf("backend mismatch: C=%q Go=%q", bc.CryptoBackendVersion(), bc.CryptoVersion())
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
	if sel := hex.EncodeToString(bc.EthSelector("transfer(address,uint256)")); sel != "a9059cbb" {
		t.Fatalf("selector = %s, want a9059cbb", sel)
	}
}
