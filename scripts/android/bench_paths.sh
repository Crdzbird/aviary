#!/usr/bin/env bash
# Builds tools/bench_paths.cpp for arm64 Android against a shipped
# libaviary_c.so, runs it on a connected device pinned to one core, and prints
# the markdown table. The plain and encrypted paths all go through the C ABI,
# so the numbers are those of the library an app ships.
#
# Usage: scripts/android/bench_paths.sh [libaviary_c.so] [cpu]
#   libaviary_c.so  default: the self contained aviary_lite arm64 engine
#   cpu             core to pin to, default 7 (a Cortex-X1 on Tensor G2)
# ANDROID_SERIAL selects the device when more than one is attached.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LIB="${1:-$ROOT/bridge/bindings/aviary_lite/android/src/main/jniLibs/arm64-v8a/libaviary_c.so}"
CPU="${2:-7}"
NDK="${ANDROID_NDK_HOME:-$(ls -d "$HOME/Library/Android/sdk/ndk/"* 2>/dev/null | sort -V | tail -1)}"
CXX="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android24-clang++"
[[ -x "$CXX" ]] || { echo "Android NDK clang not found (set ANDROID_NDK_HOME)"; exit 1; }
OUT="$ROOT/build-android-bench"
DEST=/data/local/tmp/aviary-bench
mkdir -p "$OUT"
cp "$LIB" "$OUT/libaviary_c.so"
"$CXX" -std=c++20 -O2 -I "$ROOT/bridge" -static-libstdc++ "$ROOT/tools/bench_paths.cpp" \
  -L "$OUT" -laviary_c -o "$OUT/bench_paths"
adb shell "mkdir -p $DEST" >/dev/null
adb push "$OUT/libaviary_c.so" "$OUT/bench_paths" "$DEST/" >/dev/null
MASK="$(printf '%x' $((1 << CPU)))"
echo "device: $(adb shell getprop ro.product.model | tr -d '\r'), Android $(adb shell getprop ro.build.version.release | tr -d '\r'), cpu$CPU at $(adb shell cat /sys/devices/system/cpu/cpu$CPU/cpufreq/cpuinfo_max_freq | tr -d '\r') kHz"
echo "engine: $LIB"
adb shell "cd $DEST && LD_LIBRARY_PATH=. taskset $MASK ./bench_paths"
