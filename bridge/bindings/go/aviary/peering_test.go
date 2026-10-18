package aviary_test

import (
	"bytes"
	"strings"
	"testing"

	bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
)

func TestFullMeshWithPeerMesh(t *testing.T) {
	must := ok(t)
	keys := make([]*bc.PortableAccount, 4)
	vset := make([]bc.GenesisValidator, 4)
	for i := range keys {
		keys[i], _ = bc.NewBLSValidator(bytes.Repeat([]byte{byte(0x30 + i)}, 32))
		vset[i] = bc.GenesisValidator{PublicKey: keys[i].Public, Power: 1}
	}
	genesis := must(bc.BuildGenesis(43, 1, 1, 0, nil, vset))
	m := bc.NewPeerMesh()
	for i := range keys {
		n, err := bc.NewNetNode(genesis, uint32(i), keys[i].Public, keys[i].Secret, 1)
		if err != nil {
			t.Fatal(err)
		}
		m.Nodes = append(m.Nodes, n)
	}
	m.LinkAll()
	if err := m.Start(); err != nil {
		t.Fatal(err)
	}
	if err := m.Settle(); err != nil {
		t.Fatal(err)
	}
	for _, n := range m.Nodes {
		if n.Height() != 1 {
			t.Fatal("full mesh height 1")
		}
	}
	if ok, _ := m.Run(func() bool { return false }, 1); ok {
		t.Fatal("run reports a miss")
	}
	m.Free()
}

func TestRecordsFromStoreAndHybrid(t *testing.T) {
	must := ok(t)
	v, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x71}, 32))
	bp, _ := bc.NewBlueprint("bft", "record", "", nil)
	chain, err := bc.AssembleChain(must(bc.CommitBlueprint(must(bc.BuildGenesis(41, 1, 1, 2, nil, []bc.GenesisValidator{{PublicKey: v.Public, Power: 1}})), bp)), 0, v.Public, v.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer chain.Free()
	node := bc.NewNode(1)
	defer node.Free()
	edSecret, edPublic, _, _ := node.NewAccount()
	hySecret, hyPublic, hyAddr, _ := node.NewAccountHybrid()
	store, err := bc.NewKeyStore("pass")
	if err != nil {
		t.Fatal(err)
	}
	defer store.Free()
	edAddr := must(store.AddAccount(edPublic, edSecret))
	k1, k2 := must(bc.Hash([]byte("asset:1"))), must(bc.Hash([]byte("asset:2")))
	must(chain.WriteFromStore(store, edAddr, k1, bc.RecordPut, []byte("a")))
	must(chain.AddTx(must(bc.SignRecordTxHybrid(41, 0, k2, bc.RecordPut, []byte("b"), hyPublic, hySecret))))
	if ev, _ := chain.DrainEvents(); strings.Count(ev, "tx_admitted") != 2 {
		t.Fatalf("admission events: %s", ev)
	}
	blk := must(chain.Propose(2))
	if !chain.Submit(blk, must(chain.Certify(blk))) {
		t.Fatal("submit")
	}
	if ev, _ := chain.DrainEvents(); strings.Count(ev, "tx_removed") != 2 {
		t.Fatalf("removal events: %s", ev)
	}
	r1, _ := chain.Record(k1)
	r2, _ := chain.Record(k2)
	if r1 == nil || !bytes.Equal(r1.Owner, edAddr) || r2 == nil || !bytes.Equal(r2.Owner, hyAddr) {
		t.Fatal("authors")
	}
	store.Lock()
	if _, err := chain.WriteFromStore(store, edAddr, k1, bc.RecordDelete, nil); err == nil {
		t.Fatal("locked store must not sign")
	}
}

func TestPeerRecordsAndAddressBook(t *testing.T) {
	a, _ := bc.GenerateSecureIdentity()
	b, _ := bc.GenerateSecureIdentity()
	ra, err := bc.SignPeerRecord(1, "10.0.0.1", 7001, a, nil)
	if err != nil {
		t.Fatal(err)
	}
	rb, _ := bc.SignPeerRecord(2, "10.0.0.2", 7002, b, nil)
	info, err := bc.ReadPeerRecord(rb)
	if err != nil || info.ID != 2 || info.Host != "10.0.0.2" || info.Port != 7002 || !bytes.Equal(info.SigPublic, b.SigPublic) {
		t.Fatalf("fields: %+v %v", info, err)
	}
	forged := append([]byte(nil), rb...)
	forged[len(forged)-1] ^= 1
	if !bc.VerifyPeerRecord(ra) || bc.VerifyPeerRecord(forged) {
		t.Fatal("verify")
	}
	mine, theirs, evil := bc.NewAddressBook(), bc.NewAddressBook(), bc.NewAddressBook()
	defer mine.Free()
	defer theirs.Free()
	defer evil.Free()
	_ = mine.Add(ra)
	_ = theirs.Add(rb)
	_ = theirs.Add(ra)
	enc, _ := theirs.Encode()
	if n, err := mine.Merge(enc); err != nil || n != 1 || mine.Len() != 2 || !bytes.Equal(mine.Record(2), rb) || mine.Record(9) != nil {
		t.Fatal("merge")
	}
	_ = evil.Add(forged)
	ee, _ := evil.Encode()
	fresh := bc.NewAddressBook()
	defer fresh.Free()
	if n, _ := fresh.Merge(ee); n != 0 {
		t.Fatal("forged record merged")
	}
	if _, err := bc.DecodeAddressBook([]byte{1, 2, 3}); err == nil {
		t.Fatal("malformed book decoded")
	}
}

