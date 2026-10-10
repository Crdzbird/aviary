//go:build aviary_static

package aviary

// Fully-static linking, selected with `-tags aviary_static`.
//
// Links the merged archive build/static/libaviary_c.a (aviary core
// + C ABI). aviary's cryptolib_* symbols are satisfied by cryptolib's own
// static archive, so build with BOTH tags (cryptolib also links statically):
//
//	scripts/build_static_archive.sh                       # aviary archive
//	( cd ../cryptolib && scripts/build_static_archive.sh ) # cryptolib archive
//	go build -tags "aviary_static cryptolib_static" ./...
//
// The resulting binary has NO dependency on libaviary_c / libcryptolib_c
// shared libraries — nothing to ship alongside it, no rpath, no
// DYLD_LIBRARY_PATH/LD_LIBRARY_PATH. Only the C++ runtime (-lc++ / -lstdc++) is
// resolved dynamically (present on every macOS and Linux system).

/*
#cgo CFLAGS: -I${SRCDIR}/../../../
#cgo darwin LDFLAGS: ${SRCDIR}/../../../../build/static/libaviary_c.a -lc++
#cgo linux  LDFLAGS: ${SRCDIR}/../../../../build/static/libaviary_c.a -lstdc++ -lm
*/
import "C"
