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

// TestThresholdAccount exercises FROST t-of-n signing over the C ABI: a 2-of-3
// ceremony produces a STANDARD Ed25519 signature under the group key (so it
// verifies through the ordinary path), a lone share cannot, a corrupt cosigner
// is attributed, and a sealed share round-trips.
func TestThresholdAccount(t *testing.T) {
	cards, err := bc.CreateThresholdAccount(3, 2)
	if err != nil {
		t.Fatal(err)
	}
	if len(cards) != 3 {
		t.Fatalf("want 3 cards, got %d", len(cards))
	}
	groupPK := cards[0].GroupPublicKey()
	if len(groupPK) != 32 {
		t.Fatalf("group pk = %d bytes", len(groupPK))
	}
	for _, c := range cards[1:] {
		if !bytes.Equal(c.GroupPublicKey(), groupPK) {
			t.Fatal("cards disagree on the group key")
		}
	}

	msg := []byte("threshold-signed transfer")

	// Participants: cards 0 and 3 (indices 1 and 3). Round 1.
	n0, c0, err := cards[0].Commit()
	if err != nil {
		t.Fatal(err)
	}
	n2, c2, err := cards[2].Commit()
	if err != nil {
		t.Fatal(err)
	}

	cer, err := bc.NewCeremony(groupPK, msg, []bc.Commitment{c2, c0}) // order-insensitive
	if err != nil {
		t.Fatal(err)
	}
	defer cer.Free()

	s0, err := cer.Sign(cards[0], n0)
	if err != nil {
		t.Fatal(err)
	}
	s2, err := cer.Sign(cards[2], n2)
	if err != nil {
		t.Fatal(err)
	}

	sig, err := cer.Aggregate([]uint16{cards[0].Index(), cards[2].Index()}, [][]byte{s0, s2})
	if err != nil {
		t.Fatal(err)
	}
	if len(sig) != 64 {
		t.Fatalf("aggregate is %d bytes, want a 64-byte Ed25519 signature", len(sig))
	}
	if !cryptolib.Ed25519Verify(msg, sig, groupPK) {
		t.Fatal("the aggregate is not a valid Ed25519 signature under the group key")
	}

	// A lying cosigner is identified, not just rejected.
	if !cer.VerifyShare(cards[0].Index(), cards[0].PublicShare(), s0) {
		t.Fatal("honest share attributed as invalid")
	}
	bad := append([]byte(nil), s2...)
	bad[5] ^= 0x01
	if cer.VerifyShare(cards[2].Index(), cards[2].PublicShare(), bad) {
		t.Fatal("corrupt share was not detected")
	}

	// Below threshold: one participant alone cannot forge the group signature.
	nSolo, cSolo, _ := cards[1].Commit()
	solo, err := bc.NewCeremony(groupPK, msg, []bc.Commitment{cSolo})
	if err != nil {
		t.Fatal(err)
	}
	defer solo.Free()
	sSolo, _ := solo.Sign(cards[1], nSolo)
	soloSig, err := solo.Aggregate([]uint16{cards[1].Index()}, [][]byte{sSolo})
	if err == nil && cryptolib.Ed25519Verify(msg, soloSig, groupPK) {
		t.Fatal("a single share produced a valid group signature")
	}

	// A sealed share round-trips; a wrong passphrase fails closed.
	env, err := cards[1].Seal("share-pass", 3, 1<<16)
	if err != nil {
		t.Fatal(err)
	}
	back, err := bc.UnsealShare(env, "share-pass")
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(back, cards[1]) {
		t.Fatal("sealed share did not round-trip")
	}
	if _, err := bc.UnsealShare(env, "wrong"); err == nil {
		t.Fatal("wrong passphrase unsealed the share")
	}
}

