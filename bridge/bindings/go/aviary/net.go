package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import (
	"encoding/binary"
	"errors"
)

// Observer is the validator index of a node that follows a chain without voting.
const Observer uint32 = 0xFFFFFFFF

// Consensus round steps a timeout belongs to.
const (
	StepPropose   = 0
	StepPrevote   = 1
	StepPrecommit = 2
)

// Relay is a gossip message to send to one peer.
type Relay struct {
	To      uint32
	Message []byte
}

// TimeoutRequest is a consensus timer to arm; when it fires, call OnTimeout.
type TimeoutRequest struct {
	Step  uint8
	Round uint32
}

// Outbound is what a NetNode wants done after an input: relays to send and
// timers to arm. The duration of a timer is the caller's choice.
type Outbound struct {
	Relays   []Relay
	Timeouts []TimeoutRequest
}

// NetNode is a validator or observer running BFT consensus over a gossip mesh
// with no socket and no clock of its own. Every input returns an Outbound; the
// application sends the relays over its own transport, arms the timeouts, and
// feeds what arrives back through OnGossip and OnTimeout.
type NetNode struct{ h C.AviaryNetNode }

// NewNetNode creates validator me of genesis (or an observer when me is
// Observer; bls keys may then be nil).
func NewNetNode(genesis []byte, me uint32, blsPublic, blsSecret []byte, timestamp uint64) (*NetNode, error) {
	ensure()
	h := C.aviary_net_node_new(bp(genesis), C.size_t(len(genesis)), C.uint32_t(me),
		bp(blsPublic), C.size_t(len(blsPublic)), bp(blsSecret), C.size_t(len(blsSecret)), C.uint64_t(timestamp))
	if h == nil {
		return nil, errors.New("net node: malformed genesis or key")
	}
	return &NetNode{h}, nil
}

func (n *NetNode) Free() { C.aviary_net_node_free(n.h) }

// AddPeer adds a mesh neighbour.
func (n *NetNode) AddPeer(peer uint32) { C.aviary_net_node_add_peer(n.h, C.uint32_t(peer)) }

// SetTransactions sets the encoded transactions this node proposes at the
// current height; cleared by Advance.
func (n *NetNode) SetTransactions(encodedTxs [][]byte) error {
	blob := encodeBlobList(encodedTxs)
	if C.aviary_net_node_set_transactions(n.h, bp(blob), C.size_t(len(blob))) != 1 {
		return errors.New("net node: malformed transaction")
	}
	return nil
}

func decodeOutbound(b []byte) (Outbound, error) {
	var o Outbound
	if len(b) < 4 {
		return o, errors.New("outbound: truncated")
	}
	off := 0
	relays := int(binary.BigEndian.Uint32(b[off:]))
	off += 4
	for i := 0; i < relays; i++ {
		if len(b) < off+8 {
			return o, errors.New("outbound: truncated relay")
		}
		to := binary.BigEndian.Uint32(b[off:])
		l := int(binary.BigEndian.Uint32(b[off+4:]))
		off += 8
		if len(b) < off+l {
			return o, errors.New("outbound: truncated relay body")
		}
		o.Relays = append(o.Relays, Relay{To: to, Message: append([]byte(nil), b[off:off+l]...)})
		off += l
	}
	if len(b) < off+4 {
		return o, errors.New("outbound: truncated timeouts")
	}
	timeouts := int(binary.BigEndian.Uint32(b[off:]))
	off += 4
	for i := 0; i < timeouts; i++ {
		if len(b) < off+5 {
			return o, errors.New("outbound: truncated timeout")
		}
		o.Timeouts = append(o.Timeouts, TimeoutRequest{Step: b[off], Round: binary.BigEndian.Uint32(b[off+1:])})
		off += 5
	}
	return o, nil
}

func (n *NetNode) outbound(res C.AviaryBufferResult) (Outbound, error) {
	b, err := takeBuf(res)
	if err != nil {
		return Outbound{}, err
	}
	return decodeOutbound(b)
}

// Start begins consensus for the current height.
func (n *NetNode) Start() (Outbound, error) { return n.outbound(C.aviary_net_node_start(n.h)) }

// OnGossip handles a payload received from peer from.
func (n *NetNode) OnGossip(from uint32, payload []byte) (Outbound, error) {
	return n.outbound(C.aviary_net_node_on_gossip(n.h, C.uint32_t(from), bp(payload), C.size_t(len(payload))))
}

// OnTimeout fires a timeout this node asked for.
func (n *NetNode) OnTimeout(t TimeoutRequest) (Outbound, error) {
	return n.outbound(C.aviary_net_node_on_timeout(n.h, C.uint8_t(t.Step), C.uint32_t(t.Round)))
}

// Advance rolls into the next height after Committed.
func (n *NetNode) Advance() (Outbound, error) { return n.outbound(C.aviary_net_node_advance(n.h)) }

func (n *NetNode) Committed() bool { return C.aviary_net_node_committed(n.h) == 1 }
func (n *NetNode) Height() uint64  { return uint64(C.aviary_net_node_height(n.h)) }
func (n *NetNode) PeerBanned(peer uint32) bool {
	return C.aviary_net_node_peer_banned(n.h, C.uint32_t(peer)) == 1
}
func (n *NetNode) Tip() ([]byte, error)       { return takeBuf(C.aviary_net_node_tip(n.h)) }
func (n *NetNode) StateRoot() ([]byte, error) { return takeBuf(C.aviary_net_node_state_root(n.h)) }

