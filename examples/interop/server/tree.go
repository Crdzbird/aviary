package main

import (
	"context"

	bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
	pb "interop/gen"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// treeSpec is the shared, deterministic family: every validator key derives
// from (seed || role), so Go and Flutter build byte-identical genesis specs and
// therefore identical headers and roots.
func treeSpec(seed, alice []byte, credit uint64) (bc.ChainTreeNode, error) {
	val := func(role string) (bc.ChainTreeValidator, error) {
		ikm, err := bc.Hash(append(append([]byte{}, seed...), []byte(role)...))
		if err != nil {
			return bc.ChainTreeValidator{}, err
		}
		v, err := bc.NewBLSValidator(ikm)
		if err != nil {
			return bc.ChainTreeValidator{}, err
		}
		return bc.ChainTreeValidator{PublicKey: v.Public, Secret: v.Secret, Power: 1}, nil
	}
	r, err := val("root")
	if err != nil {
		return bc.ChainTreeNode{}, err
	}
	c, err := val("child")
	if err != nil {
		return bc.ChainTreeNode{}, err
	}
	g, err := val("grandchild")
	if err != nil {
		return bc.ChainTreeNode{}, err
	}
	return bc.ChainTreeNode{ChainID: 1, Validators: []bc.ChainTreeValidator{r}, Children: []bc.ChainTreeNode{
		{ChainID: 42, Validators: []bc.ChainTreeValidator{c}, Children: []bc.ChainTreeNode{
			{ChainID: 420, Validators: []bc.ChainTreeValidator{g}, Accounts: []bc.GenesisAccount{{Address: alice, Balance: credit}}}}}}}, nil
}

func (s *server) BuildTree(_ context.Context, in *pb.TreeScenario) (*pb.TreeResult, error) {
	if len(in.AliceAddress) != 32 {
		return nil, status.Error(codes.InvalidArgument, "alice address must be 32 bytes")
	}
	spec, err := treeSpec(in.Seed, in.AliceAddress, in.AliceCredit)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	tree, err := bc.BuildChainTree(spec)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	defer tree.Free()
	// The same deterministic schedule as the Flutter side: two grandchild blocks,
	// one child block (anchors them), one root block (anchors the child).
	for _, step := range []struct{ id, ts uint64 }{{420, 2}, {420, 3}, {42, 2}, {1, 2}} {
		if _, err := tree.Produce(step.id, step.ts); err != nil {
			return nil, status.Error(codes.Internal, err.Error())
		}
	}
	ids, _ := tree.ChainIDs()
	tip, _ := tree.Tip(1)
	root, _ := bc.HeaderSubchainRoot(tip)
	proof, err := tree.ProveAccount(420, in.AliceAddress)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	return &pb.TreeResult{ChainIds: ids, RootSubchainRoot: root, RootTipHeader: tip, Proof: proof, RootHeight: tree.Height(1)}, nil
}

func (s *server) VerifyTreeProof(_ context.Context, in *pb.TreeProofBundle) (*pb.VerifyResult, error) {
	return &pb.VerifyResult{Ok: bc.VerifySubchainProof(in.RootSubchainRoot, in.Proof)}, nil
}
