#!/usr/bin/env bash
# Build AviaryC.xcframework as a DYNAMIC-framework xcframework for the
# Flutter plugin's Swift Package.
#
# Unlike the old static+merged build, aviary's cryptolib_* symbols are
# left UNDEFINED (-undefined dynamic_lookup) and resolve at runtime against the
# CryptoLibC framework that cryptolib_flutter embeds. So this build:
#   1. compiles aviary_core.a + aviary_c.a per slice (no cryptolib),
#   2. wraps them in a dynamic AviaryC.framework (force-load -> all C-ABI
#      symbols exported for DynamicLibrary.process()),
#   3. assembles ios-arm64 + ios-arm64-simulator + macos-arm64 into one
#      xcframework, copied into the plugin's ios/ and macos/ SPM trees.
#
# Usage: ./build_dynamic_xcframework.sh [slice ...]   (default: all three)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CRYPTOLIB_ROOT="${CIPHERBIRD_ROOT:-${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}}"
TOOLCHAIN="$CRYPTOLIB_ROOT/cmake/ios.toolchain.cmake"
PLUGIN="$ROOT/bridge/bindings/aviary"
BUILD="$ROOT/build-apple"
FW_NAME="AviaryC"
VERSION="0.0.1"

[[ -f "$TOOLCHAIN" ]] || { echo "missing $TOOLCHAIN"; exit 1; }
mkdir -p "$BUILD"

# slice -> (IOS_PLATFORM for the cmake toolchain, xcrun sdk, min-version flag)
slice_ios_platform() { case "$1" in iphoneos) echo OS;; iphonesimulator) echo SIMULATOR;; macosx) echo MAC;; esac; }
slice_sdk()          { case "$1" in iphoneos) echo iphoneos;; iphonesimulator) echo iphonesimulator;; macosx) echo macosx;; esac; }
slice_minflag()      { case "$1" in
  iphoneos)        echo "-miphoneos-version-min=15.0";;
  iphonesimulator) echo "-mios-simulator-version-min=15.0";;
  macosx)          echo "-mmacosx-version-min=12.0";; esac; }
slice_platform_name(){ case "$1" in iphoneos) echo iPhoneOS;; iphonesimulator) echo iPhoneSimulator;; macosx) echo MacOSX;; esac; }

build_archives() {  # <slice>
  local slice="$1" platform; platform="$(slice_ios_platform "$slice")"
  echo "== compiling aviary archives: $slice ($platform) =="
  cmake -B "$BUILD/$slice" -S "$ROOT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DIOS_PLATFORM="$platform" \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVIARY_STATIC=ON \
    -DCRYPTOLIB_ROOT="$CRYPTOLIB_ROOT" \
    -DCRYPTOLIB_C_INCLUDE_DIR="$CRYPTOLIB_ROOT/bridge" >/dev/null
  cmake --build "$BUILD/$slice" --target aviary_c >/dev/null
  # One archive with aviary core + C ABI only (cryptolib stays external).
  libtool -static -o "$BUILD/$slice/libaviary_only.a" \
    "$BUILD/$slice/libaviary_core.a" "$BUILD/$slice/libaviary_c.a"
}

info_plist() {  # <slice> <out>
  local slice="$1" out="$2" platform min_key min_ver
  platform="$(slice_platform_name "$slice")"
  if [[ "$slice" == macosx ]]; then min_key=LSMinimumSystemVersion; min_ver=12.0; else min_key=MinimumOSVersion; min_ver=15.0; fi
  cat > "$out" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>CFBundleExecutable</key><string>${FW_NAME}</string>
  <key>CFBundleIdentifier</key><string>com.aviary.${FW_NAME}</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>CFBundleName</key><string>${FW_NAME}</string>
  <key>CFBundlePackageType</key><string>FMWK</string>
  <key>CFBundleShortVersionString</key><string>${VERSION}</string>
  <key>CFBundleVersion</key><string>${VERSION}</string>
  <key>CFBundleSupportedPlatforms</key><array><string>${platform}</string></array>
  <key>${min_key}</key><string>${min_ver}</string>
</dict></plist>
EOF
}