// ReplayLog is every unique consensus payload of the current height, in
// arrival order; replay it to a peer whose link connects late.
func (n *NetNode) ReplayLog() ([][]byte, error) {
	b, err := takeBuf(C.aviary_net_node_replay_log(n.h))
	if err != nil {
		return nil, err
	}
	return splitBlobList(b)
}

func (n *NetNode) Balance(address []byte) uint64 {
	if len(address) != 32 {
		return 0
	}
	return uint64(C.aviary_net_node_balance(n.h, bp(address)))
}

func (n *NetNode) ProveAccount(address []byte) ([]byte, error) {
	if len(address) != 32 {
		return nil, errors.New("net node: address must be 32 bytes")
	}
	return takeBuf(C.aviary_net_node_prove_account(n.h, bp(address)))
}

// SecureIdentity is a long term peer identity: a hybrid KEM pair to receive
// handshakes and a hybrid signature pair to authenticate.
type SecureIdentity struct {
	KemPublic, KemSecret, SigPublic, SigSecret []byte
}

// GenerateSecureIdentity creates a fresh identity.
func GenerateSecureIdentity() (SecureIdentity, error) {
	ensure()
	b, err := takeBuf(C.aviary_secure_identity_generate())
	if err != nil {
		return SecureIdentity{}, err
	}
	parts, err := splitBlobList(b)
	if err != nil || len(parts) != 4 {
		return SecureIdentity{}, errors.New("secure identity: malformed blob")
	}
	return SecureIdentity{parts[0], parts[1], parts[2], parts[3]}, nil
}

// SecureSession is a confidential, mutually authenticated, post quantum hybrid
// link between two peers. Check PeerIdentity against the peer you expected.
type SecureSession struct{ h C.AviarySecureSession }

// InitiateSecureSession is the dialing side: it encapsulates to the responder's
// KEM public key and authenticates with the given signature key pair. Send
// HandshakeMessage and feed the reply to Complete.
func InitiateSecureSession(responderKemPublic, sigPublic, sigSecret []byte) (*SecureSession, error) {
	ensure()
	h := C.aviary_secure_initiate(bp(responderKemPublic), C.size_t(len(responderKemPublic)),
		bp(sigPublic), C.size_t(len(sigPublic)), bp(sigSecret), C.size_t(len(sigSecret)))
	if h == nil {
		return nil, errors.New("secure session: initiate failed")
	}
	return &SecureSession{h}, nil
}

// AcceptSecureSession is the accepting side: it consumes the initiator's first
// message under me and is open at once; send HandshakeMessage back.
func AcceptSecureSession(me SecureIdentity, firstMessage []byte) (*SecureSession, error) {
	ensure()
	h := C.aviary_secure_accept(bp(me.KemPublic), C.size_t(len(me.KemPublic)), bp(me.KemSecret), C.size_t(len(me.KemSecret)),
		bp(me.SigPublic), C.size_t(len(me.SigPublic)), bp(me.SigSecret), C.size_t(len(me.SigSecret)),
		bp(firstMessage), C.size_t(len(firstMessage)))
	if h == nil {
		return nil, errors.New("secure session: initiator refused")
	}
	return &SecureSession{h}, nil
}

func (s *SecureSession) Free() { C.aviary_secure_free(s.h) }

// HandshakeMessage is what this side must send: the first message for an
// initiator, the reply for a responder.
func (s *SecureSession) HandshakeMessage() ([]byte, error) {
	return takeBuf(C.aviary_secure_handshake_message(s.h))
}

// Complete consumes the responder's reply on the initiator side.
func (s *SecureSession) Complete(reply []byte) error {
	if C.aviary_secure_complete(s.h, bp(reply), C.size_t(len(reply))) != 1 {
		return errors.New("secure session: responder refused")
	}
	return nil
}

func (s *SecureSession) Open() bool { return C.aviary_secure_open(s.h) == 1 }

// PeerIdentity is the authenticated remote signing public key; empty until open.
func (s *SecureSession) PeerIdentity() ([]byte, error) {
	return takeBuf(C.aviary_secure_peer_identity(s.h))
}

func (s *SecureSession) Encrypt(plaintext []byte) ([]byte, error) {
	return takeBuf(C.aviary_secure_encrypt(s.h, bp(plaintext), C.size_t(len(plaintext))))
}

func (s *SecureSession) Decrypt(frame []byte) ([]byte, error) {
	return takeBuf(C.aviary_secure_decrypt(s.h, bp(frame), C.size_t(len(frame))))
}

// splitBlobList decodes [count u32]([len u32][bytes])*.
func splitBlobList(b []byte) ([][]byte, error) {
	if len(b) < 4 {
		return nil, errors.New("blob list: truncated")
	}
	n := int(binary.BigEndian.Uint32(b))
	off := 4
	out := make([][]byte, 0, n)
	for i := 0; i < n; i++ {
		if len(b) < off+4 {
			return nil, errors.New("blob list: truncated length")
		}
		l := int(binary.BigEndian.Uint32(b[off:]))
		off += 4
		if len(b) < off+l {
			return nil, errors.New("blob list: truncated item")
		}
		out = append(out, append([]byte(nil), b[off:off+l]...))
		off += l
	}
	return out, nil
}
