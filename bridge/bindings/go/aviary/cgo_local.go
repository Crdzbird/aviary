//go:build !aviary_pkgconfig && !aviary_static

package aviary

// Default linking configuration (in-repository layout).
//
// The C header lives in `bridge/aviary_c.h` and the compiled shared
// library in `build/` (relative to the repo root). aviary_c links
// cryptolib's C ABI; cryptolib's own Go package (depended on in go.mod) provides
// libcryptolib_c, so this file links ONLY -laviary_c.
//
// Consuming from OUTSIDE the repo (via `go get`)? The header resolves from the
// module cache, but the prebuilt library does not ship in the module — point the
// linker at your build with environment variables (appended to the flags below):
//
//	export CGO_LDFLAGS="-L/opt/aviary/lib"
//	export CGO_CFLAGS="-I/opt/aviary/include"
//	go build ./...
//
// At run time the dynamic loader must also find the shared objects:
//
//	export DYLD_LIBRARY_PATH=/opt/aviary/lib   # macOS
//	export LD_LIBRARY_PATH=/opt/aviary/lib     # Linux
//
// Or install system-wide with a pkg-config file and build with
// `-tags aviary_pkgconfig` (see cgo_pkgconfig.go).

/*
#cgo CFLAGS: -I${SRCDIR}/../../../
#cgo LDFLAGS: -L${SRCDIR}/../../../../build -laviary_c -Wl,-rpath,${SRCDIR}/../../../../build
*/
import "C"

// Note: cryptolib's own cgo_local.go now bakes the rpath to its build/release,
// so we no longer add a cryptolib rpath here — importing its Go package brings
// the loader path along. For a fully self-contained binary (no shared library at
// run time at all), build with `-tags "aviary_static cryptolib_static"`
// after building both static archives (see cgo_static.go).
