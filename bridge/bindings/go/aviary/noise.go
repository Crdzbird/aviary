package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import "errors"

// NoiseIdentity is an X25519 static pair and a hybrid signature pair.
type NoiseIdentity struct {
	StaticPublic, StaticSecret, SigPublic, SigSecret []byte
}

func (id NoiseIdentity) blob() []byte {
	return encodeBlobList([][]byte{id.StaticPublic, id.StaticSecret, id.SigPublic, id.SigSecret})
}

// GenerateNoiseIdentity creates a fresh identity.
func GenerateNoiseIdentity() (NoiseIdentity, error) {
	ensure()
	b, err := takeBuf(C.aviary_noise_identity_generate())
	if err != nil {
		return NoiseIdentity{}, err
	}
	p, err := splitBlobList(b)
	if err != nil || len(p) != 4 {
		return NoiseIdentity{}, errors.New("noise identity: malformed blob")
	}
	return NoiseIdentity{p[0], p[1], p[2], p[3]}, nil
}

// NoiseSession is a Noise_XX_25519_ChaChaPoly_SHA256 link with a post quantum
// identity proof bound to the handshake hash. Authentication is post quantum,
// confidentiality is X25519. Pin PeerIdentity against the keys you expect.
type NoiseSession struct{ h C.AviaryNoiseSession }

// InitiateNoiseSession starts the dialing side; HandshakeMessage is msg0.
func InitiateNoiseSession(me NoiseIdentity, prologue []byte) (*NoiseSession, error) {
	ensure()
	id := me.blob()
	h := C.aviary_noise_initiate(bp(id), C.size_t(len(id)), bp(prologue), C.size_t(len(prologue)))
	if h == nil {
		return nil, errors.New("noise: initiate refused (malformed identity)")
	}
	return &NoiseSession{h}, nil
}

// AcceptNoiseSession answers the initiator's msg0; HandshakeMessage is msg1.
func AcceptNoiseSession(me NoiseIdentity, msg0, prologue []byte) (*NoiseSession, error) {
	ensure()
	id := me.blob()
	h := C.aviary_noise_accept(bp(id), C.size_t(len(id)), bp(msg0), C.size_t(len(msg0)), bp(prologue), C.size_t(len(prologue)))
	if h == nil {
		return nil, errors.New("noise: accept refused (malformed message or identity)")
	}
	return &NoiseSession{h}, nil
}

// HandshakeMessage is the message this side produced when it was created.
func (s *NoiseSession) HandshakeMessage() ([]byte, error) {
	return takeBuf(C.aviary_noise_handshake_message(s.h))
}

// CompleteInitiator consumes msg1 and returns msg2.
func (s *NoiseSession) CompleteInitiator(msg1 []byte) ([]byte, error) {
	return takeBuf(C.aviary_noise_complete_initiator(s.h, bp(msg1), C.size_t(len(msg1))))
}

// CompleteResponder consumes msg2.
func (s *NoiseSession) CompleteResponder(msg2 []byte) error {
	if C.aviary_noise_complete_responder(s.h, bp(msg2), C.size_t(len(msg2))) != 0 {
		return errors.New("noise: handshake refused")
	}
	return nil
}

// IdentityProof is this side's proof, sent once after the handshake.
func (s *NoiseSession) IdentityProof() ([]byte, error) {
	return takeBuf(C.aviary_noise_identity_proof(s.h))
}

// VerifyIdentityProof checks the peer's proof.
func (s *NoiseSession) VerifyIdentityProof(proof []byte) error {
	if C.aviary_noise_verify_identity_proof(s.h, bp(proof), C.size_t(len(proof))) != 0 {
		return errors.New("noise: identity proof refused")
	}
	return nil
}

// Open reports whether the handshake is done and the peer's proof verified.
func (s *NoiseSession) Open() bool { return C.aviary_noise_open(s.h) == 1 }

// PeerIdentity is the peer's authenticated hybrid signature key.
func (s *NoiseSession) PeerIdentity() ([]byte, error) {
	return takeBuf(C.aviary_noise_peer_identity(s.h))
}

// PeerStatic is the peer's X25519 static key.
func (s *NoiseSession) PeerStatic() ([]byte, error) { return takeBuf(C.aviary_noise_peer_static(s.h)) }

// HandshakeHash is the final handshake hash, equal on both sides.
func (s *NoiseSession) HandshakeHash() ([]byte, error) {
	return takeBuf(C.aviary_noise_handshake_hash(s.h))
}

// Encrypt seals one record; it fails until Open.
func (s *NoiseSession) Encrypt(plaintext []byte) ([]byte, error) {
	return takeBuf(C.aviary_noise_encrypt(s.h, bp(plaintext), C.size_t(len(plaintext))))
}

// Decrypt opens one record; it fails on tamper, reorder or before Open.
func (s *NoiseSession) Decrypt(record []byte) ([]byte, error) {
	return takeBuf(C.aviary_noise_decrypt(s.h, bp(record), C.size_t(len(record))))
}

// Free releases the session.
func (s *NoiseSession) Free() { C.aviary_noise_free(s.h) }

// AdmissionChallenge is what a verifier keeps (Nonce) and sends (Wire).
type AdmissionChallenge struct{ Nonce, Wire []byte }

// NewAdmissionChallenge is a fresh challenge for epoch.
func NewAdmissionChallenge(epoch uint32) (AdmissionChallenge, error) {
	ensure()
	b, err := takeBuf(C.aviary_admission_challenge(C.uint32_t(epoch)))
	if err != nil {
		return AdmissionChallenge{}, err
	}
	if len(b) <= 32 {
		return AdmissionChallenge{}, errors.New("admission: malformed challenge")
	}
	return AdmissionChallenge{b[:32], b[32:]}, nil
}

// AdmissionAnswer is the peer's answer to a challenge wire from its credential.
func (p *CredentialPolicy) AdmissionAnswer(credential, wire []byte) ([]byte, error) {
	e := p.encoded
	return takeBuf(C.aviary_admission_answer(bp(e), C.size_t(len(e)), bp(credential), C.size_t(len(credential)),
		bp(wire), C.size_t(len(wire))))
}

// AdmissionGranted reports whether presentation, answering the challenge whose
// nonce this verifier issued, admits for epoch at now.
func (g *CredentialGate) AdmissionGranted(epoch uint32, nonce, presentation []byte, now uint64) bool {
	return C.aviary_admission_granted(g.h, C.uint32_t(epoch), bp(nonce), C.size_t(len(nonce)),
		bp(presentation), C.size_t(len(presentation)), C.uint64_t(now)) == 1
}