func TestNoiseSessionAndAdmission(t *testing.T) {
	must := ok(t)
	a, _ := bc.GenerateNoiseIdentity()
	b, _ := bc.GenerateNoiseIdentity()
	init, err := bc.InitiateNoiseSession(a, []byte("aviary"))
	if err != nil {
		t.Fatal(err)
	}
	defer init.Free()
	resp, err := bc.AcceptNoiseSession(b, must(init.HandshakeMessage()), []byte("aviary"))
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Free()
	if err := resp.CompleteResponder(must(init.CompleteInitiator(must(resp.HandshakeMessage())))); err != nil {
		t.Fatal(err)
	}
	if err := resp.VerifyIdentityProof(must(init.IdentityProof())); err != nil {
		t.Fatal(err)
	}
	if err := init.VerifyIdentityProof(must(resp.IdentityProof())); err != nil {
		t.Fatal(err)
	}
	if !init.Open() || !bytes.Equal(must(init.PeerIdentity()), b.SigPublic) {
		t.Fatal("noise open and pinned")
	}
	if pt := must(resp.Decrypt(must(init.Encrypt([]byte("hello"))))); string(pt) != "hello" {
		t.Fatal("noise round trip")
	}

	issuer, _ := bc.NewBBSIssuer(bytes.Repeat([]byte{0x11}, 32))
	policy, err := bc.NewCredentialPolicy(7, issuer.Public, []byte("aviary.credential.v1"), 4, 2)
	if err != nil {
		t.Fatal(err)
	}
	cred := must(policy.IssueCredential(issuer.Secret, bytes.Repeat([]byte{0x77}, 32), 3, 4102444800, 1))
	gate := bc.NewCredentialGate(policy)
	defer gate.Free()
	ch, err := bc.NewAdmissionChallenge(5)
	if err != nil {
		t.Fatal(err)
	}
	ans := must(policy.AdmissionAnswer(cred, ch.Wire))
	if !gate.AdmissionGranted(5, ch.Nonce, ans, 1700000000) || gate.AdmissionGranted(6, ch.Nonce, ans, 1700000000) {
		t.Fatal("admission")
	}
}

func TestNetNodeRecords(t *testing.T) {
	must := ok(t)
	keys := make([]*bc.PortableAccount, 4)
	vset := make([]bc.GenesisValidator, 4)
	for i := range keys {
		keys[i], _ = bc.NewBLSValidator(bytes.Repeat([]byte{byte(0x50 + i)}, 32))
		vset[i] = bc.GenesisValidator{PublicKey: keys[i].Public, Power: 1}
	}
	genesis := must(bc.BuildGenesis(77, 1, 1, 2, nil, vset))
	m := bc.NewPeerMesh()
	for i := range keys {
		n, err := bc.NewNetNode(genesis, uint32(i), keys[i].Public, keys[i].Secret, 1)
		if err != nil {
			t.Fatal(err)
		}
		m.Nodes = append(m.Nodes, n)
	}
	observer, err := bc.NewNetNode(genesis, bc.Observer, nil, nil, 1)
	if err != nil {
		t.Fatal(err)
	}
	m.Nodes = append(m.Nodes, observer)
	defer m.Free()
	m.LinkAll()
	node := bc.NewNode(1)
	defer node.Free()
	secret, _, author, _ := node.NewAccount()
	_, _, heir, _ := node.NewAccount()
	key := bytes.Repeat([]byte{0x42}, 32)
	give := func(nonce uint64, op uint8, value []byte) {
		tx := must(bc.SignRecordTx(77, nonce, key, op, value, secret))
		for _, n := range m.Nodes[:4] {
			if err := n.SetTransactions([][]byte{tx}); err != nil {
				t.Fatal(err)
			}
		}
	}
	give(0, bc.RecordPut, []byte("v1"))
	if err := m.Start(); err != nil {
		t.Fatal(err)
	}
	if err := m.Settle(0, 1, 2, 3); err != nil {
		t.Fatal(err)
	}
	for _, n := range m.Nodes {
		rec, err := n.Record(key)
		if err != nil || rec == nil || rec.Version != 1 || !bytes.Equal(rec.Owner, author) {
			t.Fatalf("record on every node: %v %v", rec, err)
		}
	}
	if observer.RecordNonce(author) != 1 {
		t.Fatal("observer nonce")
	}
	proof := must(m.Nodes[1].ProveRecord(key))
	if !bc.VerifyStateProof(must(observer.StateRoot()), proof) {
		t.Fatal("validator proof under the observer root")
	}
	if rec, err := m.Nodes[2].Record(make([]byte, 32)); err != nil || rec != nil {
		t.Fatal("unknown key reads nil")
	}
	give(1, bc.RecordTransfer, heir)
	if err := m.Advance(); err != nil {
		t.Fatal(err)
	}
	if err := m.Settle(0, 1, 2, 3); err != nil {
		t.Fatal(err)
	}
	if rec, _ := observer.Record(key); rec == nil || !bytes.Equal(rec.Owner, heir) {
		t.Fatal("a list set after the commit lands in the next height")
	}
	account, err := bc.NewNetNode(must(bc.BuildGenesis(78, 1, 1, 0, nil, vset[:1])), 0, keys[0].Public, keys[0].Secret, 1)
	if err != nil {
		t.Fatal(err)
	}
	defer account.Free()
	if _, err := account.Record(key); err == nil || !strings.Contains(err.Error(), "not a record-model chain") {
		t.Fatal("account network refuses record queries")
	}
}
