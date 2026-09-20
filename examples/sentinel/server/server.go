package main

import (
	"context"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"io"
	"sort"
	"strings"
	"sync"
	"time"

	bc "github.com/Crdzbird/Blockchainer/bridge/bindings/go/blockchainer"
	pb "sentinel/gen"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

// anchorCredit is the device's on-chain balance, spent 1 unit per anchored record.
const anchorCredit = uint64(1) << 40

// device is one enrolled device's dedicated provenance chain.
type device struct {
	mu          sync.Mutex
	node        *bc.Node
	chainID     uint64
	fingerprint []byte // 32 bytes, BLAKE3(device info ‖ credential)
	address     []byte // on-chain identity (32 bytes)
	secret      []byte // custodial signing key for anchoring txs
	label       string
	nextNonce   uint64
	ts          uint64 // monotonic block timestamp
	records     uint64 // anchored data records (excludes the genesis fingerprint)
	kindCounts  map[string]uint64
	pending     []*pb.AnchorReceipt // anchored, awaiting the next sealed block
}

func (d *device) nextTs() uint64 { d.ts++; return d.ts }

type server struct {
	pb.UnimplementedSentinelServer
	mu      sync.Mutex
	devices map[uint64]*device
	fleet   *fleet // the chain tree (fleet.go)
}

func newServer() *server { return &server{devices: map[uint64]*device{}} }

func (s *server) lookup(chainID uint64) *device {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.devices[chainID]
}

// ProvisionDevice binds a chain to a device: the device info + credential form a
// fingerprint that (a) deterministically derives the chain id and (b) is anchored
// as the origin block — the device's on-chain birth certificate.
func (s *server) ProvisionDevice(_ context.Context, in *pb.DeviceInfo) (*pb.Provenance, error) {
	if in.GetDeviceId() == "" {
		return nil, status.Error(codes.InvalidArgument, "device_id required")
	}
	fp, err := bc.Hash(fingerprintInput(in))
	if err != nil {
		return nil, status.Errorf(codes.Internal, "fingerprint: %v", err)
	}
	chainID := binary.BigEndian.Uint64(fp[:8])
	if chainID == 0 {
		chainID = 1
	}

	s.mu.Lock()
	if d, ok := s.devices[chainID]; ok { // idempotent re-enrollment
		s.mu.Unlock()
		return d.provenance(), nil
	}
	node := bc.NewNode(chainID)
	secret, _, address, err := node.NewAccount()
	if err != nil {
		s.mu.Unlock()
		return nil, status.Errorf(codes.Internal, "account: %v", err)
	}
	if err := node.Credit(address, anchorCredit); err != nil {
		s.mu.Unlock()
		return nil, status.Errorf(codes.Internal, "genesis credit: %v", err)
	}
	d := &device{
		node: node, chainID: chainID, fingerprint: fp, address: address,
		secret: secret, label: in.GetLabel(), kindCounts: map[string]uint64{},
	}
	s.devices[chainID] = d
	s.mu.Unlock()

	// Seal the origin block, anchoring the device fingerprint.
	d.mu.Lock()
	defer d.mu.Unlock()
	if _, err := node.SubmitTransfer(secret, fp, 1, d.nextNonce); err != nil {
		return nil, status.Errorf(codes.Internal, "anchor genesis: %v", err)
	}
	d.nextNonce++
	if _, err := node.ProduceBlock(d.nextTs()); err != nil {
		return nil, status.Errorf(codes.Internal, "genesis block: %v", err)
	}
	return d.provenance(), nil
}

func (d *device) provenance() *pb.Provenance {
	return &pb.Provenance{
		ChainId:           d.chainID,
		DeviceFingerprint: hex.EncodeToString(d.fingerprint),
		GenesisHash:       hex.EncodeToString(d.node.StateRoot()),
		DeviceAddress:     hex.EncodeToString(d.address),
		GenesisHeight:     d.node.Height(),
	}
}

// StreamData is the live capture pipeline. The device streams records; each is
// hashed and anchored into the mempool with an immediate "pending" receipt, and a
// background sealer produces a block on a cadence and emits "finalized" receipts.
func (s *server) StreamData(stream pb.Sentinel_StreamDataServer) error {
	sendCh := make(chan *pb.AnchorReceipt, 128)
	senderDone := make(chan struct{})
	go func() {
		for r := range sendCh {
			if err := stream.Send(r); err != nil {
				break
			}
		}
		close(senderDone)
	}()

	var dev *device
	stopSealer := make(chan struct{})
	sealerDone := make(chan struct{})
	go func() {
		defer close(sealerDone)
		t := time.NewTicker(1500 * time.Millisecond)
		defer t.Stop()
		for {
			select {
			case <-stopSealer:
				return
			case <-t.C:
				if dev == nil {
					continue
				}
				dev.mu.Lock()
				if dev.node.MempoolSize() == 0 {
					dev.mu.Unlock()
					continue
				}
				h, err := dev.node.ProduceBlock(dev.nextTs())
				sealed := dev.pending
				dev.pending = nil
				dev.mu.Unlock()
				if err != nil {
					continue
				}
				for _, rc := range sealed {
					rc.Finalized = true
					rc.Height = h
					sendCh <- rc
				}
			}
		}
	}()

	var recvErr error
	for {
		rec, err := stream.Recv()
		if err == io.EOF {
			break
		}
		if err != nil {
			recvErr = err
			break
		}
		d := s.lookup(rec.GetChainId())
		if d == nil {
			sendCh <- &pb.AnchorReceipt{Summary: "unknown device — provision first"}
			continue
		}
		dev = d

		content, err := contentHash(rec)
		if err != nil {
			sendCh <- &pb.AnchorReceipt{Summary: "hash error: " + err.Error()}
			continue
		}
		d.mu.Lock()
		txid, err := d.node.SubmitTransfer(d.secret, content, 1, d.nextNonce)
		if err != nil {
			d.mu.Unlock()
			sendCh <- &pb.AnchorReceipt{ContentHash: hex.EncodeToString(content), Summary: "anchor rejected: " + err.Error()}
			continue
		}
		d.nextNonce++
		seq := d.records
		d.records++
		d.kindCounts[rec.GetKind().String()]++
		rc := &pb.AnchorReceipt{
			Sequence:           seq,
			Kind:               rec.GetKind(),
			ContentHash:        hex.EncodeToString(content),
			Txid:               hex.EncodeToString(txid),
			Finalized:          false,
			Summary:            summarize(rec),
			AnchoredUnixMillis: rec.GetCapturedUnixMillis(),
		}
		d.pending = append(d.pending, rc)
		d.mu.Unlock()

		// Send a distinct "pending" copy now; rc itself flips to finalized later.
		pendingCopy := *rc
		sendCh <- &pendingCopy
	}

	close(stopSealer)
	<-sealerDone
	close(sendCh)
	<-senderDone
	if recvErr != nil && recvErr != io.EOF {
		return recvErr
	}
	return nil
}

// VerifyRecord proves a content hash is committed in the device's chain.
func (s *server) VerifyRecord(_ context.Context, in *pb.VerifyRequest) (*pb.VerifyReply, error) {
	d := s.lookup(in.GetChainId())
	if d == nil {
		return nil, status.Error(codes.NotFound, "unknown device chain")
	}
	hash, err := hex.DecodeString(strings.TrimPrefix(in.GetContentHash(), "0x"))
	if err != nil || len(hash) != 32 {
		return nil, status.Error(codes.InvalidArgument, "content_hash must be 32-byte hex")
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	balance := d.node.Balance(hash)
	root := d.node.StateRoot()
	proof := d.node.ProveAccount(hash)
	anchored := balance > 0
	verified := anchored && bc.VerifyStateProof(root, proof)
	return &pb.VerifyReply{
		Anchored:   anchored,
		Verified:   verified,
		Height:     d.node.Height(),
		StateRoot:  hex.EncodeToString(root),
		ProofBytes: uint32(len(proof)),
	}, nil
}

func (s *server) GetLedger(_ context.Context, in *pb.DeviceRef) (*pb.LedgerStatus, error) {
	d := s.lookup(in.GetChainId())
	if d == nil {
		return nil, status.Error(codes.NotFound, "unknown device chain")
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	counts := make(map[string]uint64, len(d.kindCounts))
	for k, v := range d.kindCounts {
		counts[k] = v
	}
	return &pb.LedgerStatus{
		ChainId:           d.chainID,
		DeviceAddress:     hex.EncodeToString(d.address),
		DeviceFingerprint: hex.EncodeToString(d.fingerprint),
		Height:            d.node.Height(),
		Records:           d.records,
		Mempool:           d.node.MempoolSize(),
		StateRoot:         hex.EncodeToString(d.node.StateRoot()),
		KindCounts:        counts,
	}, nil
}

// --- hashing & formatting -------------------------------------------------

func fingerprintInput(in *pb.DeviceInfo) []byte {
	var b strings.Builder
	fmt.Fprintf(&b, "sentinel-device\x00%s\x00%s\x00%s\x00%s\x00%s\x00",
		in.GetDeviceId(), in.GetModel(), in.GetPlatform(), in.GetOsVersion(), in.GetLabel())
	out := append([]byte(b.String()), in.GetCredential()...)
	return out
}

// contentHash is the anchored identifier. For media it is the pure BLAKE3 of the
// bytes (so a verifier can independently re-hash the file); for location/sensor
// records it is the hash of a canonical, deterministic encoding of the metadata.
func contentHash(rec *pb.DataRecord) ([]byte, error) {
	switch rec.GetKind() {
	case pb.DataKind_PHOTO, pb.DataKind_VIDEO:
		if len(rec.GetPayload()) > 0 {
			return bc.Hash(rec.GetPayload())
		}
	}
	return bc.Hash(canonicalMeta(rec))
}

func canonicalMeta(rec *pb.DataRecord) []byte {
	var b strings.Builder
	fmt.Fprintf(&b, "k=%s;t=%d;mime=%s;note=%s;", rec.GetKind().String(),
		rec.GetCapturedUnixMillis(), rec.GetMime(), rec.GetNote())
	switch rec.GetKind() {
	case pb.DataKind_LOCATION:
		fmt.Fprintf(&b, "lat=%.6f;lon=%.6f;acc=%.2f;", rec.GetLat(), rec.GetLon(), rec.GetAccuracyM())
	case pb.DataKind_SENSOR:
		keys := make([]string, 0, len(rec.GetSensors()))
		for k := range rec.GetSensors() {
			keys = append(keys, k)
		}
		sort.Strings(keys) // deterministic: map order is not canonical
		for _, k := range keys {
			fmt.Fprintf(&b, "%s=%.6f;", k, rec.GetSensors()[k])
		}
	}
	return []byte(b.String())
}

func summarize(rec *pb.DataRecord) string {
	switch rec.GetKind() {
	case pb.DataKind_PHOTO:
		return fmt.Sprintf("Photo %s, %s", humanBytes(len(rec.GetPayload())), orDash(rec.GetMime()))
	case pb.DataKind_VIDEO:
		return fmt.Sprintf("Video chunk %s, %s", humanBytes(len(rec.GetPayload())), orDash(rec.GetMime()))
	case pb.DataKind_LOCATION:
		return fmt.Sprintf("GPS %.5f, %.5f (±%.0fm)", rec.GetLat(), rec.GetLon(), rec.GetAccuracyM())
	case pb.DataKind_SENSOR:
		keys := make([]string, 0, len(rec.GetSensors()))
		for k := range rec.GetSensors() {
			keys = append(keys, k)
		}
		sort.Strings(keys)
		parts := make([]string, 0, len(keys))
		for _, k := range keys {
			parts = append(parts, fmt.Sprintf("%s=%.2f", k, rec.GetSensors()[k]))
		}
		return "Sensors " + strings.Join(parts, " ")
	default:
		return "Record"
	}
}

func humanBytes(n int) string {
	if n < 1024 {
		return fmt.Sprintf("%d B", n)
	}
	return fmt.Sprintf("%.1f KB", float64(n)/1024)
}

func orDash(s string) string {
	if s == "" {
		return "-"
	}
	return s
}
