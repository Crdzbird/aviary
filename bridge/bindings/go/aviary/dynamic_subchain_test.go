package aviary_test

import (
	"bufio"
	"bytes"
	"encoding/hex"
	"os"
	"strings"
	"testing"

	bc "github.com/Crdzbird/aviary/bridge/bindings/go/aviary"
)

// loadSubchainVectors reads vectors/subchain_v2_vectors.txt from the
// repository; outside a repository checkout the replay is skipped.
func loadSubchainVectors(t *testing.T) map[string]string {
	t.Helper()
	f, err := os.Open("../../../../vectors/subchain_v2_vectors.txt")
	if err != nil {
		t.Skip("vectors/subchain_v2_vectors.txt is not available outside the repository")
	}
	defer f.Close()
	out := map[string]string{}
	scanner := bufio.NewScanner(f)
	scanner.Buffer(make([]byte, 1<<20), 1<<20)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		fields := strings.Fields(line)
		if len(fields) != 2 {
			t.Fatalf("malformed vector line: %q", line)
		}
		out[fields[0]] = fields[1]
	}
	return out
}

type subchainFixture struct {
	t      *testing.T
	must   func([]byte, error) []byte
	secret []byte
}

func (f subchainFixture) recordGenesis(chainID uint64, v *bc.PortableAccount) []byte {
	bp, err := bc.NewBlueprint("bft", "record", "", nil)
	if err != nil {
		f.t.Fatal(err)
	}
	base := f.must(bc.BuildGenesis(chainID, 1, 1, 2, nil, []bc.GenesisValidator{{PublicKey: v.Public, Power: 1}}))
	return f.must(bc.CommitBlueprint(base, bp))
}

func (f subchainFixture) assemble(genesis []byte, v *bc.PortableAccount) *bc.Chain {
	c, err := bc.AssembleChain(genesis, 0, v.Public, v.Secret)
	if err != nil {
		f.t.Fatal(err)
	}
	if err := c.EnableArchive(); err != nil {
		f.t.Fatal(err)
	}
	return c
}

func (f subchainFixture) commit(c *bc.Chain, block []byte) []byte {
	if !c.Submit(block, f.must(c.Certify(block))) {
		f.t.Fatal("block refused")
	}
	return block
}

func (f subchainFixture) registration(chainID uint64, genesis []byte, v *bc.PortableAccount) []byte {
	reg := bc.ChildRegistration{
		ChainID:       chainID,
		GenesisHeader: f.must(bc.GenesisHeader(genesis)),
		Validators:    []bc.GenesisValidator{{PublicKey: v.Public, Power: 1}},
	}
	return f.must(reg.Encode())
}

func (f subchainFixture) sign(op []byte, parentID uint64, parentHash []byte) []byte {
	return f.must(bc.SignOp(op, parentID, parentHash, 0, bc.SigAlgoEd25519, f.secret))
}

