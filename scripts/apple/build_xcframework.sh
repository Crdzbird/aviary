#!/usr/bin/env bash
# Build libaviary_c as a static archive for iOS device, iOS simulator, and
# macOS, then assemble Aviary.xcframework. aviary_c only needs
# cipherbird's *header* to compile (its symbols resolve when the app links
# alongside cipherbird_flutter's xcframework), so this build links nothing.
#
# Output: bridge/bindings/aviary_flutter/{ios,macos}/Aviary.xcframework
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CIPHERBIRD_ROOT="${CIPHERBIRD_ROOT:-$ROOT/../cryptolib}"
TOOLCHAIN="$CIPHERBIRD_ROOT/cmake/ios.toolchain.cmake"
PLUGIN="$ROOT/bridge/bindings/aviary_lite"
BUILD="$ROOT/build-apple"

[[ -f "$TOOLCHAIN" ]] || { echo "missing $TOOLCHAIN"; exit 1; }
mkdir -p "$BUILD"

build_slice() {  # <slice> <IOS_PLATFORM> <cryptolib-slice>
  local slice="$1" platform="$2" cryptoslice="$3"
  echo "== building slice $slice ($platform) =="
  cmake -B "$BUILD/$slice" -S "$ROOT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DIOS_PLATFORM="$platform" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVIARY_STATIC=ON \
    -DCIPHERBIRD_ROOT="$CIPHERBIRD_ROOT" \
    -DCIPHERBIRD_C_INCLUDE_DIR="$CIPHERBIRD_ROOT/bridge" >/dev/null
  cmake --build "$BUILD/$slice" --target aviary_c >/dev/null
  # Merge core + bridge + cipherbird's slice into ONE self-contained archive, so
  # the xcframework needs no companion pod (avoids cross-plugin OTHER_LDFLAGS
  # conflicts). cipherbird's merged.a already bundles its own deps.
  # cipherbird >= 3.x keeps the per-slice merged archive in build/ios/<slice>/
  # (its xcframework now wraps slices as static *frameworks* - no bare .a
  # inside). Fall back to the pre-3.x in-xcframework location.
  local crypto_a="$CIPHERBIRD_ROOT/build/ios/$slice/libcipherbird_c_merged.a"
  [[ -f "$crypto_a" ]] || crypto_a="$CIPHERBIRD_ROOT/CipherBird.xcframework/$cryptoslice/libcipherbird_c_merged.a"
  [[ -f "$crypto_a" ]] || { echo "missing $crypto_a - run cipherbird's scripts/ios/build_all.sh"; exit 1; }
  libtool -static -o "$BUILD/$slice/libaviary_c_merged.a" \
    "$BUILD/$slice/libaviary_core.a" "$BUILD/$slice/libaviary_c.a" "$crypto_a"
  echo "   $(du -h "$BUILD/$slice/libaviary_c_merged.a" | awk '{print $1}')  libaviary_c_merged.a (incl. cipherbird)"
}

build_slice iphoneos        OS        ios-arm64
build_slice iphonesimulator SIMULATOR ios-arm64-simulator
build_slice macosx          MAC       macos-arm64

# Stage headers + modulemap (so Swift can `import AviaryC`).
HDR="$BUILD/headers"
rm -rf "$HDR"; mkdir -p "$HDR"
cp "$ROOT/bridge/aviary_c.h" "$HDR/aviary_c.h"
cat > "$HDR/module.modulemap" <<EOF
module AviaryC {
    header "aviary_c.h"
    export *
}
EOF

assemble() {  # <out-dir> <slice...>
  local out="$1"; shift
  local args=()
  for s in "$@"; do
    args+=(-library "$BUILD/$s/libaviary_c_merged.a" -headers "$HDR")
  done
  mkdir -p "$out"
  rm -rf "$out/Aviary.xcframework"
  xcodebuild -create-xcframework "${args[@]}" -output "$out/Aviary.xcframework" >/dev/null
  echo "ok: $out/Aviary.xcframework"
}

assemble "$PLUGIN/ios/aviary_lite"   iphoneos iphonesimulator
assemble "$PLUGIN/macos/aviary_lite" macosx
echo "done."