make_framework() {  # <slice> -> echoes .framework path
  local slice="$1" sdk minflag archive fwdir cc modulemap sysroot
  sdk="$(slice_sdk "$slice")"; minflag="$(slice_minflag "$slice")"
  archive="$BUILD/$slice/libaviary_only.a"
  fwdir="$BUILD/$slice/${FW_NAME}.framework"
  rm -rf "$fwdir"
  sysroot="$(xcrun --sdk "$sdk" --show-sdk-path)"
  cc="$(xcrun --sdk "$sdk" -f clang)"
  modulemap="framework module ${FW_NAME} {
    header \"aviary_c.h\"
    export *
}"
  # -undefined dynamic_lookup: cryptolib_* symbols resolve at load time from the
  # CryptoLibC framework (also embedded by cryptolib_flutter).
  local common=(-dynamiclib -arch arm64 -isysroot "$sysroot" "$minflag"
                -Wl,-force_load,"$archive" -undefined dynamic_lookup -lc++)
  if [[ "$slice" == macosx ]]; then
    local vdir="$fwdir/Versions/A"
    mkdir -p "$vdir/Headers" "$vdir/Modules" "$vdir/Resources"
    "$cc" "${common[@]}" -install_name "@rpath/${FW_NAME}.framework/Versions/A/${FW_NAME}" \
      -o "$vdir/${FW_NAME}" 2>"$BUILD/$slice/dylink.log" || { cat "$BUILD/$slice/dylink.log"; exit 1; }
    cp "$ROOT/bridge/aviary_c.h" "$vdir/Headers/aviary_c.h"
    printf '%s\n' "$modulemap" > "$vdir/Modules/module.modulemap"
    info_plist "$slice" "$vdir/Resources/Info.plist"
    cp "$ROOT/bridge/apple/PrivacyInfo.xcprivacy" "$vdir/Resources/PrivacyInfo.xcprivacy"
    ( cd "$fwdir/Versions" && ln -sfn A Current )
    ( cd "$fwdir" && ln -sfn "Versions/Current/${FW_NAME}" "${FW_NAME}" \
        && ln -sfn Versions/Current/Headers Headers \
        && ln -sfn Versions/Current/Modules Modules \
        && ln -sfn Versions/Current/Resources Resources )
  else
    mkdir -p "$fwdir/Headers" "$fwdir/Modules"
    "$cc" "${common[@]}" -install_name "@rpath/${FW_NAME}.framework/${FW_NAME}" \
      -o "$fwdir/${FW_NAME}" 2>"$BUILD/$slice/dylink.log" || { cat "$BUILD/$slice/dylink.log"; exit 1; }
    cp "$ROOT/bridge/aviary_c.h" "$fwdir/Headers/aviary_c.h"
    printf '%s\n' "$modulemap" > "$fwdir/Modules/module.modulemap"
    info_plist "$slice" "$fwdir/Info.plist"
    cp "$ROOT/bridge/apple/PrivacyInfo.xcprivacy" "$fwdir/PrivacyInfo.xcprivacy"
  fi
  echo "$fwdir"
}

SLICES=("$@"); [[ ${#SLICES[@]} -eq 0 ]] && SLICES=(iphoneos iphonesimulator macosx)
ARGS=()
for slice in "${SLICES[@]}"; do
  build_archives "$slice"
  fw="$(make_framework "$slice")"
  ARGS+=(-framework "$fw")
  echo "   ok: $slice -> ${FW_NAME}.framework ($(du -h "$BUILD/$slice/${FW_NAME}.framework"/*/"${FW_NAME}" 2>/dev/null | tail -1 | awk '{print $1}'))"
done

OUT="$BUILD/${FW_NAME}.xcframework"
rm -rf "$OUT"
xcodebuild -create-xcframework "${ARGS[@]}" -output "$OUT" >/dev/null
echo "ok: assembled $OUT"

# Stage into the plugin's SPM trees (ios + macos each get the full xcframework).
for dest in "$PLUGIN/ios/aviary" "$PLUGIN/macos/aviary"; do
  mkdir -p "$dest"
  rm -rf "$dest/${FW_NAME}.xcframework"
  cp -R "$OUT" "$dest/${FW_NAME}.xcframework"
  echo "   staged -> $dest/${FW_NAME}.xcframework"
done
echo "done."
