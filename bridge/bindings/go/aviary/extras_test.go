package aviary_test

import (
	"bytes"
	"strings"
	"testing"

	bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
)

func ok(t *testing.T) func([]byte, error) []byte {
	return func(b []byte, err error) []byte {
		t.Helper()
		if err != nil {
			t.Fatal(err)
		}
		return b
	}
}

func TestPersistenceSnapshotRestore(t *testing.T) {
	must := ok(t)
	n := bc.NewNode(0x7701)
	aSec, _, aAddr, _ := n.NewAccount()
	_, _, bAddr, _ := n.NewAccount()
	if err := n.Credit(aAddr, 500); err != nil {
		t.Fatal(err)
	}
	must(n.SubmitTransfer(aSec, bAddr, 120, 0))
	if _, err := n.ProduceBlock(1); err != nil {
		t.Fatal(err)
	}
	must(n.SubmitTransfer(aSec, bAddr, 30, 1))
	if _, err := n.ProduceBlock(2); err != nil {
		t.Fatal(err)
	}
	snap := must(n.Snapshot())
	back, err := bc.RestoreNode(snap)
	if err != nil {
		t.Fatal(err)
	}
	defer back.Free()
	if back.Height() != 2 || back.Balance(bAddr) != 150 || back.Nonce(aAddr) != 2 {
		t.Fatalf("restored state mismatch: height %d balance %d nonce %d", back.Height(), back.Balance(bAddr), back.Nonce(aAddr))
	}
	if !bytes.Equal(back.StateRoot(), n.StateRoot()) {
		t.Fatal("state roots differ")
	}
	if !bytes.Equal(must(back.BlockAt(1)), must(n.BlockAt(1))) {
		t.Fatal("archived block differs")
	}
	audited, err := bc.RestoreNodeReplay(snap)
	if err != nil || !bytes.Equal(audited.StateRoot(), back.StateRoot()) {
		t.Fatal("replayed restore disagrees")
	}
	audited.Free()
	if back.NextNonce(aAddr) != 2 {
		t.Fatal("next nonce before queueing")
	}
	must(back.SubmitTransfer(aSec, bAddr, 1, back.NextNonce(aAddr)))
	if back.NextNonce(aAddr) != 3 {
		t.Fatal("next nonce must advance past the pending transfer")
	}
	must(back.SubmitTransfer(aSec, bAddr, 1, back.NextNonce(aAddr)))
	if h, _ := back.ProduceBlock(3); h != 3 {
		t.Fatalf("restored node did not keep producing: %d", h)
	}
	if _, err := bc.RestoreNode([]byte{1, 2, 3}); err == nil {
		t.Fatal("garbage snapshot accepted")
	}
	n.Free()
}

func TestCustodySigning(t *testing.T) {
	must := ok(t)
	n := bc.NewNode(0x7702)
	defer n.Free()
	aSec, aPub, aAddr, _ := n.NewAccount()
	_, _, bAddr, _ := n.NewAccount()
	ks, err := bc.NewKeyStore("hunter2")
	if err != nil {
		t.Fatal(err)
	}
	defer ks.Free()
	stored, _ := ks.AddAccount(aPub, aSec)
	if !bytes.Equal(stored, aAddr) || !ks.Has(aAddr) || ks.Has(bAddr) {
		t.Fatal("store inventory wrong")
	}
	if pk, _ := ks.PublicKey(aAddr); !bytes.Equal(pk, aPub) {
		t.Fatal("public key not kept")
	}
	if addrs, _ := ks.Addresses(); len(addrs) != 1 {
		t.Fatal("one address expected")
	}
	if err := n.Credit(aAddr, 1000); err != nil {
		t.Fatal(err)
	}
	must(n.SubmitTransferFromStore(ks, aAddr, bAddr, 100, 0, 0))
	tx := must(ks.SignTransfer(0x7702, aAddr, bAddr, 1, 1, 3))
	must(n.SubmitTx(tx))
	if _, err := n.ProduceBlock(1); err != nil {
		t.Fatal(err)
	}
	if n.Balance(bAddr) != 101 || n.Balance(aAddr) != 1000-100-1-3 {
		t.Fatalf("custody transfers not applied: a %d b %d", n.Balance(aAddr), n.Balance(bAddr))
	}
	ks.Lock()
	if _, err := n.SubmitTransferFromStore(ks, aAddr, bAddr, 1, 2, 0); err == nil {
		t.Fatal("locked store signed")
	}
	if err := ks.Unlock("hunter2"); err != nil {
		t.Fatal(err)
	}
	if _, err := ks.SignTransfer(0x7702, bAddr, aAddr, 1, 0, 0); err == nil {
		t.Fatal("unknown account signed")
	}
}

func TestEventHook(t *testing.T) {
	must := ok(t)
	n := bc.NewNode(0x7703)
	defer n.Free()
	var seen []string
	n.SetEventHook(func(j string) { seen = append(seen, j) })
	aSec, _, aAddr, _ := n.NewAccount()
	_, _, bAddr, _ := n.NewAccount()
	_ = n.Credit(aAddr, 10)
	must(n.SubmitTransfer(aSec, bAddr, 1, 0))
	if _, err := n.ProduceBlock(1); err != nil {
		t.Fatal(err)
	}
	if len(seen) != 3 || !strings.Contains(seen[0], "tx_admitted") || !strings.Contains(seen[1], "block_finalized") {
		t.Fatalf("hook events: %v", seen)
	}
	if d := n.DrainEvents(); d != "[]" {
		t.Fatalf("buffer should be empty while hooked: %s", d)
	}
	n.SetEventHook(nil)
	must(n.SubmitTransfer(aSec, bAddr, 1, 1))
	if d := n.DrainEvents(); !strings.Contains(d, "tx_admitted") {
		t.Fatal("buffering did not resume after unhook")
	}
}

