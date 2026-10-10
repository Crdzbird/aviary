#!/usr/bin/env bash
# Cross-binding post-quantum determinism check: Go emits a hybrid scenario +
# state root; Dart replays it from the same keys and must reach the same root.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DY="${AVIARY_DYLIB:-$ROOT/build/libaviary_c.dylib}"
JSON="$(cd "$ROOT/bridge/bindings/go" && CGO_ENABLED=1 go run ./cmd/hybriddiff)"
( cd "$ROOT/bridge/bindings/dart" && AVIARY_DYLIB="$DY" dart run bin/hybrid_diff.dart "$JSON" )