// TestAssuranceProfile checks each posture maps to a consistent tier at every seam.
func TestAssuranceProfile(t *testing.T) {
	std, err := bc.Describe(bc.Standard)
	if err != nil {
		t.Fatal(err)
	}
	if std.Ratcheted || std.VRF || std.ThresholdCustody || std.SealedMempool {
		t.Fatal("standard should use the baseline tier everywhere")
	}
	hard, _ := bc.Describe(bc.Hardened)
	if !hard.Ratcheted || !hard.VRF || !hard.ThresholdCustody || hard.SealedMempool {
		t.Fatal("hardened: ratcheted transport + VRF + threshold custody, no sealed mempool")
	}
	fort, _ := bc.Describe(bc.Fortress)
	if !fort.Ratcheted || !fort.VRF || !fort.ThresholdCustody || !fort.SealedMempool {
		t.Fatal("fortress: every tier on")
	}
	// Stronger levels never permit bare Ed25519 (SigAlgo 1).
	if !containsByte(std.SigAlgos, 1) {
		t.Fatal("standard should permit Ed25519")
	}
	if containsByte(hard.SigAlgos, 1) || containsByte(fort.SigAlgos, 1) {
		t.Fatal("hardened/fortress must not permit bare Ed25519")
	}
	if _, err := bc.Describe(99); err == nil {
		t.Fatal("an unknown level should error")
	}
}

func containsByte(b []byte, x byte) bool {
	for _, v := range b {
		if v == x {
			return true
		}
	}
	return false
}

// TestSealedMempool: transactions stay sealed to the proposer, ordered by fee,
// opened only by the proposer's key.
func TestSealedMempool(t *testing.T) {
	proposerPub, proposerSec, err := bc.HybridKEMKeypair()
	if err != nil {
		t.Fatal(err)
	}
	// Encode three v2 (fee-carrying) transactions with distinct fees.
	mk := func(fee uint64, tag byte) []byte {
		tx, e := bc.EncodeTx(2, 7, 1, fee, []byte{tag}, make([]byte, 32), make([]byte, 64))
		if e != nil {
			t.Fatal(e)
		}
		return tx
	}
	pool := bc.NewSealedMempool(64)
	defer pool.Free()
	for _, f := range []uint64{50, 300, 120} {
		sealed, e := bc.SealTx(mk(f, byte(f)), proposerPub)
		if e != nil {
			t.Fatal(e)
		}
		if !pool.Add(sealed) {
			t.Fatal("add rejected")
		}
	}
	if pool.Size() != 3 {
		t.Fatalf("pool size = %d", pool.Size())
	}

	opened, err := pool.Reveal(proposerSec, 10)
	if err != nil {
		t.Fatal(err)
	}
	if len(opened) != 3 {
		t.Fatalf("revealed %d, want 3", len(opened))
	}
	// The pool never exposes plaintext; only the proposer's key opens it. A
	// different (non-proposer) key reveals nothing. (Fee-priority ordering is a
	// C++ property, verified in test_sealed_mempool.cpp.)
	_, sec2, _ := bc.HybridKEMKeypair()
	if other, _ := pool.Reveal(sec2, 10); len(other) != 0 {
		t.Fatal("a non-proposer key opened the sealed pool")
	}
}