func TestNetMeshAndSecureSession(t *testing.T) {
	must := ok(t)
	keys := make([]*bc.PortableAccount, 4)
	vset := make([]bc.GenesisValidator, 4)
	for i := range keys {
		k, err := bc.NewBLSValidator(bytes.Repeat([]byte{byte(0x60 + i)}, 32))
		if err != nil {
			t.Fatal(err)
		}
		keys[i] = k
		vset[i] = bc.GenesisValidator{PublicKey: k.Public, Power: 1}
	}
	alice := bytes.Repeat([]byte{0x11}, 32)
	genesis := must(bc.BuildGenesis(42, 1, 1, 0, []bc.GenesisAccount{{Address: alice, Balance: 1000}}, vset))
	m := bc.NewPeerMesh()
	for i := 0; i < 4; i++ {
		n, err := bc.NewNetNode(genesis, uint32(i), keys[i].Public, keys[i].Secret, 1)
		if err != nil {
			t.Fatal(err)
		}
		m.Nodes = append(m.Nodes, n)
	}
	obs, err := bc.NewNetNode(genesis, bc.Observer, nil, nil, 1)
	if err != nil {
		t.Fatal(err)
	}
	m.Nodes = append(m.Nodes, obs)
	for i := 0; i < 4; i++ {
		m.Link(i, i+1)
	}
	validators := []int{0, 1, 2, 3}
	if err := m.Start(); err != nil {
		t.Fatal(err)
	}
	if err := m.Settle(validators...); err != nil {
		t.Fatal(err)
	}
	for i, n := range m.Nodes {
		if n.Height() != 1 {
			t.Fatalf("node %d height %d", i, n.Height())
		}
	}
	if !bytes.Equal(must(m.Nodes[0].Tip()), must(obs.Tip())) || obs.Balance(alice) != 1000 {
		t.Fatal("observer did not follow")
	}
	if log, _ := m.Nodes[0].ReplayLog(); len(log) == 0 {
		t.Fatal("replay log empty")
	}
	if err := m.Advance(); err != nil {
		t.Fatal(err)
	}
	if err := m.Settle(validators...); err != nil {
		t.Fatal(err)
	}
	if m.Nodes[3].Height() != 2 || obs.Height() != 2 {
		t.Fatal("second height not reached")
	}
	if ev, _ := m.Nodes[2].DrainEvents(); !strings.Contains(ev, "block_finalized") {
		t.Fatalf("net events: %s", ev)
	}
	if mt, _ := m.Nodes[2].Metrics(); !strings.Contains(mt, "aviary_gossip_received_total") || !strings.Contains(mt, "aviary_chain_height 2") {
		t.Fatalf("net metrics: %s", mt)
	}
	if err := m.Nodes[0].SetTransactions([][]byte{{1, 2}}); err == nil {
		t.Fatal("malformed transaction accepted")
	}
	m.Free()

	a, _ := bc.GenerateSecureIdentity()
	b, _ := bc.GenerateSecureIdentity()
	init, err := bc.InitiateSecureSession(b.KemPublic, a.SigPublic, a.SigSecret)
	if err != nil {
		t.Fatal(err)
	}
	defer init.Free()
	msg1 := must(init.HandshakeMessage())
	resp, err := bc.AcceptSecureSession(b, msg1)
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Free()
	if err := init.Complete(must(resp.HandshakeMessage())); err != nil {
		t.Fatal(err)
	}
	if !init.Open() || !resp.Open() {
		t.Fatal("sessions not open")
	}
	if pk, _ := init.PeerIdentity(); !bytes.Equal(pk, b.SigPublic) {
		t.Fatal("responder not pinned")
	}
	frame := must(init.Encrypt([]byte("hello")))
	if got := must(resp.Decrypt(frame)); string(got) != "hello" {
		t.Fatal("round trip failed")
	}
	frame[len(frame)-1] ^= 1
	if _, err := resp.Decrypt(frame); err == nil {
		t.Fatal("tampered frame accepted")
	}
	if _, err := bc.AcceptSecureSession(a, msg1); err == nil {
		t.Fatal("wrong responder identity accepted")
	}
}

func TestChainObservability(t *testing.T) {
	must := ok(t)
	v, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x70}, 32))
	bft, _ := bc.NewBlueprint("bft", "account", "", nil)
	genesis := must(bc.CommitBlueprint(must(bc.BuildGenesis(9, 1, 1, 0, nil, []bc.GenesisValidator{{PublicKey: v.Public, Power: 1}})), bft))
	chain, err := bc.AssembleChain(genesis, 0, v.Public, v.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer chain.Free()
	var hooked []string
	chain.SetEventHook(func(j string) { hooked = append(hooked, j) })
	blk := must(chain.Propose(2))
	if !chain.Submit(blk, must(chain.Certify(blk))) {
		t.Fatal("submit")
	}
	if len(hooked) != 1 || !strings.Contains(hooked[0], "block_finalized") {
		t.Fatalf("hook: %v", hooked)
	}
	chain.SetEventHook(nil)
	if !chain.Submit([]byte{1, 2, 3}, nil) == false {
		t.Fatal("garbage must be refused")
	}
	mt, _ := chain.Metrics()
	if !strings.Contains(mt, "aviary_chain_height 1") || !strings.Contains(mt, "aviary_blocks_finalized_total 1") {
		t.Fatalf("metrics: %s", mt)
	}
}
