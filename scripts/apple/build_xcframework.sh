#!/usr/bin/env bash
# Build libblockchainer_c as a static archive for iOS device, iOS simulator, and
# macOS, then assemble Blockchainer.xcframework. blockchainer_c only needs
# cryptolib's *header* to compile (its symbols resolve when the app links
# alongside cryptolib_flutter's xcframework), so this build links nothing.
#
# Output: bridge/bindings/blockchainer_flutter/{ios,macos}/Blockchainer.xcframework
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CRYPTOLIB_ROOT="${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}"
TOOLCHAIN="$CRYPTOLIB_ROOT/cmake/ios.toolchain.cmake"
PLUGIN="$ROOT/bridge/bindings/blockchainer_flutter"
BUILD="$ROOT/build-apple"

[[ -f "$TOOLCHAIN" ]] || { echo "missing $TOOLCHAIN"; exit 1; }
mkdir -p "$BUILD"

build_slice() {  # <slice> <IOS_PLATFORM> <cryptolib-slice>
  local slice="$1" platform="$2" cryptoslice="$3"
  echo "── building slice $slice ($platform) ──"
  cmake -B "$BUILD/$slice" -S "$ROOT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DIOS_PLATFORM="$platform" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBLOCKCHAINER_STATIC=ON \
    -DCRYPTOLIB_ROOT="$CRYPTOLIB_ROOT" \
    -DCRYPTOLIB_C_INCLUDE_DIR="$CRYPTOLIB_ROOT/bridge" >/dev/null
  cmake --build "$BUILD/$slice" --target blockchainer_c >/dev/null
  # Merge core + bridge + cryptolib's slice into ONE self-contained archive, so
  # the xcframework needs no companion pod (avoids cross-plugin OTHER_LDFLAGS
  # conflicts). cryptolib's merged.a already bundles its own deps.
  local crypto_a="$CRYPTOLIB_ROOT/CryptoLib.xcframework/$cryptoslice/libcryptolib_c_merged.a"
  [[ -f "$crypto_a" ]] || { echo "missing $crypto_a — rebuild cryptolib's xcframework"; exit 1; }
  libtool -static -o "$BUILD/$slice/libblockchainer_c_merged.a" \
    "$BUILD/$slice/libblockchainer_core.a" "$BUILD/$slice/libblockchainer_c.a" "$crypto_a"
  echo "   $(du -h "$BUILD/$slice/libblockchainer_c_merged.a" | awk '{print $1}')  libblockchainer_c_merged.a (incl. cryptolib)"
}

build_slice iphoneos        OS        ios-arm64
build_slice iphonesimulator SIMULATOR ios-arm64-simulator
build_slice macosx          MAC       macos-arm64

# Stage headers + modulemap (so Swift can `import BlockchainerC`).
HDR="$BUILD/headers"
rm -rf "$HDR"; mkdir -p "$HDR"
cp "$ROOT/bridge/blockchainer_c.h" "$HDR/blockchainer_c.h"
cat > "$HDR/module.modulemap" <<EOF
module BlockchainerC {
    header "blockchainer_c.h"
    export *
}
EOF

assemble() {  # <out-dir> <slice...>
  local out="$1"; shift
  local args=()
  for s in "$@"; do
    args+=(-library "$BUILD/$s/libblockchainer_c_merged.a" -headers "$HDR")
  done
  mkdir -p "$out"
  rm -rf "$out/Blockchainer.xcframework"
  xcodebuild -create-xcframework "${args[@]}" -output "$out/Blockchainer.xcframework" >/dev/null
  echo "✓ $out/Blockchainer.xcframework"
}

assemble "$PLUGIN/ios"   iphoneos iphonesimulator macosx
assemble "$PLUGIN/macos" iphoneos iphonesimulator macosx
echo "done."
