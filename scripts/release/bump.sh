#!/usr/bin/env bash
# Set the project version everywhere it is declared: the VERSION file that
# CMake reads, the three pub packages, the npm package, the Android module,
# the jar default, and a fresh section at the top of each changelog.
# Usage: bash scripts/release/bump.sh X.Y.Z
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
V="${1:?usage: bump.sh X.Y.Z}"
[[ "$V" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$ ]] || { echo "not a semantic version: $V"; exit 1; }
echo "$V" > "$ROOT/VERSION"
for p in dart aviary aviary_lite; do
  sed -i '' "s/^version: .*/version: $V/" "$ROOT/bridge/bindings/$p/pubspec.yaml"
done
sed -i '' "s/^version = \".*\"/version = \"$V\"/" "$ROOT/bridge/bindings/aviary/android/build.gradle.kts" "$ROOT/bridge/bindings/aviary_lite/android/build.gradle.kts" "$ROOT/bridge/bindings/aviary-android/build.gradle.kts"
( cd "$ROOT/bridge/bindings/aviary-node" && npm version "$V" --no-git-tag-version --allow-same-version > /dev/null )
sed -i '' "s/^VERSION=\"\${1:-.*}\"/VERSION=\"\${1:-$V}\"/" "$ROOT/scripts/jvm/build_jar.sh"
for c in bridge/bindings/dart/CHANGELOG.md bridge/bindings/aviary/CHANGELOG.md bridge/bindings/aviary_lite/CHANGELOG.md bridge/bindings/aviary-node/CHANGELOG.md bridge/bindings/go/CHANGELOG.md; do
  f="$ROOT/$c"
  [[ -f "$f" ]] || continue
  if ! grep -q "^## $V" "$f"; then
    python3 - "$f" "$V" <<'PY'
import sys
path, v = sys.argv[1:]
s = open(path).read()
head, sep, rest = s.partition("\n## ")
open(path, "w").write(head.rstrip("\n") + f"\n\n## {v}\n\n- Describe the changes in this release.\n" + (sep + rest if sep else ""))
PY
  fi
done
echo "version set to $V; edit the new changelog sections, then tag:"
echo "  aviary_dart-v$V  aviary_lite-v$V  aviary-v$V  aviary-ledger-v$V  bridge/bindings/go/v$V  swift-v$V"
