#!/usr/bin/env bash
# Build the self-contained libaviary_c.so for the Linux host architecture and
# place it where the aviary_dart build hook bundles it from:
#   bridge/bindings/dart/native/linux-<x64|arm64>/libaviary_c.so
# The CipherBird engine is linked in from its merged static archive, produced
# in the CipherBird checkout by scripts/build_static_archive_linux.sh. Links
# with lld so link time optimization and identical code folding apply.
# Usage: bash scripts/linux/build_so.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CRYPTOLIB_ROOT="${CIPHERBIRD_ROOT:-${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}}"
ENGINE="$CRYPTOLIB_ROOT/build/static/libcryptolib_c.a"
[[ -f "$ENGINE" ]] || { echo "missing $ENGINE; run scripts/build_static_archive_linux.sh in the CipherBird checkout first"; exit 1; }
case "$(uname -m)" in
  x86_64) SLOT=linux-x64;;
  aarch64|arm64) SLOT=linux-arm64;;
  *) echo "unsupported architecture $(uname -m)"; exit 1;;
esac
BUILD="$ROOT/build-linux"
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DAVIARY_BUILD_TESTS=OFF \
  -DCRYPTOLIB_ROOT="$CRYPTOLIB_ROOT" \
  -DCRYPTOLIB_C_INCLUDE_DIR="$CRYPTOLIB_ROOT/bridge" \
  -DCRYPTOLIB_C_LIBRARY="$ENGINE" \
  -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld -Wl,--exclude-libs,ALL -Wl,-z,defs -Wl,--icf=all" >/dev/null
cmake --build "$BUILD" --target aviary_c
strip --strip-unneeded "$BUILD/libaviary_c.so"
OUT="$ROOT/bridge/bindings/dart/native/$SLOT"
mkdir -p "$OUT"
cp "$BUILD/libaviary_c.so" "$OUT/libaviary_c.so"
echo "ok: $OUT/libaviary_c.so ($(du -h "$OUT/libaviary_c.so" | cut -f1))"
