#!/usr/bin/env bash
# Package the Swift binding's binary targets for remote consumption: zip the
# dynamic AviaryC.xcframework and the merged CipherBird.xcframework, compute
# the SwiftPM checksums, and write them into bridge/bindings/swift/Package.swift
# for the given release tag (the manifest is Package.swift at the repository
# root). Upload the two zips as assets of a GitHub release
# named after that tag; SwiftPM downloads them by URL and verifies the checksum.
# Usage: bash scripts/apple/package_swift_release.sh swift-v1.0.0
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TAG="${1:?usage: package_swift_release.sh <release tag>}"
OUT="$ROOT/dist/swift"
SWIFT="$ROOT/bridge/bindings/swift"
[[ -d "$SWIFT/Vendor/CipherBird.xcframework" ]] || bash "$ROOT/scripts/apple/vendor_cipherbird.sh"
rm -rf "$OUT"; mkdir -p "$OUT"
( cd "$ROOT/bridge/bindings/aviary/ios/aviary" && zip -qry "$OUT/AviaryC.xcframework.zip" AviaryC.xcframework )
( cd "$SWIFT/Vendor" && zip -qry "$OUT/CipherBird.xcframework.zip" CipherBird.xcframework )
AVIARY_SUM="$(cd "$ROOT" && swift package compute-checksum "$OUT/AviaryC.xcframework.zip")"
CIPHER_SUM="$(cd "$ROOT" && swift package compute-checksum "$OUT/CipherBird.xcframework.zip")"
BASE="https://github.com/Crdzbird/aviary/releases/download/$TAG"
python3 - "$ROOT/Package.swift" "$BASE" "$AVIARY_SUM" "$CIPHER_SUM" <<'PY'
import re, sys
path, base, a, c = sys.argv[1:]
s = open(path).read()
s = re.sub(r'let releaseBase = "[^"]*"', f'let releaseBase = "{base}"', s)
s = re.sub(r'let aviaryChecksum = "[^"]*"', f'let aviaryChecksum = "{a}"', s)
s = re.sub(r'let cipherBirdChecksum = "[^"]*"', f'let cipherBirdChecksum = "{c}"', s)
open(path, "w").write(s)
PY
echo "assets:"; ls -la "$OUT" | awk 'NR>1 {print "  " $5 "  " $9}'
echo "AviaryC checksum:    $AVIARY_SUM"
echo "CipherBird checksum: $CIPHER_SUM"
echo "upload both zips to the GitHub release $TAG; Package.swift now points at $BASE"