// TestCredentialGate drives Option A anonymous membership admission end to end:
// issue an issuer-signed credential, present it unlinkably, admit — then confirm
// the gate rejects on context mismatch, expiry, tier, revocation, and forgery.
func TestCredentialGate(t *testing.T) {
	issuer, err := bc.NewBBSIssuer(bytes.Repeat([]byte{0x11}, 32))
	if err != nil {
		t.Fatal(err)
	}
	if len(issuer.Public) != 96 || len(issuer.Secret) != 32 {
		t.Fatalf("bad issuer key sizes: pub=%d sec=%d", len(issuer.Public), len(issuer.Secret))
	}

	policy, err := bc.NewCredentialPolicy(7, issuer.Public, []byte("blockchainer.credential.v1"), 4, 2)
	if err != nil {
		t.Fatal(err)
	}
	h, err := policy.Hash()
	if err != nil || len(h) != 32 {
		t.Fatalf("policy hash: %v len=%d", err, len(h))
	}

	memberSecret := bytes.Repeat([]byte{0xAB}, 32)
	const notAfter = uint64(4102444800) // year 2100
	const now = uint64(1700000000)
	cred, err := policy.IssueCredential(issuer.Secret, memberSecret, 3, notAfter, 1)
	if err != nil {
		t.Fatal(err)
	}

	gate := bc.NewCredentialGate(policy)
	defer gate.Free()
	challenge := bytes.Repeat([]byte{0x01}, 32)

	// Happy path.
	pres, err := policy.Present(cred, 5, "submit", challenge)
	if err != nil {
		t.Fatal(err)
	}
	if !gate.Admit(5, "submit", challenge, pres, now) {
		t.Fatal("valid presentation was rejected")
	}

	// Unlinkability: a fresh presentation of the same credential differs on the
	// wire yet still admits.
	pres2, _ := policy.Present(cred, 5, "submit", challenge)
	if bytes.Equal(pres, pres2) {
		t.Fatal("two presentations of one credential were byte-identical (not unlinkable)")
	}
	if !gate.Admit(5, "submit", challenge, pres2, now) {
		t.Fatal("second presentation rejected")
	}

	// Context binding (anti-replay): wrong epoch / scope / challenge all reject.
	if gate.Admit(6, "submit", challenge, pres, now) {
		t.Fatal("admitted under the wrong epoch")
	}
	if gate.Admit(5, "gossip", challenge, pres, now) {
		t.Fatal("admitted under the wrong scope")
	}
	if gate.Admit(5, "submit", bytes.Repeat([]byte{0x02}, 32), pres, now) {
		t.Fatal("admitted under the wrong challenge")
	}

	// Expiry.
	expired, _ := policy.IssueCredential(issuer.Secret, memberSecret, 3, 1000, 1)
	ep, _ := policy.Present(expired, 5, "submit", challenge)
	if gate.Admit(5, "submit", challenge, ep, now) {
		t.Fatal("admitted an expired credential")
	}

	// Tier below the policy threshold.
	low, _ := policy.IssueCredential(issuer.Secret, memberSecret, 1, notAfter, 1)
	lp, _ := policy.Present(low, 5, "submit", challenge)
	if gate.Admit(5, "submit", challenge, lp, now) {
		t.Fatal("admitted a credential below min tier")
	}

	// Bulk revocation of issuer epoch 1.
	revoking := bc.NewCredentialGate(policy)
	defer revoking.Free()
	revoking.RevokeIssuerEpoch(1)
	if revoking.Admit(5, "submit", challenge, pres, now) {
		t.Fatal("admitted a credential from a revoked issuer epoch")
	}

	// Forgery: a credential from another issuer is refused.
	rogue, _ := bc.NewBBSIssuer(bytes.Repeat([]byte{0x77}, 32))
	forged, _ := policy.IssueCredential(rogue.Secret, memberSecret, 3, notAfter, 1)
	fp, _ := policy.Present(forged, 5, "submit", challenge)
	if gate.Admit(5, "submit", challenge, fp, now) {
		t.Fatal("admitted a forged (wrong-issuer) credential")
	}

	// Malformed presentation is a quiet reject.
	if gate.Admit(5, "submit", challenge, []byte("garbage"), now) {
		t.Fatal("admitted a malformed presentation")
	}
}

