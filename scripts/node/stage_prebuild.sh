#!/usr/bin/env bash
# Stage the native dylibs into the npm package's prebuilds/<triple>/ and make the
# pair relocatable (blockchainer finds cryptolib next to it via @loader_path), so
# `require('blockchainer')` works with no env override. Re-run after rebuilding.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CRYPTOLIB_ROOT="${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}"
PKG="$ROOT/bridge/bindings/blockchainer-node"

case "$(uname -s)-$(uname -m)" in
  Darwin-arm64) triple=darwin-arm64; ext=dylib ;;
  Darwin-x86_64) triple=darwin-x64; ext=dylib ;;
  Linux-x86_64) triple=linux-x64; ext=so ;;
  Linux-aarch64) triple=linux-arm64; ext=so ;;
  *) echo "unsupported platform"; exit 1 ;;
esac
DEST="$PKG/prebuilds/$triple"; mkdir -p "$DEST"

bc_src="$ROOT/build/libblockchainer_c.$ext"
cl_src="$CRYPTOLIB_ROOT/build/selfcontained/libcryptolib_c.$ext"
[[ -f "$cl_src" ]] || cl_src="$CRYPTOLIB_ROOT/build/libcryptolib_c.$ext"
cp "$bc_src" "$cl_src" "$DEST/"; chmod +w "$DEST"/*."$ext"

if [[ "$ext" == dylib ]]; then
  install_name_tool -id @loader_path/libcryptolib_c.dylib "$DEST/libcryptolib_c.dylib"
  install_name_tool -id @loader_path/libblockchainer_c.dylib "$DEST/libblockchainer_c.dylib"
  install_name_tool -change @rpath/libcryptolib_c.dylib @loader_path/libcryptolib_c.dylib "$DEST/libblockchainer_c.dylib"
  codesign -f -s - "$DEST"/*.dylib
else
  patchelf --set-rpath '$ORIGIN' "$DEST/libblockchainer_c.so" 2>/dev/null || true
fi
echo "staged → $DEST"
