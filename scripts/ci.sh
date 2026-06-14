#!/usr/bin/env bash
# Full CI gate: C++ build + tests, a sanitized pass, the Dart & Go bindings, and
# Flutter analyze. Run locally or from .github/workflows/ci.yml.
#
# Prereq: cryptolib's cryptolib_c built. Override its location with CRYPTOLIB_ROOT
# (default: a sibling ../cryptolib checkout).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CMAKE_ARGS=()
if [[ -n "${CRYPTOLIB_ROOT:-}" ]]; then CMAKE_ARGS+=("-DCRYPTOLIB_ROOT=${CRYPTOLIB_ROOT}"); fi

echo "── 1/5  C++ build + tests ────────────────────────────────────────────"
cmake -S . -B build -G "Unix Makefiles" ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}
cmake --build build
ctest --test-dir build --output-on-failure

echo "── 2/5  ASan + UBSan ─────────────────────────────────────────────────"
cmake -S . -B build-asan -G "Unix Makefiles" -DBLOCKCHAINER_SANITIZE=ON ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"}
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure

echo "── 3/5  Dart binding ─────────────────────────────────────────────────"
( cd bridge/bindings/dart && dart pub get && dart run bin/demo.dart && dart run bin/node_demo.dart )

echo "── 4/5  Go binding ───────────────────────────────────────────────────"
( cd bridge/bindings/go && CGO_ENABLED=1 go run ./cmd/demo )

echo "── 5/6  JVM binding (Java + Kotlin) ──────────────────────────────────"
if command -v javac >/dev/null 2>&1; then
  LIB="$ROOT/build/libblockchainer_c.dylib"
  [[ -f "$LIB" ]] || LIB="$ROOT/build/libblockchainer_c.so"
  ( cd bridge/bindings/jvm && javac src/blockchainer/*.java -d out \
      && java --enable-native-access=ALL-UNNAMED -cp out -Dblockchainer.path="$LIB" blockchainer.Main >/dev/null \
      && echo "  Java demo OK" )
  if command -v kotlinc >/dev/null 2>&1; then
    ( cd bridge/bindings/jvm && kotlinc Demo.kt -cp out -include-runtime -d demo.jar 2>/dev/null \
        && java --enable-native-access=ALL-UNNAMED -cp out:demo.jar -Dblockchainer.path="$LIB" DemoKt >/dev/null \
        && echo "  Kotlin demo OK" )
  fi
else
  echo "javac not installed — skipping JVM"
fi

echo "── 6/6  Flutter example (analyze) ────────────────────────────────────"
if command -v flutter >/dev/null 2>&1; then
  ( cd examples/blockchainer_demo && flutter pub get && flutter analyze )
else
  echo "flutter not installed — skipping analyze"
fi

echo
echo "✓ CI passed"
