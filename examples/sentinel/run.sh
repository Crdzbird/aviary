#!/usr/bin/env bash
# Sentinel device provenance ledger — start the gRPC server, then run the app.
# Prereq: the blockchainer host dylib is built (cd ../.. && cmake --build build).
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
( cd "$ROOT/server" && CGO_ENABLED=1 go run . ) &
SERVER=$!
trap "kill $SERVER 2>/dev/null" EXIT
sleep 2
( cd "$ROOT/app" && flutter run "${@:-"-d","macos"}" )