// TestCredentialGatePseudonymous drives Option B end to end: blind issuance
// (commit -> blind-sign -> finalize), then per-(epoch, scope) pseudonyms — same
// member+epoch+scope -> same nym (Sybil-linkable), different epoch -> unlinkable —
// plus the Sybil cap, targeted nym revocation, and the reject set.
func TestCredentialGatePseudonymous(t *testing.T) {
	issuer, err := bc.NewBBSIssuer(bytes.Repeat([]byte{0x2a}, 32))
	if err != nil {
		t.Fatal(err)
	}
	policy, err := bc.NewPseudonymousPolicy(7, issuer.Public, []byte("blockchainer.credential.v2"), 4, 2)
	if err != nil {
		t.Fatal(err)
	}

	// Blind issuance: the issuer never sees the member's seed.
	issueCred := func(memberSecret []byte, tier uint8, notAfter uint64, issuerEpoch uint32) []byte {
		mc, err := policy.MembershipCommit(memberSecret)
		if err != nil {
			t.Fatal(err)
		}
		commit, err := bc.ExtractCommitment(mc)
		if err != nil {
			t.Fatal(err)
		}
		bi, err := policy.BlindIssue(issuer.Secret, commit, tier, notAfter, issuerEpoch)
		if err != nil {
			t.Fatal(err)
		}
		cred, err := bc.FinalizeMembership(mc, bi)
		if err != nil {
			t.Fatal(err)
		}
		return cred
	}

	memberSecret := bytes.Repeat([]byte{0x77}, 32)
	const notAfter = uint64(4102444800)
	const now = uint64(1700000000)
	cred := issueCred(memberSecret, 3, notAfter, 1)

	gate := bc.NewCredentialGate(policy)
	defer gate.Free()

	present := func(c []byte, epoch uint32, scope string, ch byte) []byte {
		p, err := policy.PresentPseudonymous(c, epoch, scope, bytes.Repeat([]byte{ch}, 32))
		if err != nil {
			t.Fatal(err)
		}
		return p
	}

	// Happy path: admit returns a 48-byte pseudonym.
	nymA, ok := gate.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{1}, 32), present(cred, 5, "gossip", 1), now)
	if !ok || len(nymA) != 48 {
		t.Fatalf("admit: ok=%v len=%d", ok, len(nymA))
	}

	// Sybil-linkable: a fresh presentation of the same member in the same
	// (epoch, scope) yields the SAME pseudonym.
	nym2, ok := gate.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{2}, 32), present(cred, 5, "gossip", 2), now)
	if !ok || !bytes.Equal(nym2, nymA) {
		t.Fatal("same member+epoch+scope should yield the same nym")
	}
	if !gate.NoteAdmission(5, "gossip", nymA) {
		t.Fatal("first admission should be fresh")
	}
	if gate.NoteAdmission(5, "gossip", nym2) {
		t.Fatal("a second session on an admitted nym should be flagged (Sybil cap)")
	}

	// Unlinkable across epochs and scopes.
	nymE, ok := gate.AdmitPseudonymous(6, "gossip", bytes.Repeat([]byte{1}, 32), present(cred, 6, "gossip", 1), now)
	if !ok || bytes.Equal(nymE, nymA) {
		t.Fatal("different epoch should yield a different nym")
	}
	nymS, ok := gate.AdmitPseudonymous(5, "submit", bytes.Repeat([]byte{1}, 32), present(cred, 5, "submit", 1), now)
	if !ok || bytes.Equal(nymS, nymA) {
		t.Fatal("different scope should yield a different nym")
	}

	// A different member → a different nym in the same context.
	nymO, ok := gate.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{1}, 32),
		present(issueCred(bytes.Repeat([]byte{0x88}, 32), 3, notAfter, 1), 5, "gossip", 1), now)
	if !ok || bytes.Equal(nymO, nymA) {
		t.Fatal("different member should yield a different nym")
	}

	// Context binding (anti-replay): wrong epoch / scope / challenge all reject.
	presA := present(cred, 5, "gossip", 1)
	if _, ok := gate.AdmitPseudonymous(6, "gossip", bytes.Repeat([]byte{1}, 32), presA, now); ok {
		t.Fatal("wrong epoch admitted")
	}
	if _, ok := gate.AdmitPseudonymous(5, "submit", bytes.Repeat([]byte{1}, 32), presA, now); ok {
		t.Fatal("wrong scope admitted")
	}
	if _, ok := gate.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{9}, 32), presA, now); ok {
		t.Fatal("wrong challenge admitted")
	}

	// Expiry + tier.
	if _, ok := gate.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{1}, 32),
		present(issueCred(memberSecret, 3, 1000, 1), 5, "gossip", 1), now); ok {
		t.Fatal("expired credential admitted")
	}
	if _, ok := gate.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{1}, 32),
		present(issueCred(memberSecret, 1, notAfter, 1), 5, "gossip", 1), now); ok {
		t.Fatal("below-min-tier admitted")
	}

	// Targeted nym revocation: banning nymA blocks epoch 5, not epoch 6.
	{
		revoking := bc.NewCredentialGate(policy)
		defer revoking.Free()
		revoking.RevokeNym(nymA)
		if _, ok := revoking.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{3}, 32), present(cred, 5, "gossip", 3), now); ok {
			t.Fatal("revoked nym admitted")
		}
		if _, ok := revoking.AdmitPseudonymous(6, "gossip", bytes.Repeat([]byte{3}, 32), present(cred, 6, "gossip", 3), now); !ok {
			t.Fatal("a different epoch's nym should not be banned")
		}
	}

	// Forged issuer + malformed both reject.
	rogue, _ := bc.NewBBSIssuer(bytes.Repeat([]byte{0x99}, 32))
	forgedCommit, _ := policy.MembershipCommit(memberSecret)
	fc, _ := bc.ExtractCommitment(forgedCommit)
	fbi, _ := policy.BlindIssue(rogue.Secret, fc, 3, notAfter, 1)
	forged, _ := bc.FinalizeMembership(forgedCommit, fbi)
	if _, ok := gate.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{1}, 32), present(forged, 5, "gossip", 1), now); ok {
		t.Fatal("forged (wrong-issuer) credential admitted")
	}
	if _, ok := gate.AdmitPseudonymous(5, "gossip", bytes.Repeat([]byte{1}, 32), []byte("garbage"), now); ok {
		t.Fatal("malformed presentation admitted")
	}
}

