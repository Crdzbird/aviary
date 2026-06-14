#!/usr/bin/env bash
# One-shot verifier for the whole stack: C++ core + C ABI, then the Dart and Go
# bindings, all asserting the same cross-language golden vectors.
#
# Prereq: cryptolib's cryptolib_c built (e.g. in ../cryptolib via `make lib` or
# scripts/build_selfcontained.sh). Override its location with CRYPTOLIB_ROOT.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "── 1/3  C++ core + C ABI ─────────────────────────────────────────────"
CMAKE_ARGS=()
if [[ -n "${CRYPTOLIB_ROOT:-}" ]]; then
  CMAKE_ARGS+=("-DCRYPTOLIB_ROOT=${CRYPTOLIB_ROOT}")
fi
cmake -S . -B build -G "Unix Makefiles" ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}
cmake --build build
ctest --test-dir build --output-on-failure

echo
echo "── 2/3  Dart binding ─────────────────────────────────────────────────"
( cd bridge/bindings/dart && dart pub get && dart run bin/demo.dart )

echo
echo "── 3/3  Go binding ───────────────────────────────────────────────────"
( cd bridge/bindings/go && CGO_ENABLED=1 go run ./cmd/demo )

echo
echo "✓ all three languages agree on the golden vectors"
