package main

import (
	"context"
	"encoding/hex"
	"fmt"
	"sync"

	bc "github.com/Crdzbird/Aviary/bridge/bindings/go/aviary"
	pb "sentinel/gen"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// fleet is a CHAIN TREE: one fleet root chain anchoring a BFT subchain per
// device. Each device chain's genesis funds an identity account whose address
// is BLAKE3(label) — so "device X belongs to this fleet, at this state" is
// provable from the fleet root's header alone (docs/design/SUBCHAINS.md).
type fleet struct {
	mu       sync.Mutex
	tree     *bc.ChainTree
	rootID   uint64
	labels   map[uint64]string
	identity map[uint64][]byte
	order    []uint64
	ts       uint64
	log      []string
}

const fleetRootID = uint64(9000)

func (f *fleet) note(format string, a ...any) {
	f.log = append(f.log, fmt.Sprintf(format, a...))
	if len(f.log) > 40 {
		f.log = f.log[len(f.log)-40:]
	}
}

func fleetValidator(seed []byte) (bc.ChainTreeValidator, error) {
	v, err := bc.NewBLSValidator(seed)
	if err != nil {
		return bc.ChainTreeValidator{}, err
	}
	return bc.ChainTreeValidator{PublicKey: v.Public, Secret: v.Secret, Power: 1}, nil
}

func (s *server) BuildFleet(_ context.Context, in *pb.FleetSpec) (*pb.Fleet, error) {
	if len(in.DeviceLabels) == 0 {
		return nil, status.Error(codes.InvalidArgument, "at least one device label")
	}
	f := &fleet{rootID: fleetRootID, labels: map[uint64]string{}, identity: map[uint64][]byte{}, ts: 1}
	rootKey, err := fleetValidator(mustHash([]byte("fleet-operator")))
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	root := bc.ChainTreeNode{ChainID: fleetRootID, Validators: []bc.ChainTreeValidator{rootKey}, MaxHeadersPerAnchor: 8}
	f.labels[fleetRootID] = "fleet root"
	f.order = append(f.order, fleetRootID)
	for i, label := range in.DeviceLabels {
		id := fleetRootID + 1 + uint64(i)
		key, err := fleetValidator(mustHash([]byte("device-validator:" + label)))
		if err != nil {
			return nil, status.Error(codes.Internal, err.Error())
		}
		identity := mustHash([]byte(label))
		root.Children = append(root.Children, bc.ChainTreeNode{ChainID: id, Validators: []bc.ChainTreeValidator{key},
			Accounts: []bc.GenesisAccount{{Address: identity, Balance: 1}}})
		f.labels[id] = label
		f.identity[id] = identity
		f.order = append(f.order, id)
	}
	tree, err := bc.BuildChainTree(root)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}
	f.tree = tree
	tree.OnFinalized = func(chainID uint64, _ []byte) { f.note("finalized: %s (chain %d) → height %d", f.labels[chainID], chainID, tree.Height(chainID)) }
	f.note("fleet built: root %d + %d device subchains (genesis bottom-up, bindings top-down)", fleetRootID, len(in.DeviceLabels))
	s.mu.Lock()
	if s.fleet != nil {
		s.fleet.tree.Free()
	}
	s.fleet = f
	s.mu.Unlock()
	return f.pb(), nil
}

func mustHash(b []byte) []byte { h, _ := bc.Hash(b); return h }

func (s *server) getFleet() (*fleet, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.fleet == nil {
		return nil, status.Error(codes.FailedPrecondition, "fleet not built yet")
	}
	return s.fleet, nil
}

func (f *fleet) pb() *pb.Fleet {
	out := &pb.Fleet{Built: true, Log: f.log}
	for _, id := range f.order {
		out.Nodes = append(out.Nodes, &pb.FleetNode{ChainId: id, Label: f.labels[id], Height: f.tree.Height(id),
			PendingAnchors: f.tree.PendingAnchors(id), IdentityAddress: hex.EncodeToString(f.identity[id]), Root: id == f.rootID})
	}
	if tip, err := f.tree.Tip(f.rootID); err == nil {
		if r, err := bc.HeaderSubchainRoot(tip); err == nil {
			out.RootSubchainRoot = hex.EncodeToString(r)
		}
	}
	return out
}

func (s *server) GetFleet(context.Context, *pb.FleetRef) (*pb.Fleet, error) {
	s.mu.Lock()
	f := s.fleet
	s.mu.Unlock()
	if f == nil {
		return &pb.Fleet{Built: false}, nil
	}
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.pb(), nil
}

func (s *server) FleetProduce(_ context.Context, r *pb.FleetRef) (*pb.Fleet, error) {
	f, err := s.getFleet()
	if err != nil {
		return nil, err
	}
	f.mu.Lock()
	defer f.mu.Unlock()
	id := r.ChainId
	if id == 0 {
		id = f.rootID
	}
	pending := f.tree.PendingAnchors(id)
	f.ts++
	if _, err := f.tree.Produce(id, f.ts); err != nil {
		return nil, status.Error(codes.FailedPrecondition, err.Error())
	}
	if pending > 0 {
		f.note("%s anchored %d device subchain(s) in its block %d", f.labels[id], pending, f.tree.Height(id))
	}
	return f.pb(), nil
}

func (s *server) FleetProveDevice(_ context.Context, r *pb.FleetRef) (*pb.FleetProof, error) {
	f, err := s.getFleet()
	if err != nil {
		return nil, err
	}
	f.mu.Lock()
	defer f.mu.Unlock()
	identity, ok := f.identity[r.ChainId]
	if !ok {
		return nil, status.Error(codes.NotFound, "not a device chain")
	}
	proof, err := f.tree.ProveAccount(r.ChainId, identity)
	if err != nil {
		return nil, status.Error(codes.FailedPrecondition, err.Error())
	}
	verified := f.tree.Verify(proof)
	tip, _ := f.tree.Tip(f.rootID)
	root, _ := bc.HeaderSubchainRoot(tip)
	f.note("proof: %s's identity from the fleet root header → %s", f.labels[r.ChainId], map[bool]string{true: "verified", false: "NOT verified (root moved on or device never anchored)"}[verified])
	return &pb.FleetProof{ChainId: r.ChainId, Verified: verified, ProofBytes: uint32(len(proof)),
		RootSubchainRoot: hex.EncodeToString(root), IdentityAddress: hex.EncodeToString(identity)}, nil
}
