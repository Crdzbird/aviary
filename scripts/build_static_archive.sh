#!/usr/bin/env bash
# Build a single STATIC archive: build/static/libblockchainer_c.a
#
# It merges blockchainer's core + C-ABI bridge objects into ONE .a. A consumer
# linking it (e.g. the Go binding with `-tags blockchainer_static`) gets a
# fully self-contained binary — no libblockchainer_c.dylib/.so to ship or locate
# at run time. blockchainer's cryptolib_* symbols stay UNDEFINED here; they are
# satisfied by cryptolib's own static archive (the cryptolib Go binding built
# with `-tags cryptolib_static`):
#
#   ( cd ../cryptolib && scripts/build_static_archive.sh )
#   scripts/build_static_archive.sh
#   ( cd bridge/bindings/go && go build -tags "blockchainer_static cryptolib_static" ./... )
#
# Usage: scripts/build_static_archive.sh [out_dir]   (macOS/Linux host)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="${1:-build/static}"
CRYPTOLIB_ROOT="${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}"
mkdir -p "$OUT"

# Build blockchainer_c as a STATIC archive (no cryptolib linked — symbols stay
# undefined, resolved later by cryptolib's static archive).
cmake -S . -B build-static -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBLOCKCHAINER_STATIC=ON \
  -DCRYPTOLIB_ROOT="$CRYPTOLIB_ROOT" \
  -DCRYPTOLIB_C_INCLUDE_DIR="$CRYPTOLIB_ROOT/bridge" >/dev/null
cmake --build build-static --target blockchainer_c >/dev/null

CORE="build-static/libblockchainer_core.a"
CABI="build-static/libblockchainer_c.a"
[[ -f "$CORE" && -f "$CABI" ]] || { echo "missing static archives in build-static/"; exit 1; }

if [[ "$(uname -s)" == "Darwin" ]]; then
  libtool -static -o "$OUT/libblockchainer_c.a" "$CORE" "$CABI"
else
  # GNU ar: concatenate members via a MRI script.
  rm -f "$OUT/libblockchainer_c.a"
  ar -M <<EOF
create $OUT/libblockchainer_c.a
addlib $CORE
addlib $CABI
save
end
EOF
fi
echo "built $OUT/libblockchainer_c.a ($(du -h "$OUT/libblockchainer_c.a" | awk '{print $1}'))"
