//go:build blockchainer_static

package blockchainer

// Fully-static linking, selected with `-tags blockchainer_static`.
//
// Links the merged archive build/static/libblockchainer_c.a (blockchainer core
// + C ABI). blockchainer's cryptolib_* symbols are satisfied by cryptolib's own
// static archive, so build with BOTH tags (cryptolib also links statically):
//
//	scripts/build_static_archive.sh                       # blockchainer archive
//	( cd ../cryptolib && scripts/build_static_archive.sh ) # cryptolib archive
//	go build -tags "blockchainer_static cryptolib_static" ./...
//
// The resulting binary has NO dependency on libblockchainer_c / libcryptolib_c
// shared libraries — nothing to ship alongside it, no rpath, no
// DYLD_LIBRARY_PATH/LD_LIBRARY_PATH. Only the C++ runtime (-lc++ / -lstdc++) is
// resolved dynamically (present on every macOS and Linux system).

/*
#cgo CFLAGS: -I${SRCDIR}/../../../
#cgo darwin LDFLAGS: ${SRCDIR}/../../../../build/static/libblockchainer_c.a -lc++
#cgo linux  LDFLAGS: ${SRCDIR}/../../../../build/static/libblockchainer_c.a -lstdc++ -lm
*/
import "C"
