//go:build blockchainer_pkgconfig

package blockchainer

// pkg-config linking configuration, selected with `-tags blockchainer_pkgconfig`.
//
// Use this when libblockchainer_c is installed system-wide and a
// `blockchainer_c.pc` file is on your PKG_CONFIG_PATH. It resolves both the
// include path and the link flags from pkg-config, so no repo-relative paths are
// involved:
//
//	go build -tags blockchainer_pkgconfig ./...
//
// A ready-to-edit template ships next to this file as `blockchainer_c.pc.in`.

/*
#cgo pkg-config: blockchainer_c
*/
import "C"
