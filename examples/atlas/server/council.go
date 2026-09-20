package main

import (
	"context"
	"crypto/rand"
	"fmt"
	"sync"

	pb "atlas/gen"
	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// council is the settlement rail's validator council: a BFT chain (the
// composable-chain handle) whose validator set the operator rotates by
// announcing the next set, certified by the current one. An "auditor" light
// client that only ever trusted the genesis set follows with the OVERLAP
// (skipping) rule — bisecting through every rotation header.
type council struct {
	mu          sync.Mutex
	chainID     uint64
	chain       *bc.Chain
	operator    *bc.PortableAccount
	genesisSet  []bc.GenesisValidator
	genesisHdr  []byte
	checkpoints map[uint64]bc.Checkpoint // height → (header, cert, set in force at that height)
	ts          uint64
	rotations   uint32
	auditorH    uint64
	log         []*pb.CouncilStep
}

func (s *server) council(chainID uint64) (*council, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.councils == nil {
		s.councils = map[uint64]*council{}
	}
	if c, ok := s.councils[chainID]; ok {
		return c, nil
	}
	ikm := make([]byte, 32)
	if _, err := rand.Read(ikm); err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	op, err := bc.NewBLSValidator(ikm)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	set := []bc.GenesisValidator{{PublicKey: op.Public, Power: 1}}
	bft, _ := bc.NewBlueprint("bft", "account", "", nil)
	gen, err := bc.BuildGenesis(chainID+1000, 1, 1, bc.StateModelAccount, nil, set)
	if err == nil {
		gen, err = bc.CommitBlueprint(gen, bft)
	}
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	hdr, _ := bc.GenesisHeader(gen)
	chain, err := bc.AssembleChain(gen, 0, op.Public, op.Secret)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	c := &council{chainID: chainID, chain: chain, operator: op, genesisSet: set, genesisHdr: hdr,
		checkpoints: map[uint64]bc.Checkpoint{}, ts: 1}
	c.step("council formed", true, "1 validator (the operator); genesis set is what the auditor trusts")
	s.councils[chainID] = c
	return c, nil
}

func (c *council) step(label string, ok bool, detail string) {
	c.log = append(c.log, &pb.CouncilStep{Label: label, Ok: ok, Detail: detail})
	if len(c.log) > 40 {
		c.log = c.log[len(c.log)-40:]
	}
}

// produce finalizes the next council block (the operator always keeps quorum
// alone: it holds ≥ 3/4 of the power after every rotation) and records the
// checkpoint the auditor will need.
func (c *council) produce() error {
	inForce, err := c.chain.Validators()
	if err != nil {
		return err
	}
	c.ts++
	blk, err := c.chain.Propose(c.ts)
	if err != nil {
		return err
	}
	cert, err := c.chain.Certify(blk)
	if err != nil {
		return err
	}
	if !c.chain.Submit(blk, cert) {
		return fmt.Errorf("council block refused")
	}
	hdr, _ := bc.BlockHeader(blk)
	c.checkpoints[c.chain.Height()] = bc.Checkpoint{Header: hdr, Certificate: cert, Validators: inForce}
	return nil
}

func (c *council) pb() *pb.Council {
	vs, _ := c.chain.Validators()
	out := &pb.Council{ChainId: c.chainID, Height: c.chain.Height(), AuditorHeight: c.auditorH, Rotations: c.rotations, Log: c.log}
	for _, v := range vs {
		out.Validators = append(out.Validators, &pb.CouncilValidator{PublicKey: hexb(v.PublicKey), Power: v.Power,
			Operator: string(v.PublicKey) == string(c.operator.Public)})
	}
	return out
}

func (s *server) GetCouncil(_ context.Context, r *pb.ChainRef) (*pb.Council, error) {
	if _, err := s.chain(r.ChainId); err != nil {
		return nil, err
	}
	c, err := s.council(r.ChainId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.pb(), nil
}

func (s *server) CouncilAction(_ context.Context, r *pb.CouncilRequest) (*pb.Council, error) {
	if _, err := s.chain(r.ChainId); err != nil {
		return nil, err
	}
	c, err := s.council(r.ChainId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	switch r.Op {
	case pb.CouncilOp_COUNCIL_PRODUCE:
		vs, _ := c.chain.Validators()
		if err := c.produce(); err != nil {
			c.step("produce block", false, err.Error())
		} else {
			c.step(fmt.Sprintf("block %d finalized", c.chain.Height()), true, fmt.Sprintf("certified under the %d-validator set in force", len(vs)))
		}
	case pb.CouncilOp_COUNCIL_ADD_VALIDATOR:
		vs, _ := c.chain.Validators()
		ikm := make([]byte, 32)
		rand.Read(ikm)
		nv, err := bc.NewBLSValidator(ikm)
		if err != nil {
			return nil, status.Error(codes.Internal, err.Error())
		}
		// Next set: every existing member keeps power 1 except the operator, who
		// holds 3× the others so it still reaches quorum alone (a demo convenience —
		// on a real council the other validators would certify too).
		next := []bc.GenesisValidator{}
		others := 0
		for _, v := range vs {
			if string(v.PublicKey) != string(c.operator.Public) {
				next = append(next, bc.GenesisValidator{PublicKey: v.PublicKey, Power: 1})
				others++
			}
		}
		next = append(next, bc.GenesisValidator{PublicKey: nv.Public, Power: 1})
		others++
		next = append([]bc.GenesisValidator{{PublicKey: c.operator.Public, Power: uint64(3 * others)}}, next...)
		if err := c.chain.ScheduleValidators(next); err != nil {
			c.step("schedule rotation", false, err.Error())
			break
		}
		if err := c.produce(); err != nil {
			c.step("rotation block", false, err.Error())
			break
		}
		c.rotations++
		c.step(fmt.Sprintf("rotation #%d announced in block %d", c.rotations, c.chain.Height()), true,
			fmt.Sprintf("certified by the OLD %d-validator set; the new %d-validator set rules from the next block (%s… joined)", len(vs), len(next), hexb(nv.Public)[:12]))
	case pb.CouncilOp_COUNCIL_AUDITOR_SYNC:
		target := c.chain.Height()
		if target == 0 {
			c.step("auditor sync", false, "nothing to sync yet — produce a block first")
			break
		}
		lc, err := bc.NewLightClient(c.chainID+1000, c.genesisSet, c.genesisHdr)
		if err != nil {
			return nil, status.Error(codes.Internal, err.Error())
		}
		defer lc.Free()
		tcp := c.checkpoints[target]
		switch lc.AcceptSkipping(tcp) {
		case bc.SkipAccepted:
			c.step(fmt.Sprintf("auditor skipped genesis → %d in one jump", target), true,
				"the target's certificate is signed by > 1/3 of the set the auditor trusts")
		case bc.SkipInsufficientOverlap:
			c.step(fmt.Sprintf("auditor cannot jump genesis → %d", target), true,
				"thin overlap with the genesis set (rotations in between) → bisecting")
			fetched := []uint64{}
			if err := lc.SyncTo(tcp, target, func(h uint64) (bc.Checkpoint, bool) {
				fetched = append(fetched, h)
				cp, ok := c.checkpoints[h]
				return cp, ok
			}); err != nil {
				c.step("auditor bisection", false, err.Error())
			} else {
				c.step(fmt.Sprintf("auditor bisected to %d", target), true, fmt.Sprintf("intermediate headers fetched: %v — each accepted rotation refreshed the trusted set", fetched))
			}
		default:
			c.step("auditor sync", false, "checkpoint refused (hard failure)")
		}
		c.auditorH = lc.Height()
		vs, _ := lc.Validators()
		c.step(fmt.Sprintf("auditor now trusts height %d", c.auditorH), c.auditorH == target, fmt.Sprintf("and a %d-validator set — without replaying every header", len(vs)))
	}
	return c.pb(), nil
}
