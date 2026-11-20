#!/usr/bin/env bash
# Build the WebAssembly engine aviary_dart loads in a browser: the aviary core,
# CipherBird's C ABI and its dependencies in one module, exporting the
# stateless functions the web binding wraps (hashing, header hashes, proof and
# certificate checks). Output: bridge/bindings/dart/native/web/aviary.mjs and
# aviary.wasm, an ES module whose default export instantiates the engine.
#
# Prereq: the wasm32 dependencies of the CipherBird checkout, built once there
# with scripts/web/build_all.sh (third_party/web/wasm32). CipherBird's C ABI is
# compiled here from that checkout's source, so the module bundles the
# checkout's current revision; the script ends by naming it.
# Usage: bash scripts/web/build_wasm.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CIPHERBIRD_ROOT="${CIPHERBIRD_ROOT:-$ROOT/../cryptolib}"
PREFIX="$CIPHERBIRD_ROOT/third_party/web/wasm32"
command -v emcc >/dev/null || { echo "emcc not found; install Emscripten"; exit 1; }
DEPS=(libsodium libblake3 liboqs libcrypto libblst libsecp256k1)
for dep in "${DEPS[@]}"; do
  [[ -f "$PREFIX/lib/$dep.a" ]] || { echo "missing $PREFIX/lib/$dep.a; run scripts/web/build_all.sh in the CipherBird checkout"; exit 1; }
done
LIBS=()
for dep in "${DEPS[@]}"; do LIBS+=("$PREFIX/lib/$dep.a"); done
BUILD="$ROOT/build-web"
OUT="$ROOT/bridge/bindings/dart/native/web"
FLAGS="-O3 -fwasm-exceptions -msimd128"
# The same feature set as CipherBird's own web engine (its scripts/web/build_engine.sh).
CIPHERBIRD_DEFS="-DCIPHERBIRD_BUILD_SHARED -DCIPHERBIRD_HAS_BLAKE3=1 -DCIPHERBIRD_HAS_PQ=1 -DCIPHERBIRD_HAS_SODIUM_CRYPTO=1 -DCIPHERBIRD_HAS_STEGO=1 -DCIPHERBIRD_HAS_MEDIA_ENTROPY=1 -DCIPHERBIRD_HAS_NATIVE_COMBINERS=1 -DCIPHERBIRD_HAS_BLS=1 -DCIPHERBIRD_HAS_OPENSSL=1 -DCIPHERBIRD_HAS_SECP256K1=1"
EXPORTS=(malloc free aviary_init aviary_version aviary_crypto_backend_version
  aviary_buffer_free aviary_str_free aviary_hash aviary_web_txid
  aviary_web_header_hash aviary_verify_state_proof aviary_verify_tx_inclusion
  aviary_web_verify_commit aviary_block_header aviary_header_subchain_root
  aviary_encoded_header_hash aviary_child_proof_verify
  aviary_subchain_proof_v2_verify)
EXPORT_LIST="$(printf '_%s,' "${EXPORTS[@]}")"

emcmake cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DAVIARY_STATIC=ON \
  -DAVIARY_LTO=OFF \
  -DCIPHERBIRD_ROOT="$CIPHERBIRD_ROOT" \
  -DCIPHERBIRD_C_INCLUDE_DIR="$CIPHERBIRD_ROOT/bridge" \
  -DCMAKE_CXX_FLAGS="$FLAGS" >/dev/null
cmake --build "$BUILD" --target aviary_c >/dev/null
em++ -std=c++20 $FLAGS $CIPHERBIRD_DEFS -I "$CIPHERBIRD_ROOT/include" -I "$PREFIX/include" \
  -c "$CIPHERBIRD_ROOT/bridge/cipherbird_c.cpp" -o "$BUILD/cipherbird_c.o"
em++ -std=c++20 $FLAGS -I "$ROOT/bridge" -c "$ROOT/bridge/web/aviary_web.cpp" -o "$BUILD/aviary_web.o"
mkdir -p "$OUT"
em++ $FLAGS "$BUILD/aviary_web.o" "$BUILD/libaviary_c.a" "$BUILD/libaviary_core.a" "$BUILD/cipherbird_c.o" \
  "${LIBS[@]}" \
  --no-entry \
  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createAviary \
  -sEXPORTED_FUNCTIONS="${EXPORT_LIST%,}" \
  -sEXPORTED_RUNTIME_METHODS=HEAPU8 \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=16MB -sMAXIMUM_MEMORY=2GB -sSTACK_SIZE=4MB \
  -sENVIRONMENT=web,worker -sASSERTIONS=0 \
  -o "$OUT/aviary.mjs"
REVISION="$(sed -n 's/^revision //p' "$BUILD/cipherbird_revision.txt")"
echo "ok: $OUT/aviary.wasm bundles CipherBird $REVISION ($(du -h "$OUT/aviary.wasm" | cut -f1), ${#EXPORTS[@]} exports)"