func TestSubchainV2Vectors(t *testing.T) {
	want := loadSubchainVectors(t)
	must := ok(t)
	seen := 0
	check := func(name string, got []byte) {
		t.Helper()
		seen++
		if want[name] != hex.EncodeToString(got) {
			t.Fatalf("vector %s: got %x", name, got)
		}
	}
	secret, err := hex.DecodeString(want["registrar_secret"])
	if err != nil || len(secret) != 64 {
		t.Fatal("registrar_secret")
	}
	f := subchainFixture{t: t, must: must, secret: secret}
	server, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x01}, 32))
	user, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x10}, 32))
	check("registrar_secret", secret)

	policy := must(bc.DynamicSubchainPolicy{
		MaxDepth:   1,
		Threshold:  1,
		Registrars: []bc.Registrar{{SigAlgo: bc.SigAlgoEd25519, PublicKey: secret[32:]}},
	}.Encode())
	check("policy", policy)
	mainGenesis := must(bc.CommitSubchainPolicy(f.recordGenesis(1, server), policy))
	mainHash := must(bc.GenesisHash(mainGenesis))
	check("main_genesis_hash", mainHash)
	check("main_genesis_header", must(bc.GenesisHeader(mainGenesis)))

	userGenesis := must(bc.CommitParentBinding(f.recordGenesis(1001, user), 1, mainHash))
	registration := f.registration(1001, userGenesis, user)
	check("registration", registration)
	opRegister := must(bc.OpRegister(0, 0, registration))
	check("op_register", opRegister)
	check("op_register_digest", must(bc.OpDigest(opRegister, 1, mainHash)))
	opRegisterSigned := f.sign(opRegister, 1, mainHash)
	check("op_register_signed", opRegisterSigned)

	mainChain := f.assemble(mainGenesis, server)
	defer mainChain.Free()
	userChain := f.assemble(userGenesis, user)
	defer userChain.Free()
	header1 := must(bc.BlockHeader(f.commit(mainChain, must(mainChain.ProposeSubchain(10, nil, [][]byte{opRegisterSigned})))))
	check("header_1", header1)
	check("header_1_hash", must(bc.EncodedHeaderHash(header1)))
	check("subchain_root_1", must(bc.HeaderSubchainRoot(header1)))
	record1, err := mainChain.ChildRecord(1001)
	if err != nil {
		t.Fatal(err)
	}
	check("child_record_1", record1.Encoded)

	key := bytes.Repeat([]byte{0x42}, 32)
	tx := must(bc.SignRecordTx(1001, 0, key, bc.RecordPut, []byte("door opened"), secret))
	must(userChain.AddTx(tx))
	f.commit(userChain, must(userChain.Propose(20)))
	f.commit(userChain, must(userChain.Propose(21)))
	anchor := must(bc.PendingAnchor(mainChain, userChain, 0))
	check("anchor", anchor)
	header2 := must(bc.BlockHeader(f.commit(mainChain, must(mainChain.ProposeSubchain(11, [][]byte{anchor}, nil)))))
	check("header_2_hash", must(bc.EncodedHeaderHash(header2)))
	root2 := must(bc.HeaderSubchainRoot(header2))
	check("subchain_root_2", root2)
	record2, err := mainChain.ChildRecord(1001)
	if err != nil {
		t.Fatal(err)
	}
	check("child_record_2", record2.Encoded)
	proof2 := must(mainChain.ChildProof(1001))
	check("child_proof_2", proof2)
	proven, err := bc.VerifyChildProof(root2, proof2)
	if err != nil {
		t.Fatal(err)
	}
	check("child_proof_2_verified", proven.Encoded)
	if proven.TipHeight != 2 || proven.Status != bc.ChildOpen || !bytes.Equal(proven.TipHash, must(bc.EncodedHeaderHash(must(userChain.Tip())))) {
		t.Fatalf("proven record: %+v", proven)
	}
	recordProof := must(userChain.ProveRecord(key))
	if !bc.VerifyStateProof(proven.TipStateRoot, recordProof) {
		t.Fatal("record proof under the proven tip")
	}
	path := must(bc.EncodeSubchainProofV2([][]byte{must(mainChain.SubchainHopV2(1001))}, recordProof))
	check("path_proof_2", path)
	if !bc.VerifySubchainProofV2(root2, path) || bc.VerifySubchainProofV2(must(bc.HeaderSubchainRoot(header1)), path) {
		t.Fatal("path proof")
	}
	mainSnapshot := must(mainChain.Snapshot())
	check("main_snapshot_hash_2", must(bc.Hash(mainSnapshot)))
	check("user_snapshot_hash_2", must(bc.Hash(must(userChain.Snapshot()))))

	opClose := must(bc.OpClose(0, 0, 1001, record2.TipHeight, record2.TipHash))
	check("op_close", opClose)
	check("op_close_digest", must(bc.OpDigest(opClose, 1, mainHash)))
	header3 := must(bc.BlockHeader(f.commit(mainChain, must(mainChain.ProposeSubchain(12, nil, [][]byte{f.sign(opClose, 1, mainHash)})))))
	check("header_3_hash", must(bc.EncodedHeaderHash(header3)))
	check("subchain_root_3", must(bc.HeaderSubchainRoot(header3)))
	record3, err := mainChain.ChildRecord(1001)
	if err != nil {
		t.Fatal(err)
	}
	check("child_record_3", record3.Encoded)
	if record3.Status != bc.ChildClosed || record3.ClosedHeight != 3 {
		t.Fatalf("closed record: %+v", record3)
	}
	check("main_snapshot_hash_3", must(bc.Hash(must(mainChain.Snapshot()))))

	wideGenesis := must(bc.CommitSubchainPolicy(f.recordGenesis(2, server), policy))
	wideHash := must(bc.GenesisHash(wideGenesis))
	wide := f.assemble(wideGenesis, server)
	defer wide.Free()
	var ops [][]byte
	for id := uint64(2000); id < 2011; id++ {
		reg := f.registration(id, f.recordGenesis(id, user), user)
		ops = append(ops, f.sign(must(bc.OpRegister(0, 0, reg)), 2, wideHash))
	}
	wideRoot := must(bc.HeaderSubchainRoot(must(bc.BlockHeader(f.commit(wide, must(wide.ProposeSubchain(10, nil, ops)))))))
	check("wide_root", wideRoot)
	wideProof := must(wide.ChildProof(2007))
	check("wide_proof", wideProof)
	wideRecord, err := bc.VerifyChildProof(wideRoot, wideProof)
	if err != nil {
		t.Fatal(err)
	}
	check("wide_proof_verified", wideRecord.Encoded)
	if wideRecord.ChainID != 2007 || wideRecord.Index != 7 {
		t.Fatalf("wide record: %+v", wideRecord)
	}
	if seen != len(want) {
		t.Fatalf("replayed %d vectors, the file pins %d", seen, len(want))
	}
}

