//go:build !cgo

package aviary

// This file compiles only when cgo is DISABLED (CGO_ENABLED=0). Aviary's
// Go binding links the native libaviary_c library through cgo and cannot
// work without it, so the reference below deliberately fails the build with a
// descriptive identifier — far clearer than the wall of "undefined: C.xxx"
// errors you would otherwise get.
//
// Fix: build with cgo enabled (it is on by default unless cross-compiling):
//
//	CGO_ENABLED=1 go build ./...

func init() {
	_ = aviary_requires_cgo__rebuild_with_CGO_ENABLED_1
}
