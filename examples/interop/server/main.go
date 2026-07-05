// Interop server — the Go side of the Go↔Flutter cross-binding ledger check.
package main

import (
	"context"
	"log"
	"net"

	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
	pb "interop/gen"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

type server struct{ pb.UnimplementedInteropServer }

// ComputeCanonical — go ↔ flutter: canonical bytes for identical inputs.
func (s *server) ComputeCanonical(_ context.Context, in *pb.CanonicalInput) (*pb.CanonicalOutput, error) {
	hash, err := bc.Hash(in.GetMessage())
	if err != nil {
		return nil, err
	}
	txid, err := bc.Txid(uint8(in.GetTxVersion()), in.GetChainId(),
		in.GetTxBody(), in.GetTxAuthor(), in.GetTxSignature())
	if err != nil {
		return nil, err
	}
	hdr, err := bc.HeaderHash(uint8(in.GetHdrVersion()), in.GetChainId(), in.GetHeight(),
		in.GetParentHash(), in.GetTimestamp(), in.GetTxRoot(), in.GetStateRoot(), in.GetConsensus())
	if err != nil {
		return nil, err
	}
	return &pb.CanonicalOutput{Hash: hash, Txid: txid, HeaderHash: hdr}, nil
}

// BuildChain — go → flutter: build the scenario, return a verifiable proof.
func (s *server) BuildChain(_ context.Context, in *pb.ChainScenario) (*pb.ChainResult, error) {
	node := bc.NewNode(in.GetChainId())
	defer node.Free()
	return applyScenario(node, in)
}

// VerifyProof — flutter → go: verify a proof the client produced.
func (s *server) VerifyProof(_ context.Context, in *pb.ProofBundle) (*pb.VerifyResult, error) {
	return &pb.VerifyResult{Ok: bc.VerifyStateProof(in.GetStateRoot(), in.GetProof())}, nil
}

// applyScenario runs the deterministic scenario on a node. Both bindings run the
// IDENTICAL steps (shared secrets → identical accounts and deterministic Ed25519
// transfers), so the state root is reproducible across languages.
func applyScenario(node *bc.Node, in *pb.ChainScenario) (*pb.ChainResult, error) {
	aSec, bSec := in.GetAliceSecret(), in.GetBobSecret()
	aAddr := bc.AccountAddress(aSec[32:64])
	bAddr := bc.AccountAddress(bSec[32:64])

	if err := node.Credit(aAddr, in.GetAliceCredit()); err != nil {
		return nil, err
	}
	if _, err := node.SubmitTransfer(aSec, bAddr, in.GetTransferAmount(), node.Nonce(aAddr)); err != nil {
		return nil, err
	}
	height, err := node.ProduceBlock(in.GetTimestamp())
	if err != nil {
		return nil, err
	}
	return &pb.ChainResult{
		Height:       height,
		StateRoot:    node.StateRoot(),
		AliceAddress: aAddr,
		BobAddress:   bAddr,
		AliceBalance: node.Balance(aAddr),
		BobBalance:   node.Balance(bAddr),
		Proof:        node.ProveAccount(bAddr),
	}, nil
}

func main() {
	if err := bc.Init(); err != nil {
		log.Fatalf("blockchainer init: %v", err)
	}
	log.Printf("Interop server — blockchainer %s (crypto %s)", bc.Version(), bc.CryptoBackendVersion())
	lis, err := net.Listen("tcp", ":50053")
	if err != nil {
		log.Fatalf("listen: %v", err)
	}
	srv := grpc.NewServer()
	pb.RegisterInteropServer(srv, &server{})
	reflection.Register(srv)
	log.Printf("listening on :50053")
	if err := srv.Serve(lis); err != nil {
		log.Fatalf("serve: %v", err)
	}
}