func TestDynamicSubchainRestartAndRejections(t *testing.T) {
	must := ok(t)
	node := bc.NewNode(0x5501)
	defer node.Free()
	secret, _, _, err := node.NewAccount()
	if err != nil {
		t.Fatal(err)
	}
	stranger, _, _, err := node.NewAccount()
	if err != nil {
		t.Fatal(err)
	}
	f := subchainFixture{t: t, must: must, secret: secret}
	server, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x02}, 32))
	user, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x12}, 32))
	policy := must(bc.DynamicSubchainPolicy{
		MaxDepth:   1,
		Threshold:  1,
		Registrars: []bc.Registrar{{SigAlgo: bc.SigAlgoEd25519, PublicKey: secret[32:]}},
	}.Encode())
	mainGenesis := must(bc.CommitSubchainPolicy(f.recordGenesis(7, server), policy))
	mainHash := must(bc.GenesisHash(mainGenesis))
	userGenesis := must(bc.CommitParentBinding(f.recordGenesis(7001, user), 7, mainHash))
	mainChain := f.assemble(mainGenesis, server)
	defer mainChain.Free()
	userChain := f.assemble(userGenesis, user)
	defer userChain.Free()
	if counts, isDynamic := mainChain.RegistryCounts(); !isDynamic || counts.Records != 0 {
		t.Fatal("empty dynamic registry")
	}
	if _, isDynamic := userChain.RegistryCounts(); isDynamic {
		t.Fatal("a leaf chain has no registry")
	}

	op := must(bc.OpRegister(0, 0, f.registration(7001, userGenesis, user)))
	if _, err := mainChain.ProposeSubchain(10, nil, [][]byte{op}); err == nil || !strings.Contains(err.Error(), "not enough approvals") {
		t.Fatalf("unsigned operation: %v", err)
	}
	forged := must(bc.SignOp(op, 7, mainHash, 0, bc.SigAlgoEd25519, stranger))
	if _, err := mainChain.ProposeSubchain(10, nil, [][]byte{forged}); err == nil || !strings.Contains(err.Error(), "bad registrar signature") {
		t.Fatalf("forged operation: %v", err)
	}
	otherParent := append([]byte(nil), mainHash...)
	otherParent[0] ^= 1
	replayed := must(bc.SignOp(op, 7, otherParent, 0, bc.SigAlgoEd25519, secret))
	if _, err := mainChain.ProposeSubchain(10, nil, [][]byte{replayed}); err == nil {
		t.Fatal("an operation signed for another parent must be refused")
	}
	digest := must(bc.OpDigest(op, 7, mainHash))
	if len(digest) != 32 {
		t.Fatal("digest")
	}
	signed := f.sign(op, 7, mainHash)
	if _, err := bc.SignOp(signed, 7, mainHash, 0, bc.SigAlgoEd25519, secret); err == nil {
		t.Fatal("a registrar approves once")
	}
	f.commit(mainChain, must(mainChain.ProposeSubchain(10, nil, [][]byte{signed})))
	f.commit(userChain, must(userChain.Propose(20)))
	f.commit(mainChain, must(mainChain.ProposeSubchain(11, [][]byte{must(bc.PendingAnchor(mainChain, userChain, 0))}, nil)))
	if a, err := bc.PendingAnchor(mainChain, userChain, 0); err != nil || a != nil {
		t.Fatal("nothing pending after anchoring")
	}

	mainSnapshot, userSnapshot := must(mainChain.Snapshot()), must(userChain.Snapshot())
	if bc.ChainSnapshotError(mainGenesis, mainSnapshot) != "" {
		t.Fatal("a good snapshot has no error")
	}
	mainBack, err := bc.RestoreChain(mainGenesis, 0, server.Public, server.Secret, mainSnapshot)
	if err != nil {
		t.Fatal(err)
	}
	defer mainBack.Free()
	userBack, err := bc.RestoreChain(userGenesis, 0, user.Public, user.Secret, userSnapshot)
	if err != nil {
		t.Fatal(err)
	}
	defer userBack.Free()
	if !bytes.Equal(must(mainBack.Snapshot()), mainSnapshot) || !bytes.Equal(must(mainBack.Tip()), must(mainChain.Tip())) {
		t.Fatal("restored chain differs")
	}
	for _, u := range []*bc.Chain{userChain, userBack} {
		f.commit(u, must(u.Propose(21)))
	}
	straight := f.commit(mainChain, must(mainChain.ProposeSubchain(12, [][]byte{must(bc.PendingAnchor(mainChain, userChain, 0))}, nil)))
	resumed := f.commit(mainBack, must(mainBack.ProposeSubchain(12, [][]byte{must(bc.PendingAnchor(mainBack, userBack, 0))}, nil)))
	if !bytes.Equal(straight, resumed) || !bytes.Equal(must(mainBack.Snapshot()), must(mainChain.Snapshot())) {
		t.Fatal("a restored chain must continue byte for byte")
	}
	if first, last, has := mainBack.ArchiveRange(); !has || first != 1 || last != 3 {
		t.Fatalf("archive range %d..%d", first, last)
	}
	if !bytes.Equal(must(mainBack.ArchivedBlock(3)), straight) || len(must(mainBack.ArchivedCertificate(3))) == 0 {
		t.Fatal("archived block")
	}
	if mainBack.PruneArchive(3) != 2 {
		t.Fatal("prune")
	}

	trailing := append(append([]byte(nil), mainSnapshot...), 0)
	if _, err := bc.RestoreChain(mainGenesis, 0, server.Public, server.Secret, trailing); err == nil {
		t.Fatal("trailing bytes must be refused")
	}
	if _, err := bc.RestoreChain(userGenesis, 0, server.Public, server.Secret, mainSnapshot); err == nil || !strings.Contains(err.Error(), "another genesis") {
		t.Fatalf("another genesis: %v", err)
	}
	record, err := mainBack.ChildRecord(7001)
	if err != nil || record.TipHeight != 2 {
		t.Fatalf("record: %+v %v", record, err)
	}
	root := must(bc.HeaderSubchainRoot(must(mainBack.Tip())))
	proof := must(mainBack.ChildProof(7001))
	if _, err := bc.VerifyChildProof(root, proof); err != nil {
		t.Fatal(err)
	}
	proof[len(proof)-1] ^= 1
	if _, err := bc.VerifyChildProof(root, proof); err == nil {
		t.Fatal("a tampered proof must be refused")
	}
	if _, err := mainBack.ChildRecord(9); err == nil {
		t.Fatal("unknown child")
	}
}

