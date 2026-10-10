#!/usr/bin/env bash
# Build the self-contained JVM binding jar: compiled classes plus the native
# libraries for every slot shipped by aviary_dart (darwin-arm64, linux-x64,
# linux-arm64), each slot with an index.txt that NativeLoader extracts.
# Usage: bash scripts/jvm/build_jar.sh [version]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION="${1:-0.1.0}"
JVM="$ROOT/bridge/bindings/jvm"
NATIVE="$ROOT/bridge/bindings/dart/native"
OUT="$ROOT/dist/jvm"
STAGE="$JVM/out-jar"
rm -rf "$STAGE"; mkdir -p "$STAGE" "$OUT"
javac --release 23 "$JVM"/src/aviary/*.java -d "$STAGE"
for slot in "$NATIVE"/*/; do
  name="$(basename "$slot")"
  mkdir -p "$STAGE/native/$name"
  : > "$STAGE/native/$name/index.txt"
  for lib in "$slot"*.dylib "$slot"*.so "$slot"*.dll; do
    [[ -f "$lib" ]] || continue
    cp "$lib" "$STAGE/native/$name/"
    basename "$lib" >> "$STAGE/native/$name/index.txt"
  done
done
jar --create --file "$OUT/aviary-$VERSION.jar" --main-class aviary.Main -C "$STAGE" .
rm -rf "$STAGE"
echo "ok: $OUT/aviary-$VERSION.jar ($(du -h "$OUT/aviary-$VERSION.jar" | cut -f1))"
jar --list --file "$OUT/aviary-$VERSION.jar" | grep "native/" | grep -v "/$"
