#!/usr/bin/env bash
# Stage the native libraries into the npm package's prebuilds/<triple>/ so that
# require('aviary-ledger') works with no environment override. The libraries
# are the ones aviary_dart ships: the relocatable darwin pair (libaviary_c
# finds libcipherbird next to it through its loader relative install name) and
# the self-contained Linux libraries. Re-run after rebuilding the engine.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
NATIVE="$ROOT/bridge/bindings/dart/native"
PKG="$ROOT/bridge/bindings/aviary-node"
rm -rf "$PKG/prebuilds"
for slot in "$NATIVE"/*/; do
  name="$(basename "$slot")"
  mkdir -p "$PKG/prebuilds/$name"
  cp "$slot"/*.dylib "$slot"/*.so "$PKG/prebuilds/$name/" 2>/dev/null || true
done
find "$PKG/prebuilds" -type f | sed "s#$PKG/##" | sort