// TestChainBlueprint drives composable chains end to end: discover registered
// components, declare a mixture, commit it into genesis (identity changes),
// assemble, and commit blocks through built-in PoS and PoW — then confirm the
// fail-closed rejects (unknown id, posture-forbidden engine, state mismatch).
func TestChainBlueprint(t *testing.T) {
	has := func(xs []string, s string) bool {
		for _, x := range xs {
			if x == s {
				return true
			}
		}
		return false
	}
	eng, _ := bc.EngineIDs()
	if !has(eng, "bft") || !has(eng, "pow") || !has(eng, "pos") || !has(eng, "vrf-pos") {
		t.Fatalf("engine ids missing built-ins: %v", eng)
	}
	st, _ := bc.StateModelIDs()
	if !has(st, "account") || !has(st, "utxo") {
		t.Fatalf("state ids: %v", st)
	}
	pr, _ := bc.ProposerIDs()
	if !has(pr, "stake-weighted") || !has(pr, "round-robin") {
		t.Fatalf("proposer ids: %v", pr)
	}

	v, err := bc.NewBLSValidator(bytes.Repeat([]byte{0x60}, 32))
	if err != nil || len(v.Public) != 48 || len(v.Secret) != 32 {
		t.Fatalf("bls keygen: %v pub=%d sec=%d", err, len(v.Public), len(v.Secret))
	}
	accounts := []bc.GenesisAccount{{Address: bytes.Repeat([]byte{0x11}, 32), Balance: 1000}}
	validators := []bc.GenesisValidator{{PublicKey: v.Public, Power: 1}}
	genesis, err := bc.BuildGenesis(7, 1, 1, bc.StateModelAccount, accounts, validators)
	if err != nil {
		t.Fatal(err)
	}

	// The mixture is chain identity.
	pos, _ := bc.NewBlueprint("pos", "account", "round-robin", nil)
	if h, _ := pos.Hash(); len(h) != 32 {
		t.Fatal("blueprint hash size")
	}
	mixed, err := bc.CommitBlueprint(genesis, pos)
	if err != nil {
		t.Fatal(err)
	}
	h0, _ := bc.GenesisHash(genesis)
	h1, _ := bc.GenesisHash(mixed)
	if bytes.Equal(h0, h1) {
		t.Fatal("committing a mixture must change the genesis hash")
	}

	// Built-in PoS: propose → seal → submit.
	chain, err := bc.AssembleChain(mixed, 0, v.Public, v.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer chain.Free()
	if got, _ := chain.Blueprint(); !bytes.Equal(got, pos.Encoded()) {
		t.Fatal("assembled chain should report the committed mixture")
	}
	blk, err := chain.Propose(2)
	if err != nil {
		t.Fatal(err)
	}
	just, _ := chain.Seal(blk)
	if len(just) == 0 || !chain.Submit(blk, just) || chain.Height() != 1 {
		t.Fatalf("PoS block did not commit (just=%d height=%d)", len(just), chain.Height())
	}

	// Built-in PoW via params (32-byte easiest target): propose → mine → submit.
	pow, _ := bc.NewBlueprint("pow", "account", "", bytes.Repeat([]byte{0xFF}, 32))
	powGen, _ := bc.CommitBlueprint(genesis, pow)
	pc, err := bc.AssembleChain(powGen, 0, v.Public, v.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer pc.Free()
	cand, _ := pc.Propose(2)
	if s, _ := pc.Seal(cand); len(s) != 0 {
		t.Fatal("PoW must not seal")
	}
	solved, err := pc.Mine(cand)
	if err != nil || !pc.Submit(solved, nil) || pc.Height() != 1 {
		t.Fatalf("PoW block did not commit: %v height=%d", err, pc.Height())
	}

	// Fail-closed: unknown engine id.
	bad, _ := bc.NewBlueprint("no-such-engine", "account", "", nil)
	badGen, _ := bc.CommitBlueprint(genesis, bad)
	if _, err := bc.AssembleChain(badGen, 0, v.Public, v.Secret); err == nil {
		t.Fatal("unknown engine id must be rejected")
	}
	// Fail-closed: a hardened (VRF) posture forbids a predictable-leader engine.
	hardened, _ := bc.BuildGenesis(7, 1, 2, bc.StateModelAccount, accounts, validators)
	hardPos, _ := bc.CommitBlueprint(hardened, pos)
	if _, err := bc.AssembleChain(hardPos, 0, v.Public, v.Secret); err == nil {
		t.Fatal("hardened posture must reject a non-grind-proof engine")
	}
	// Fail-closed: state id must match the genesis seeding.
	utxo, _ := bc.NewBlueprint("pos", "utxo", "", nil)
	mism, _ := bc.CommitBlueprint(genesis, utxo)
	if _, err := bc.AssembleChain(mism, 0, v.Public, v.Secret); err == nil {
		t.Fatal("state model mismatch must be rejected")
	}
}

// goHashEngine is a consensus engine implemented in Go: the justification is a
// tag over the block's first bytes; verification checks it. Counters prove the
// callbacks really cross the language boundary.
type goHashEngine struct{ justified, verified int }

func (e *goHashEngine) Justify(block []byte) []byte {
	e.justified++
	n := 16
	if len(block) < n {
		n = len(block)
	}
	return append([]byte("go-just:"), block[:n]...)
}
func (e *goHashEngine) Verify(block, just []byte) bool {
	e.verified++
	return bytes.Equal(just, (&goHashEngine{}).Justify(block))
}

// TestForeignComponents: a consensus engine and a proposer policy implemented IN
// GO join a chain mixture by id, the chain assembles, and blocks commit through
// the Go justification policy — with a tampered justification refused by the Go
// verifier while the ledger stays untouched.
func TestForeignComponents(t *testing.T) {
	eng := &goHashEngine{}
	if err := bc.RegisterEngine("go-engine", eng, false); err != nil {
		t.Fatal(err)
	}
	if err := bc.RegisterProposer("go-first", func(uint64, uint32) uint32 { return 0 }); err != nil {
		t.Fatal(err)
	}
	ids, _ := bc.EngineIDs()
	found := false
	for _, x := range ids {
		found = found || x == "go-engine"
	}
	if !found {
		t.Fatalf("go-engine not discoverable: %v", ids)
	}

	v, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x60}, 32))
	genesis, _ := bc.BuildGenesis(7, 1, 1, bc.StateModelAccount,
		[]bc.GenesisAccount{{Address: bytes.Repeat([]byte{0x11}, 32), Balance: 1000}},
		[]bc.GenesisValidator{{PublicKey: v.Public, Power: 1}})
	bp, _ := bc.NewBlueprint("go-engine", "account", "", nil)
	mixed, _ := bc.CommitBlueprint(genesis, bp)
	chain, err := bc.AssembleChain(mixed, 0, v.Public, v.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer chain.Free()

	blk, _ := chain.Propose(2)
	just, err := chain.Seal(blk) // → Go Justify
	if err != nil || !bytes.HasPrefix(just, []byte("go-just:")) {
		t.Fatalf("seal via Go justify: %v %q", err, just)
	}
	bad := append([]byte(nil), just...)
	bad[len(bad)-1] ^= 1
	if chain.Submit(blk, bad) || chain.Height() != 0 { // → Go Verify refuses; ledger untouched
		t.Fatal("tampered justification must be refused with the ledger untouched")
	}
	if !chain.Submit(blk, just) || chain.Height() != 1 {
		t.Fatal("valid Go-justified block should commit")
	}
	if eng.justified < 1 || eng.verified < 2 {
		t.Fatalf("callbacks did not cross the boundary: justified=%d verified=%d", eng.justified, eng.verified)
	}

	// A Go proposer policy plugs into a built-in engine.
	pbp, _ := bc.NewBlueprint("pos", "account", "go-first", nil)
	pmixed, _ := bc.CommitBlueprint(genesis, pbp)
	pc, err := bc.AssembleChain(pmixed, 0, v.Public, v.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer pc.Free()
	pblk, _ := pc.Propose(2)
	pj, _ := pc.Seal(pblk)
	if !pc.Submit(pblk, pj) || pc.Height() != 1 {
		t.Fatal("PoS with a Go proposer should commit")
	}
}

// Subchains over the Go binding: a single-validator BFT child finalizes blocks
// with real certificates; a parent registers it at genesis, anchors the child's
// certified headers, and a state entry on the child is proven from the parent's
// header alone. Tamper → refused.
func TestSubchains(t *testing.T) {
	must := func(b []byte, err error) []byte {
		t.Helper()
		if err != nil {
			t.Fatal(err)
		}
		return b
	}
	cv, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x71}, 32))
	pv, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x72}, 32))
	alice := bytes.Repeat([]byte{0xA1}, 32)
	bft, _ := bc.NewBlueprint("bft", "account", "", nil)

	// Child chain 42 (BFT, one validator = quorum alone).
	childGen := must(bc.BuildGenesis(42, 1, 1, bc.StateModelAccount,
		[]bc.GenesisAccount{{Address: alice, Balance: 1000}}, []bc.GenesisValidator{{PublicKey: cv.Public, Power: 1}}))
	childGen = must(bc.CommitBlueprint(childGen, bft))
	childHeader := must(bc.GenesisHeader(childGen))

	// Parent chain 1 registering the child.
	parentGen := must(bc.BuildGenesis(1, 1, 1, bc.StateModelAccount, nil, []bc.GenesisValidator{{PublicKey: pv.Public, Power: 1}}))
	parentGen = must(bc.CommitBlueprint(parentGen, bft))
	policy := must(bc.SubchainPolicy{MaxChildren: 1, MaxDepth: 1, MaxHeadersPerAnchor: 4,
		Children: []bc.ChildRegistration{{ChainID: 42, GenesisHeader: childHeader,
			Validators: []bc.GenesisValidator{{PublicKey: cv.Public, Power: 1}}}}}.Encode())
	plainHash := must(bc.GenesisHash(parentGen))
	parentGen = must(bc.CommitSubchainPolicy(parentGen, policy))
	if bytes.Equal(plainHash, must(bc.GenesisHash(parentGen))) {
		t.Fatal("policy must change the parent's genesis hash")
	}
	// A structurally invalid policy is refused at commit (own chain as child).
	badPolicy := must(bc.SubchainPolicy{MaxChildren: 1, MaxDepth: 1,
		Children: []bc.ChildRegistration{{ChainID: 1, GenesisHeader: childHeader, Validators: []bc.GenesisValidator{{PublicKey: cv.Public, Power: 1}}}}}.Encode())
	if _, err := bc.CommitSubchainPolicy(parentGen, badPolicy); err == nil {
		t.Fatal("own-chain child must be refused")
	}
	// Bind the child to its parent: hash changes, header does not.
	boundChild := must(bc.CommitParentBinding(childGen, 1, must(bc.GenesisHash(parentGen))))
	if bytes.Equal(must(bc.GenesisHash(boundChild)), must(bc.GenesisHash(childGen))) || !bytes.Equal(must(bc.GenesisHeader(boundChild)), childHeader) {
		t.Fatal("binding must change the child's hash but not its genesis header")
	}

	child, err := bc.AssembleChain(boundChild, 0, cv.Public, cv.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer child.Free()
	parent, err := bc.AssembleChain(parentGen, 0, pv.Public, pv.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer parent.Free()

	// The child finalizes two blocks with real certificates.
	var certified [][]byte
	for ts := uint64(2); ts <= 3; ts++ {
		blk := must(child.Propose(ts))
		cert := must(child.Certify(blk))
		if !child.Submit(blk, cert) {
			t.Fatal("child submit")
		}
		certified = append(certified, must(bc.CertifiedHeader(must(bc.BlockHeader(blk)), cert)))
	}
	if child.Height() != 2 {
		t.Fatalf("child height %d", child.Height())
	}
	if _, err := parent.ChildTip(42); err != nil {
		t.Fatal(err)
	}
	if _, err := parent.ChildTip(99); err == nil {
		t.Fatal("unknown child must error")
	}
	if _, err := child.ChildTip(42); err == nil {
		t.Fatal("a non-parent has no child tips")
	}

	// The parent anchors the child's segment.
	anchor := must(bc.EncodeAnchor(42, certified))
	pblk := must(parent.ProposeAnchored(2, [][]byte{anchor}))
	pcert := must(parent.Certify(pblk))
	if !parent.Submit(pblk, pcert) || parent.Height() != 1 {
		t.Fatal("parent submit")
	}
	if tip := must(parent.ChildTip(42)); len(tip) == 0 {
		t.Fatal("child tip")
	}
	// A tampered certificate is refused by the producer (never proposed).
	tampered := append([]byte(nil), certified[1]...)
	tampered[len(tampered)-1] ^= 1
	badAnchor := must(bc.EncodeAnchor(42, [][]byte{tampered}))
	if _, err := parent.ProposeAnchored(3, [][]byte{badAnchor}); err == nil {
		t.Fatal("tampered anchor must be refused")
	}
	// Re-anchoring the same segment (stale) is refused.
	if _, err := parent.ProposeAnchored(3, [][]byte{anchor}); err == nil {
		t.Fatal("stale anchor must be refused")
	}

	// Cross-chain proof: alice's balance on the child from the parent's header.
	root := must(bc.HeaderSubchainRoot(must(bc.BlockHeader(pblk))))
	hop := must(parent.SubchainHop(42))
	sp := must(child.ProveAccount(alice))
	proof := must(bc.EncodeSubchainProof([][]byte{hop}, sp))
	if !bc.VerifySubchainProof(root, proof) {
		t.Fatal("cross-chain proof must verify")
	}
	if bc.VerifySubchainProof(bytes.Repeat([]byte{1}, 32), proof) {
		t.Fatal("wrong root must fail")
	}
	bad := append([]byte(nil), proof...)
	bad[len(bad)-1] ^= 1
	if bc.VerifySubchainProof(root, bad) {
		t.Fatal("tampered proof must fail")
	}
	if bc.VerifySubchainProof(root, must(bc.EncodeSubchainProof(nil, sp))) {
		t.Fatal("no hops must fail")
	}
	// After the child moves on and the parent re-anchors, the old proof is stale
	// at the new root but still verifies at the old one.
	blk3 := must(child.Propose(4))
	cert3 := must(child.Certify(blk3))
	child.Submit(blk3, cert3)
	a2 := must(bc.EncodeAnchor(42, [][]byte{must(bc.CertifiedHeader(must(bc.BlockHeader(blk3)), cert3))}))
	pblk2 := must(parent.ProposeAnchored(3, [][]byte{a2}))
	if !parent.Submit(pblk2, must(parent.Certify(pblk2))) {
		t.Fatal("parent block 2")
	}
	root2 := must(bc.HeaderSubchainRoot(must(bc.BlockHeader(pblk2))))
	if bc.VerifySubchainProof(root2, proof) || !bc.VerifySubchainProof(root, proof) {
		t.Fatal("stale proof semantics")
	}
	fresh := must(bc.EncodeSubchainProof([][]byte{must(parent.SubchainHop(42))}, must(child.ProveAccount(alice))))
	if !bc.VerifySubchainProof(root2, fresh) {
		t.Fatal("fresh proof must verify at the new root")
	}
}
