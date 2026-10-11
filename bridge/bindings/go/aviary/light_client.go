package aviary

/*
#include <stdlib.h>
#include "aviary_c.h"
*/
import "C"

import "errors"

// LightClient follows a header chain with certificates only (no state, no
// bodies). Sequential Accept, plus the OVERLAP (skipping) rule: AcceptSkipping
// jumps to a far header when its own set certifies it and the signers hold
// more than the trust level (default 1/3) of the set the client already
// trusts; SyncTo bisects when the overlap is too thin.
type LightClient struct{ h C.AviaryLightClient }

// Checkpoint is a header, its certificate, and the set in force at its height.
type Checkpoint struct {
	Header, Certificate []byte
	Validators          []GenesisValidator
}

// Skip results.
const (
	SkipRefused             = 0
	SkipAccepted            = 1
	SkipInsufficientOverlap = 2
)

func encodeValidatorSet(vs []GenesisValidator) ([]byte, error) {
	items := make([][]byte, 0, len(vs))
	for _, v := range vs {
		if len(v.PublicKey) != 48 {
			return nil, errors.New("validators: key must be 48 bytes")
		}
		items = append(items, v.PublicKey)
	}
	// [count]([len][pk][power u64])x - the canonical ValidatorSet encoding.
	out := make([]byte, 4, 4+len(vs)*60)
	out[0], out[1], out[2], out[3] = byte(len(vs)>>24), byte(len(vs)>>16), byte(len(vs)>>8), byte(len(vs))
	for i, v := range vs {
		out = append(out, 0, 0, 0, 48)
		out = append(out, items[i]...)
		p := v.Power
		out = append(out, byte(p>>56), byte(p>>48), byte(p>>40), byte(p>>32), byte(p>>24), byte(p>>16), byte(p>>8), byte(p))
	}
	return out, nil
}

// NewLightClient trusts a header (e.g. the genesis header) and the validator
// set in force for the next header.
func NewLightClient(chainID uint64, validators []GenesisValidator, trustedHeader []byte) (*LightClient, error) {
	ensure()
	vs, err := encodeValidatorSet(validators)
	if err != nil {
		return nil, err
	}
	h := C.aviary_light_client_new(C.uint64_t(chainID), bp(vs), C.size_t(len(vs)), bp(trustedHeader), C.size_t(len(trustedHeader)))
	if h == nil {
		return nil, errors.New("light client: malformed validator set or header")
	}
	return &LightClient{h}, nil
}

func (l *LightClient) Free()                { C.aviary_light_client_free(l.h) }
func (l *LightClient) Height() uint64       { return uint64(C.aviary_light_client_height(l.h)) }
func (l *LightClient) Tip() ([]byte, error) { return takeBuf(C.aviary_light_client_tip(l.h)) }

// Validators is the set in force for the next header (follows rotations).
func (l *LightClient) Validators() ([]GenesisValidator, error) {
	blob, err := takeBuf(C.aviary_light_client_validators(l.h))
	if err != nil {
		return nil, err
	}
	return decodeValidatorSet(blob)
}

// SetTrustLevel sets the overlap threshold numerator/denominator (bad values ignored).
func (l *LightClient) SetTrustLevel(numerator, denominator uint32) {
	C.aviary_light_client_set_trust_level(l.h, C.uint32_t(numerator), C.uint32_t(denominator))
}

// SetMaxSkip bounds a single jump in blocks (0 = unbounded).
func (l *LightClient) SetMaxSkip(blocks uint64) {
	C.aviary_light_client_set_max_skip(l.h, C.uint64_t(blocks))
}

// Accept applies the sequential rule.
func (l *LightClient) Accept(header, certificate []byte) bool {
	return C.aviary_light_client_accept(l.h, bp(header), C.size_t(len(header)), bp(certificate), C.size_t(len(certificate))) == 1
}

// AcceptSkipping applies the overlap rule; returns SkipAccepted, SkipRefused
// or SkipInsufficientOverlap.
func (l *LightClient) AcceptSkipping(cp Checkpoint) int {
	vs, err := encodeValidatorSet(cp.Validators)
	if err != nil {
		return SkipRefused
	}
	return int(C.aviary_light_client_accept_skipping(l.h, bp(cp.Header), C.size_t(len(cp.Header)),
		bp(cp.Certificate), C.size_t(len(cp.Certificate)), bp(vs), C.size_t(len(vs))))
}

// SyncTo reaches target with bisection: on thin overlap it asks fetch for the
// midpoint checkpoint and retries; fails closed if a checkpoint is unavailable.
func (l *LightClient) SyncTo(target Checkpoint, targetHeight uint64, fetch func(height uint64) (Checkpoint, bool)) error {
	type pend struct {
		cp     Checkpoint
		height uint64
	}
	stack := []pend{{target, targetHeight}}
	for n := 0; len(stack) > 0; n++ {
		if n > 128 {
			return errors.New("light client: bisection exceeded its fetch budget")
		}
		top := stack[len(stack)-1]
		switch l.AcceptSkipping(top.cp) {
		case SkipAccepted:
			stack = stack[:len(stack)-1]
		case SkipInsufficientOverlap:
			lo, hi := l.Height(), top.height
			if hi-lo <= 1 {
				return errors.New("light client: insufficient overlap and no intermediate header")
			}
			mid := lo + (hi-lo)/2
			cp, ok := fetch(mid)
			if !ok {
				return errors.New("light client: checkpoint unavailable")
			}
			stack = append(stack, pend{cp, mid})
		default:
			return errors.New("light client: checkpoint refused")
		}
	}
	return nil
}
