package main

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strings"
	"sync"
	"time"

	pb "atlas/gen"
	bc "github.com/Crdzbird/Aviary/bridge/bindings/go/aviary"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

const genesisSupply = uint64(1_000_000_000_000) // treasury reserve minted at chain launch

// wallet is a custodial account: an Ed25519 key for the settlement chain and a
// secp256k1 key for EVM/Bitcoin interop.
type wallet struct {
	id, label, institution string
	edSecret               []byte // 64
	address                []byte // 32 (native chain)
	secpSecret             []byte // 32 (EVM/BTC)
	ethAddr, btcAddr, sol  string
	nextNonce              uint64
	treasury               bool
}

type chain struct {
	mu       sync.Mutex
	node     *bc.Node
	name     string
	symbol   string
	chainID  uint64
	wallets  map[string]*wallet
	order    []string
	treasury *wallet
	ts       uint64
	issued   uint64
	subs     map[chan *pb.Event]struct{}
}

type server struct {
	pb.UnimplementedAtlasServer
	mu       sync.Mutex
	chains   map[uint64]*chain
	councils map[uint64]*council // governance rail per settlement chain (council.go)
}

func newServer() *server { return &server{chains: map[uint64]*chain{}} }

func hexb(b []byte) string { return hex.EncodeToString(b) }

func (s *server) chain(id uint64) (*chain, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	c, ok := s.chains[id]
	if !ok {
		return nil, status.Errorf(codes.NotFound, "chain %d not found", id)
	}
	return c, nil
}

func (c *chain) newWallet(label, institution string, treasury bool) (*wallet, error) {
	secret, _, address, err := c.node.NewAccount()
	if err != nil {
		return nil, err
	}
	secp := make([]byte, 32)
	if _, err := rand.Read(secp); err != nil {
		return nil, err
	}
	secp[0] |= 1 // keep it a valid, non-zero scalar
	id := fmt.Sprintf("w%d", len(c.order)+1)
	if treasury {
		id = "treasury"
	}
	w := &wallet{
		id: id, label: label, institution: institution,
		edSecret: secret, address: address, secpSecret: secp,
		ethAddr: bc.EthAddress(secp), btcAddr: bc.BtcAddress(secp), sol: bc.SolAddress(secret),
		treasury: treasury,
	}
	c.wallets[id] = w
	c.order = append(c.order, id)
	return w, nil
}

func (c *chain) walletPB(w *wallet) *pb.Wallet {
	return &pb.Wallet{
		Id: w.id, Label: w.label, Institution: w.institution,
		Address: hexb(w.address), Balance: c.node.Balance(w.address), Nonce: c.node.Nonce(w.address),
		EthAddress: w.ethAddr, BtcAddress: w.btcAddr, SolAddress: w.sol,
	}
}

// ── RPCs ──────────────────────────────────────────────────────────────────────

func (s *server) CreateChain(_ context.Context, r *pb.CreateChainRequest) (*pb.Chain, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if _, ok := s.chains[r.ChainId]; ok {
		return nil, status.Errorf(codes.AlreadyExists, "chain %d exists", r.ChainId)
	}
	node := bc.NewNode(r.ChainId)
	if node == nil {
		return nil, status.Error(codes.Internal, "node creation failed")
	}
	c := &chain{node: node, name: r.Name, symbol: r.NativeSymbol, chainID: r.ChainId,
		wallets: map[string]*wallet{}, ts: 1, subs: map[chan *pb.Event]struct{}{}}
	// Mint the reserve to a treasury wallet at genesis (before the node starts).
	t, err := c.newWallet("Reserve Treasury", "Atlas Network", true)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	c.treasury = t
	if err := node.Credit(t.address, genesisSupply); err != nil {
		return nil, status.Errorf(codes.Internal, "genesis credit: %v", err)
	}
	s.chains[r.ChainId] = c
	go c.eventLoop()
	return c.chainPB(), nil
}

func (c *chain) chainPB() *pb.Chain {
	return &pb.Chain{Name: c.name, ChainId: c.chainID, NativeSymbol: c.symbol,
		Height: c.node.Height(), WalletCount: uint32(len(c.order) - 1), TotalIssued: c.issued}
}

