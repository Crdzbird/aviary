//go:build aviary_pkgconfig

package aviary

// pkg-config linking configuration, selected with `-tags aviary_pkgconfig`.
//
// Use this when libaviary_c is installed system-wide and a
// `aviary_c.pc` file is on your PKG_CONFIG_PATH. It resolves both the
// include path and the link flags from pkg-config, so no repo-relative paths are
// involved:
//
//	go build -tags aviary_pkgconfig ./...
//
// A ready-to-edit template ships next to this file as `aviary_c.pc.in`.

/*
#cgo pkg-config: aviary_c
*/
import "C"
