package aviary

import (
	"errors"
	"sort"
)

// PeerMesh is a deterministic in-memory mesh of NetNodes for tests,
// simulations and single process networks. Relays are delivered in emission
// order and a timeout fires only when no relay is pending, earliest round and
// step first, so the same inputs always take the same path to a commit. A
// node's index in Nodes is its mesh id and must match the me it was created
// with.
type PeerMesh struct {
	Nodes    []*NetNode
	queue    []meshRelay
	timeouts []meshTimeout
}

type meshRelay struct {
	from, to uint32
	payload  []byte
}

type meshTimeout struct {
	round uint32
	step  uint8
	node  int
}

// NewPeerMesh wraps nodes; link them with Link or LinkAll.
func NewPeerMesh(nodes ...*NetNode) *PeerMesh { return &PeerMesh{Nodes: nodes} }

// Link makes a and b neighbours in both directions.
func (m *PeerMesh) Link(a, b int) {
	m.Nodes[a].AddPeer(uint32(b))
	m.Nodes[b].AddPeer(uint32(a))
}

// LinkAll links every pair of nodes.
func (m *PeerMesh) LinkAll() {
	for a := range m.Nodes {
		for b := a + 1; b < len(m.Nodes); b++ {
			m.Link(a, b)
		}
	}
}

// Apply queues what node emitted.
func (m *PeerMesh) Apply(node int, o Outbound) {
	for _, r := range o.Relays {
		m.queue = append(m.queue, meshRelay{uint32(node), r.To, r.Message})
	}
	for _, t := range o.Timeouts {
		m.timeouts = append(m.timeouts, meshTimeout{t.Round, t.Step, node})
	}
}

func (m *PeerMesh) each(f func(*NetNode) (Outbound, error)) error {
	for i, n := range m.Nodes {
		o, err := f(n)
		if err != nil {
			return err
		}
		m.Apply(i, o)
	}
	return nil
}

// Start starts every node.
func (m *PeerMesh) Start() error { return m.each((*NetNode).Start) }

// Advance rolls every node into the next height; it fails if one has not
// committed.
func (m *PeerMesh) Advance() error { return m.each((*NetNode).Advance) }

// Deliver hands the next relay to its peer; false when none is pending.
func (m *PeerMesh) Deliver() (bool, error) {
	if len(m.queue) == 0 {
		return false, nil
	}
	r := m.queue[0]
	m.queue = m.queue[1:]
	o, err := m.Nodes[r.to].OnGossip(r.from, r.payload)
	if err != nil {
		return true, err
	}
	m.Apply(int(r.to), o)
	return true, nil
}

// Step delivers one relay, or fires the earliest armed timeout when none is
// pending. It returns false when nothing was outstanding.
func (m *PeerMesh) Step() (bool, error) {
	if ok, err := m.Deliver(); ok || err != nil {
		return ok, err
	}
	if len(m.timeouts) == 0 {
		return false, nil
	}
	sort.SliceStable(m.timeouts, func(i, j int) bool {
		a, b := m.timeouts[i], m.timeouts[j]
		if a.round != b.round {
			return a.round < b.round
		}
		if a.step != b.step {
			return a.step < b.step
		}
		return a.node < b.node
	})
	t := m.timeouts[0]
	m.timeouts = m.timeouts[1:]
	o, err := m.Nodes[t.node].OnTimeout(TimeoutRequest{Step: t.step, Round: t.round})
	if err != nil {
		return true, err
	}
	m.Apply(t.node, o)
	return true, nil
}

// Run steps until until holds or budget steps passed, and reports whether
// until held.
func (m *PeerMesh) Run(until func() bool, budget int) (bool, error) {
	for i := 0; i < budget && !until(); i++ {
		ok, err := m.Step()
		if err != nil {
			return false, err
		}
		if !ok {
			break
		}
	}
	return until(), nil
}

// Settle runs until every node in validators (all nodes when empty) has
// committed, then delivers the remaining relays so observers catch up; armed
// timeouts are left alone, since a committed node keeps re-arming them.
func (m *PeerMesh) Settle(validators ...int) error {
	if len(validators) == 0 {
		for i := range m.Nodes {
			validators = append(validators, i)
		}
	}
	committed := func() bool {
		for _, i := range validators {
			if !m.Nodes[i].Committed() {
				return false
			}
		}
		return true
	}
	ok, err := m.Run(committed, 20000)
	if err != nil {
		return err
	}
	for {
		delivered, err := m.Deliver()
		if err != nil {
			return err
		}
		if !delivered {
			break
		}
	}
	if !ok {
		return errors.New("peer mesh: validators did not commit within the step budget")
	}
	return nil
}

// Free frees every node.
func (m *PeerMesh) Free() {
	for _, n := range m.Nodes {
		n.Free()
	}
}
