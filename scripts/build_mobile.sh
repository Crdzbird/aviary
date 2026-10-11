#!/usr/bin/env bash
# Build all of aviary's native artifacts for mobile/desktop bindings:
# the Apple xcframework (iOS device + simulator + macOS) and the Android .so.
#
# PREREQUISITE - cryptolib's *mobile* binaries must include the interop
# primitives (keccak256 / ripemd160 / secp256k1). If you added those to
# cryptolib recently, regenerate them first (see docs/MOBILE.md):
#   ( cd ../cryptolib && bash scripts/ios/build_all.sh && bash scripts/ios/assemble_xcframework.sh )
#   ( cd ../cryptolib && bash scripts/android/build_all.sh )   # -> cryptolib_flutter jniLibs
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "==== Apple xcframework (iOS + macOS) ===="
bash scripts/apple/build_xcframework.sh

echo
echo "==== Android shared libraries ===="
bash scripts/android/build_so.sh

echo
echo "ok native artifacts built into bridge/bindings/aviary_flutter/{ios,macos,android}"
echo "  Flutter app: ( cd examples/aviary_demo && flutter run -d <device> )"
