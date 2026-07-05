// probe — exercises the Interop server end-to-end and checks Go-side
// self-consistency (a local node must reproduce the server's state root).
package main

import (
	"bytes"
	"context"
	"encoding/hex"
	"fmt"
	"log"
	"time"

	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
	pb "interop/gen"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

func main() {
	conn, err := grpc.NewClient("127.0.0.1:50053", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Fatal(err)
	}
	defer conn.Close()
	c := pb.NewInteropClient(conn)
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	// Two deterministic accounts (shared with the server).
	gen := bc.NewNode(99)
	aSec, _, _, _ := gen.NewAccount()
	bSec, _, _, _ := gen.NewAccount()
	gen.Free()

	scn := &pb.ChainScenario{
		ChainId: 7, AliceSecret: aSec, BobSecret: bSec,
		AliceCredit: 1_000_000, TransferAmount: 250_000, Timestamp: 1,
	}

	// go → flutter analog: server builds + returns proof.
	res, err := c.BuildChain(ctx, scn)
	if err != nil {
		log.Fatal(err)
	}
	fmt.Printf("server stateRoot %s  bob=%d  proof=%dB\n",
		hex.EncodeToString(res.StateRoot)[:16]+"…", res.BobBalance, len(res.Proof))

	// Local replica must match byte-for-byte (determinism).
	local := bc.NewNode(7)
	aAddr := bc.AccountAddress(aSec[32:64])
	bAddr := bc.AccountAddress(bSec[32:64])
	if err := local.Credit(aAddr, 1_000_000); err != nil {
		log.Fatal(err)
	}
	local.SubmitTransfer(aSec, bAddr, 250_000, local.Nonce(aAddr))
	local.ProduceBlock(1)
	fmt.Printf("local  stateRoot %s\n", hex.EncodeToString(local.StateRoot())[:16]+"…")
	must("state roots match", bytes.Equal(res.StateRoot, local.StateRoot()))

	// flutter → go analog: server verifies a locally-produced proof.
	vr, err := c.VerifyProof(ctx, &pb.ProofBundle{StateRoot: local.StateRoot(), Proof: local.ProveAccount(bAddr)})
	if err != nil {
		log.Fatal(err)
	}
	must("server verifies local proof", vr.Ok)
	must("local verifies server proof", bc.VerifyStateProof(res.StateRoot, res.Proof))
	local.Free()

	// go ↔ flutter analog: canonical bytes match.
	in := &pb.CanonicalInput{
		Message: []byte("interop"),
		TxVersion: 3, ChainId: 7, TxBody: []byte{0}, TxAuthor: make([]byte, 32), TxSignature: make([]byte, 64),
		HdrVersion: 2, Height: 1, ParentHash: make([]byte, 32), Timestamp: 1,
		TxRoot: make([]byte, 32), StateRoot: make([]byte, 32), Consensus: nil,
	}
	co, err := c.ComputeCanonical(ctx, in)
	if err != nil {
		log.Fatal(err)
	}
	lh, _ := bc.Hash(in.Message)
	lt, _ := bc.Txid(3, 7, in.TxBody, in.TxAuthor, in.TxSignature)
	lhh, _ := bc.HeaderHash(2, 7, 1, in.ParentHash, 1, in.TxRoot, in.StateRoot, in.Consensus)
	must("hash matches", bytes.Equal(co.Hash, lh))
	must("txid matches", bytes.Equal(co.Txid, lt))
	must("headerHash matches", bytes.Equal(co.HeaderHash, lhh))

	fmt.Println("\nALL INTEROP CHECKS PASS")
}

func must(label string, ok bool) {
	state := "OK"
	if !ok {
		state = "FAIL"
	}
	fmt.Printf("  %-28s %s\n", label, state)
	if !ok {
		log.Fatalf("interop check failed: %s", label)
	}
}
