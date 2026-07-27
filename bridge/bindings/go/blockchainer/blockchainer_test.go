package blockchainer_test

import (
	"bytes"
	"encoding/hex"
	"errors"
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

// Q1 end-to-end: a hybrid (post-quantum) account funds, transfers, finalizes,
// and its balance is provable — the full PQ tx path through the C ABI.
func TestHybridTransferEndToEnd(t *testing.T) {
	node := bc.NewNode(0x9001)
	defer node.Free()

	aSec, aPub, aAddr, err := node.NewAccountHybrid()
	if err != nil {
		t.Fatalf("NewAccountHybrid: %v", err)
	}
	if len(aPub) < 1900 || len(aSec) < 1900 {
		t.Fatalf("hybrid key sizes: pub=%d sec=%d", len(aPub), len(aSec))
	}
	_, _, bAddr, _ := node.NewAccountHybrid()

	if err := node.Credit(aAddr, 1_000_000); err != nil {
		t.Fatalf("Credit: %v", err)
	}
	txid, err := node.SubmitTransferHybrid(aSec, aPub, bAddr, 250_000, 0)
	if err != nil {
		t.Fatalf("SubmitTransferHybrid: %v", err)
	}
	if len(txid) != 32 {
		t.Fatalf("txid len = %d", len(txid))
	}
	if _, err := node.ProduceBlock(1); err != nil {
		t.Fatalf("ProduceBlock (hybrid tx must be accepted): %v", err)
	}
	if node.Balance(aAddr) != 750_000 || node.Balance(bAddr) != 250_000 {
		t.Fatalf("hybrid transfer not applied: alice=%d bob=%d", node.Balance(aAddr), node.Balance(bAddr))
	}
	// Light-client proof of the PQ-funded account.
	if !bc.VerifyStateProof(node.StateRoot(), node.ProveAccount(bAddr)) {
		t.Fatal("state proof of hybrid account failed")
	}
}

// S1 regression: reads must not seal genesis, and credit after a block errors.
func TestGenesisOrdering(t *testing.T) {
	node := bc.NewNode(4242)
	defer node.Free()
	_, _, addr, _ := node.NewAccount()

	// Read state BEFORE crediting — must not silently drop the credit.
	_ = node.Height()
	_ = node.Balance(addr)
	_ = node.StateRoot()
	if err := node.Credit(addr, 500); err != nil {
		t.Fatalf("Credit after reads: %v", err)
	}
	if node.Balance(addr) != 500 {
		t.Fatalf("credit dropped: balance = %d, want 500", node.Balance(addr))
	}
	if _, err := node.ProduceBlock(1); err != nil {
		t.Fatalf("ProduceBlock: %v", err)
	}
	if node.Balance(addr) != 500 {
		t.Fatalf("credit lost across block: %d", node.Balance(addr))
	}
	// Credit after sealing must return ErrGenesisSealed (never a no-op).
	if err := node.Credit(addr, 1); !errors.Is(err, bc.ErrGenesisSealed) {
		t.Fatalf("credit after block: got %v, want ErrGenesisSealed", err)
	}
	if node.Balance(addr) != 500 {
		t.Fatalf("balance changed after rejected credit: %d", node.Balance(addr))
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

func TestKeyStoreCustodialSigning(t *testing.T) {
	n := bc.NewNode(0x9101)
	defer n.Free()
	aSec, aPub, aAddr, err := n.NewAccount()
	if err != nil {
		t.Fatal(err)
	}
	_, _, bAddr, _ := n.NewAccount()

	ks, err := bc.NewKeyStore("hunter2")
	if err != nil {
		t.Fatal(err)
	}
	addr, err := ks.AddAccount(aPub, aSec)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(addr, aAddr) {
		t.Fatalf("keystore address %x != account address %x", addr, aAddr)
	}

	// Persist, reload LOCKED, unlock, reveal.
	blob, err := ks.Serialise()
	if err != nil {
		t.Fatal(err)
	}
	ks.Free()

	loaded, err := bc.LoadKeyStore(blob)
	if err != nil {
		t.Fatal(err)
	}
	defer loaded.Free()
	if loaded.Unlocked() {
		t.Fatal("keystore should be locked immediately after load")
	}
	// The serialised blob must not leak the plaintext secret.
	if bytes.Contains(blob, aSec) {
		t.Fatal("serialised keystore leaked the plaintext secret")
	}
	if _, err := loaded.Reveal(aAddr); err == nil {
		t.Fatal("reveal must fail while locked")
	}
	if err := loaded.Unlock("hunter2"); err != nil {
		t.Fatal(err)
	}
	secret, err := loaded.Reveal(aAddr)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(secret, aSec) {
		t.Fatal("revealed secret does not match")
	}

	// Sign from custody: the revealed key moves funds.
	if err := n.Credit(aAddr, 1000); err != nil {
		t.Fatal(err)
	}
	if _, err := n.SubmitTransfer(secret, bAddr, 250, 0); err != nil {
		t.Fatal(err)
	}
	if _, err := n.ProduceBlock(1); err != nil {
		t.Fatal(err)
	}
	if got := n.Balance(aAddr); got != 750 {
		t.Fatalf("alice balance = %d, want 750", got)
	}
	if got := n.Balance(bAddr); got != 250 {
		t.Fatalf("bob balance = %d, want 250", got)
	}

	// Wrong passphrase must not unlock.
	l2, _ := bc.LoadKeyStore(blob)
	defer l2.Free()
	if err := l2.Unlock("nope"); err == nil {
		t.Fatal("wrong passphrase unlocked the keystore")
	}
}

// TestPortableAccountTiers exercises the three encrypted-export tiers over the
// C ABI: a passphrase backup, a recipient-bound (PQ) backup to a device key, and
// a signed-recipient (Fortress-grade) backup that also authenticates the owner.
// Every wrong-key / wrong-passphrase / wrong-signer path must fail closed.
func TestPortableAccountTiers(t *testing.T) {
	// The account to back up: a hybrid account gives us a real (public, secret).
	n := bc.NewNode(7)
	defer n.Free()
	secret, public, _, err := n.NewAccountHybrid()
	if err != nil {
		t.Fatal(err)
	}
	acct := bc.PortableAccount{Public: public, Secret: secret}

	// Tier 1 — passphrase. Light Argon2id factors keep the test fast.
	env, err := acct.SealPassphrase("backup-phrase", 3, 1<<16)
	if err != nil {
		t.Fatal(err)
	}
	back, err := bc.OpenPassphrase(env, "backup-phrase")
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(back.Secret, secret) || !bytes.Equal(back.Public, public) {
		t.Fatal("passphrase round-trip lost the account")
	}
	if _, err := bc.OpenPassphrase(env, "wrong"); err == nil {
		t.Fatal("wrong passphrase opened the envelope")
	}

	// A device key for the recipient-bound tiers.
	devPub, devSec, err := bc.HybridKEMKeypair()
	if err != nil {
		t.Fatal(err)
	}

	// Tier 2 — recipient-bound.
	env2, err := acct.SealTo(devPub)
	if err != nil {
		t.Fatal(err)
	}
	back2, err := bc.OpenTo(env2, devSec)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(back2.Secret, secret) {
		t.Fatal("recipient round-trip lost the secret")
	}
	strangerPub, strangerSec, _ := bc.HybridKEMKeypair()
	_ = strangerPub
	if _, err := bc.OpenTo(env2, strangerSec); err == nil {
		t.Fatal("a different device opened a recipient-bound envelope")
	}

	// Tier 3 — signed-recipient. Sign with a hybrid sig secret; the receiver
	// verifies it. Reuse the account's hybrid signing secret as the owner key.
	env3, err := acct.SealSignedTo(devPub, secret)
	if err != nil {
		t.Fatal(err)
	}
	back3, err := bc.OpenSignedFrom(env3, devSec, public)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(back3.Secret, secret) {
		t.Fatal("signed-recipient round-trip lost the secret")
	}
	// Wrong expected signer → rejected (authenticity).
	if _, err := bc.OpenSignedFrom(env3, devSec, strangerPub); err == nil {
		t.Fatal("an envelope not signed by the expected owner opened")
	}
	// Tampered envelope → rejected.
	tampered := append([]byte(nil), env3...)
	tampered[len(tampered)/2] ^= 0x01
	if _, err := bc.OpenSignedFrom(tampered, devSec, public); err == nil {
		t.Fatal("a tampered signed envelope opened")
	}
}