func (s *server) ListChains(context.Context, *pb.Empty) (*pb.ChainList, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	out := &pb.ChainList{}
	for _, c := range s.chains {
		c.mu.Lock()
		out.Chains = append(out.Chains, c.chainPB())
		c.mu.Unlock()
	}
	return out, nil
}

func (s *server) CreateWallet(_ context.Context, r *pb.CreateWalletRequest) (*pb.Wallet, error) {
	c, err := s.chain(r.ChainId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	w, err := c.newWallet(r.Label, r.Institution, false)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	return c.walletPB(w), nil
}

func (s *server) ListWallets(_ context.Context, r *pb.ChainRef) (*pb.WalletList, error) {
	c, err := s.chain(r.ChainId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	out := &pb.WalletList{}
	for _, id := range c.order {
		if id == "treasury" {
			continue
		}
		out.Wallets = append(out.Wallets, c.walletPB(c.wallets[id]))
	}
	return out, nil
}

func (s *server) GetWallet(_ context.Context, r *pb.WalletRef) (*pb.Wallet, error) {
	c, w, err := s.lookup(r.ChainId, r.WalletId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.walletPB(w), nil
}

func (s *server) lookup(chainID uint64, walletID string) (*chain, *wallet, error) {
	c, err := s.chain(chainID)
	if err != nil {
		return nil, nil, err
	}
	w, ok := c.wallets[walletID]
	if !ok {
		return nil, nil, status.Errorf(codes.NotFound, "wallet %s not found", walletID)
	}
	return c, w, nil
}

// IssueFunds moves reserve from the treasury to a wallet (a real settlement tx,
// pending until the next block).
func (s *server) IssueFunds(_ context.Context, r *pb.IssueRequest) (*pb.Wallet, error) {
	c, w, err := s.lookup(r.ChainId, r.WalletId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	if _, err := c.node.SubmitTransfer(c.treasury.edSecret, w.address, r.Amount, c.treasury.nextNonce); err != nil {
		return nil, status.Error(codes.FailedPrecondition, err.Error())
	}
	c.treasury.nextNonce++
	c.issued += r.Amount
	return c.walletPB(w), nil
}

func (s *server) Settle(_ context.Context, r *pb.SettleRequest) (*pb.SettlementReceipt, error) {
	c, from, err := s.lookup(r.ChainId, r.FromWalletId)
	if err != nil {
		return nil, err
	}
	to, ok := c.wallets[r.ToWalletId]
	if !ok {
		return nil, status.Errorf(codes.NotFound, "wallet %s not found", r.ToWalletId)
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	txid, err := c.node.SubmitTransfer(from.edSecret, to.address, r.Amount, from.nextNonce)
	if err != nil {
		return nil, status.Error(codes.FailedPrecondition, err.Error())
	}
	from.nextNonce++
	return &pb.SettlementReceipt{Txid: hexb(txid), ChainId: c.chainID,
		FromAddress: hexb(from.address), ToAddress: hexb(to.address), Amount: r.Amount, Finalized: false}, nil
}

func (s *server) ProduceBlock(_ context.Context, r *pb.ChainRef) (*pb.Block, error) {
	c, err := s.chain(r.ChainId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	h, err := c.node.ProduceBlock(c.ts)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	c.ts++
	return &pb.Block{Height: h, StateRoot: hexb(c.node.StateRoot())}, nil
}

func (s *server) GetBalanceProof(_ context.Context, r *pb.WalletRef) (*pb.BalanceProof, error) {
	c, w, err := s.lookup(r.ChainId, r.WalletId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	proof := c.node.ProveAccount(w.address)
	root := c.node.StateRoot()
	return &pb.BalanceProof{Balance: c.node.Balance(w.address), StateRoot: hexb(root),
		Verified: bc.VerifyStateProof(root, proof), ProofBytes: uint32(len(proof))}, nil
}

func (s *server) BuildErc20Transfer(_ context.Context, r *pb.Erc20Request) (*pb.SignedEvmTx, error) {
	c, w, err := s.lookup(r.ChainId, r.WalletId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	token, err := decodeAddr(r.TokenAddress)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "token: "+err.Error())
	}
	to, err := decodeAddr(r.ToAddress)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, "to: "+err.Error())
	}
	calldata := bc.EthErc20TransferCalldata(to, r.Amount)
	raw, err := bc.EthSignLegacy(r.Nonce, r.GasPrice, r.GasLimit, token, 0, calldata, r.EvmChainId, w.secpSecret)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	return &pb.SignedEvmTx{RawTxHex: "0x" + hexb(raw), CalldataHex: "0x" + hexb(calldata),
		FromAddress: w.ethAddr}, nil
}

func (s *server) FunctionSelector(_ context.Context, r *pb.SelectorRequest) (*pb.SelectorReply, error) {
	return &pb.SelectorReply{SelectorHex: "0x" + hexb(bc.EthSelector(r.Signature))}, nil
}

func (s *server) GetMetrics(_ context.Context, r *pb.ChainRef) (*pb.Metrics, error) {
	c, err := s.chain(r.ChainId)
	if err != nil {
		return nil, err
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	m := parseMetrics(c.node.Metrics())
	return &pb.Metrics{Height: c.node.Height(), Blocks: m["aviary_blocks_finalized_total"],
		Transactions: m["aviary_transactions_total"], Mempool: uint64(c.node.MempoolSize())}, nil
}

func (s *server) StreamEvents(r *pb.ChainRef, stream pb.Atlas_StreamEventsServer) error {
	c, err := s.chain(r.ChainId)
	if err != nil {
		return err
	}
	ch := make(chan *pb.Event, 64)
	c.mu.Lock()
	c.subs[ch] = struct{}{}
	c.mu.Unlock()
	defer func() { c.mu.Lock(); delete(c.subs, ch); c.mu.Unlock() }()
	for {
		select {
		case <-stream.Context().Done():
			return nil
		case ev := <-ch:
			if err := stream.Send(ev); err != nil {
				return err
			}
		}
	}
}

// eventLoop drains the node's events and fans them out to stream subscribers.
func (c *chain) eventLoop() {
	t := time.NewTicker(250 * time.Millisecond)
	defer t.Stop()
	for range t.C {
		c.mu.Lock()
		raw := c.node.DrainEvents()
		subs := make([]chan *pb.Event, 0, len(c.subs))
		for ch := range c.subs {
			subs = append(subs, ch)
		}
		c.mu.Unlock()
		if raw == "[]" || raw == "" || len(subs) == 0 {
			continue
		}
		for _, ev := range parseEvents(raw, c.chainID) {
			for _, ch := range subs {
				select {
				case ch <- ev:
				default:
				}
			}
		}
	}
}

// ── helpers ───────────────────────────────────────────────────────────────────

func decodeAddr(s string) ([]byte, error) {
	b, err := hex.DecodeString(strings.TrimPrefix(s, "0x"))
	if err != nil {
		return nil, err
	}
	if len(b) != 20 {
		return nil, fmt.Errorf("address must be 20 bytes, got %d", len(b))
	}
	return b, nil
}

func parseMetrics(text string) map[string]uint64 {
	out := map[string]uint64{}
	for _, line := range strings.Split(text, "\n") {
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		var k string
		var v uint64
		if _, err := fmt.Sscanf(line, "%s %d", &k, &v); err == nil {
			out[k] = v
		}
	}
	return out
}

type rawEvent struct {
	Type        string `json:"type"`
	Txid        string `json:"txid"`
	MempoolSize uint64 `json:"mempool_size"`
	Height      uint64 `json:"height"`
	TxCount     uint64 `json:"tx_count"`
	StateRoot   string `json:"state_root"`
}

func parseEvents(rawJSON string, chainID uint64) []*pb.Event {
	var raws []rawEvent
	if json.Unmarshal([]byte(rawJSON), &raws) != nil {
		return nil
	}
	now := time.Now().UnixMilli()
	var out []*pb.Event
	for _, r := range raws {
		t := pb.EventType_EVENT_UNSPECIFIED
		switch r.Type {
		case "tx_admitted":
			t = pb.EventType_TX_ADMITTED
		case "block_finalized":
			t = pb.EventType_BLOCK_FINALIZED
		case "tx_removed":
			t = pb.EventType_TX_REMOVED
		}
		out = append(out, &pb.Event{Type: t, ChainId: chainID, Height: r.Height, TxCount: r.TxCount,
			Txid: r.Txid, MempoolSize: r.MempoolSize, StateRoot: r.StateRoot, UnixMillis: now})
	}
	return out
}
