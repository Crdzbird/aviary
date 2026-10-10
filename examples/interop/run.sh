#!/usr/bin/env bash
# Go↔Flutter interop check — start the Go server, then run the Flutter client.
# Prereq: aviary host lib built (cd ../.. && cmake --build build) and the
# sibling cryptolib checkout present.
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
( cd "$ROOT/server" && CGO_ENABLED=1 go run . ) &
SERVER=$!
trap "kill $SERVER 2>/dev/null" EXIT
sleep 2
# On an Android device, route its localhost to the host first:
#   adb reverse tcp:50053 tcp:50053
( cd "$ROOT/app" && flutter run "${@:-"-d","macos"}" )