func TestChainTreeSnapshotRestore(t *testing.T) {
	must := ok(t)
	rootKey, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x03}, 32))
	childKey, _ := bc.NewBLSValidator(bytes.Repeat([]byte{0x13}, 32))
	pos, _ := bc.NewBlueprint("pos", "account", "round-robin", nil)
	bft, _ := bc.NewBlueprint("bft", "account", "", nil)
	spec := bc.ChainTreeNode{
		ChainID: 1, Blueprint: pos, AssuranceLevel: 1,
		Validators: []bc.ChainTreeValidator{{PublicKey: rootKey.Public, Secret: rootKey.Secret, Power: 1}},
		Children: []bc.ChainTreeNode{{
			ChainID: 42, Blueprint: bft, AssuranceLevel: 1,
			Validators: []bc.ChainTreeValidator{{PublicKey: childKey.Public, Secret: childKey.Secret, Power: 1}},
		}},
	}
	straight, err := bc.BuildChainTree(spec)
	if err != nil {
		t.Fatal(err)
	}
	defer straight.Free()
	first, err := bc.BuildChainTree(spec)
	if err != nil {
		t.Fatal(err)
	}
	round := func(tree *bc.ChainTree, ts uint64) []byte {
		return append(must(tree.Produce(42, ts)), must(tree.Produce(1, ts))...)
	}
	if !bytes.Equal(round(straight, 10), round(first, 10)) {
		t.Fatal("two trees from one spec agree")
	}
	snapshot := must(first.Snapshot())
	first.Free()
	second, err := bc.RestoreChainTree(spec, snapshot)
	if err != nil {
		t.Fatal(err)
	}
	defer second.Free()
	if !bytes.Equal(must(second.Snapshot()), snapshot) || !bytes.Equal(round(straight, 11), round(second, 11)) {
		t.Fatal("a restored tree must continue byte for byte")
	}
	if _, err := bc.RestoreChainTree(spec, append(snapshot, 1)); err == nil {
		t.Fatal("trailing bytes must be refused")
	}
}
