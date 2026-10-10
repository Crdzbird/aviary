#!/usr/bin/env bash
# Merge CipherBird's per-platform xcframeworks (iOS and macOS are shipped as
# two bundles by the cipherbird Flutter plugin) into one xcframework that the
# Swift binding can reference as a single binary target.
# Usage: bash scripts/apple/vendor_cipherbird.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CIPHERBIRD="${CIPHERBIRD_ROOT:-${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}}/bridge/bindings/cipherbird"
OUT="$ROOT/bridge/bindings/swift/Vendor/CipherBird.xcframework"
ARGS=()
for bundle in "$CIPHERBIRD/ios/cipherbird/CipherBird.xcframework" "$CIPHERBIRD/macos/cipherbird/CipherBird.xcframework"; do
  [[ -d "$bundle" ]] || { echo "missing $bundle; build the cipherbird plugin first"; exit 1; }
  for slice in "$bundle"/*/CipherBird.framework; do
    ARGS+=(-framework "$slice")
  done
done
rm -rf "$OUT"
mkdir -p "$(dirname "$OUT")"
xcodebuild -create-xcframework "${ARGS[@]}" -output "$OUT" >/dev/null
echo "ok: $OUT ($(ls "$OUT" | grep -v Info.plist | tr '\n' ' '))"
