package aviary_test

import (
	"bytes"
	"sort"
	"strings"
	"testing"

	bc "github.com/Crdzbird/Aviary/bridge/bindings/go/aviary"
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
	must(back.SubmitTransfer(aSec, bAddr, 1, 2))
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

type mesh struct {
	nodes    []*bc.NetNode
	queue    [][3]interface{}
	timeouts []struct {
		round uint32
		step  uint8
		node  int
	}
}

func (m *mesh) apply(node int, o bc.Outbound) {
	for _, r := range o.Relays {
		m.queue = append(m.queue, [3]interface{}{node, int(r.To), r.Message})
	}
	for _, to := range o.Timeouts {
		m.timeouts = append(m.timeouts, struct {
			round uint32
			step  uint8
			node  int
		}{to.Round, to.Step, node})
	}
}

func (m *mesh) run(t *testing.T, done func() bool) {
	for step := 0; step < 20000; step++ {
		if done() {
			return
		}
		if len(m.queue) > 0 {
			from, to, payload := m.queue[0][0].(int), m.queue[0][1].(int), m.queue[0][2].([]byte)
			m.queue = m.queue[1:]
			o, err := m.nodes[to].OnGossip(uint32(from), payload)
			if err != nil {
				t.Fatal(err)
			}
			m.apply(to, o)
			continue
		}
		if len(m.timeouts) == 0 {
			return
		}
		sort.Slice(m.timeouts, func(i, j int) bool {
			if m.timeouts[i].round != m.timeouts[j].round {
				return m.timeouts[i].round < m.timeouts[j].round
			}
			return m.timeouts[i].step < m.timeouts[j].step
		})
		tm := m.timeouts[0]
		m.timeouts = m.timeouts[1:]
		o, err := m.nodes[tm.node].OnTimeout(bc.TimeoutRequest{Step: tm.step, Round: tm.round})
		if err != nil {
			t.Fatal(err)
		}
		m.apply(tm.node, o)
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
	m := &mesh{}
	for i := 0; i < 4; i++ {
		n, err := bc.NewNetNode(genesis, uint32(i), keys[i].Public, keys[i].Secret, 1)
		if err != nil {
			t.Fatal(err)
		}
		m.nodes = append(m.nodes, n)
	}
	obs, err := bc.NewNetNode(genesis, bc.Observer, nil, nil, 1)
	if err != nil {
		t.Fatal(err)
	}
	m.nodes = append(m.nodes, obs)
	for _, e := range [][2]uint32{{0, 1}, {1, 2}, {2, 3}, {3, 4}} {
		m.nodes[e[0]].AddPeer(e[1])
		m.nodes[e[1]].AddPeer(e[0])
	}
	committed := func() bool {
		for _, n := range m.nodes[:4] {
			if !n.Committed() {
				return false
			}
		}
		return true
	}
	for i, n := range m.nodes {
		o, err := n.Start()
		if err != nil {
			t.Fatal(err)
		}
		m.apply(i, o)
	}
	m.run(t, committed)
	m.run(t, func() bool { return false })
	for i, n := range m.nodes {
		if n.Height() != 1 {
			t.Fatalf("node %d height %d", i, n.Height())
		}
	}
	if !bytes.Equal(must(m.nodes[0].Tip()), must(obs.Tip())) || obs.Balance(alice) != 1000 {
		t.Fatal("observer did not follow")
	}
	if log, _ := m.nodes[0].ReplayLog(); len(log) == 0 {
		t.Fatal("replay log empty")
	}
	for i, n := range m.nodes {
		o, err := n.Advance()
		if err != nil {
			t.Fatal(err)
		}
		m.apply(i, o)
	}
	m.run(t, committed)
	m.run(t, func() bool { return false })
	if m.nodes[3].Height() != 2 || obs.Height() != 2 {
		t.Fatal("second height not reached")
	}
	if err := m.nodes[0].SetTransactions([][]byte{{1, 2}}); err == nil {
		t.Fatal("malformed transaction accepted")
	}
	for _, n := range m.nodes {
		n.Free()
	}

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
