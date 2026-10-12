package aviary_test

import (
	"bytes"
	"testing"

	bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
)

func TestRecords(t *testing.T) {
	must := ok(t)
	v, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x70}, 32))
	bp, _ := bc.NewBlueprint("bft", "record", "", nil)
	genesis := must(bc.CommitBlueprint(must(bc.BuildGenesis(31, 1, 1, 2, nil, []bc.GenesisValidator{{PublicKey: v.Public, Power: 1}})), bp))
	chain, err := bc.AssembleChain(genesis, 0, v.Public, v.Secret)
	if err != nil {
		t.Fatal(err)
	}
	defer chain.Free()
	if chain.ChainID() != 31 {
		t.Fatal("chain id")
	}
	node := bc.NewNode(1)
	defer node.Free()
	aliceSecret, _, aliceAddr, err := node.NewAccount()
	if err != nil {
		t.Fatal(err)
	}
	bobSecret, _, bobAddr, err := node.NewAccount()
	if err != nil {
		t.Fatal(err)
	}
	key := must(bc.Hash([]byte("asset:PUMP-0042")))
	must(chain.Write(aliceSecret, key, bc.RecordPut, []byte(`{"status":"in service"}`)))
	must(chain.Write(aliceSecret, key, bc.RecordPut, []byte(`{"status":"serviced"}`)))
	if chain.PendingCount() != 2 {
		t.Fatal("two writes queued")
	}
	if _, err := chain.Write(bobSecret, key, bc.RecordPut, []byte("x")); err == nil {
		t.Fatal("non-owner must be refused")
	}
	blk := must(chain.Propose(2))
	if !chain.Submit(blk, must(chain.Certify(blk))) || chain.PendingCount() != 0 {
		t.Fatal("block with the queued writes")
	}
	rec, err := chain.Record(key)
	if err != nil {
		t.Fatal(err)
	}
	if rec == nil || rec.Version != 2 || !bytes.Equal(rec.Owner, aliceAddr) || string(rec.Value) != `{"status":"serviced"}` {
		t.Fatalf("record: %+v", rec)
	}
	proof := must(chain.ProveRecord(key))
	root := must(chain.StateRoot())
	if !bc.VerifyStateProof(root, proof) {
		t.Fatal("proof")
	}
	leaf, err := bc.RecordFromProof(proof)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(leaf.Key, key) || leaf.Version != 2 {
		t.Fatalf("leaf: %+v", leaf)
	}
	must(chain.Write(aliceSecret, key, bc.RecordTransfer, bobAddr))
	blk = must(chain.Propose(3))
	chain.Submit(blk, must(chain.Certify(blk)))
	rec, _ = chain.Record(key)
	if rec == nil || !bytes.Equal(rec.Owner, bobAddr) {
		t.Fatal("transfer")
	}
	must(chain.Write(bobSecret, key, bc.RecordDelete, nil))
	blk = must(chain.Propose(4))
	chain.Submit(blk, must(chain.Certify(blk)))
	if r, err := chain.Record(key); err != nil || r != nil {
		t.Fatal("deleted")
	}
	if missing, err := chain.Record(must(bc.Hash([]byte("nothing")))); err != nil || missing != nil {
		t.Fatal("missing record is nil")
	}
}
