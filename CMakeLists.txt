cmake_minimum_required(VERSION 3.20)
project(blockchainer VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Hidden by default; the C ABI marks its exports explicitly (BLOCKCHAINER_API).
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Optional AddressSanitizer + UndefinedBehaviorSanitizer over all targets. The
# deterministic fuzz-lite test then runs under sanitizers in `ctest`.
option(BLOCKCHAINER_SANITIZE "Build with ASan + UBSan" OFF)
if(BLOCKCHAINER_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

# Optional libFuzzer targets (needs a libFuzzer-capable clang, e.g. LLVM clang).
option(BLOCKCHAINER_BUILD_FUZZERS "Build libFuzzer targets" OFF)

# Build the C ABI as a STATIC archive (for an XCFramework / static linking into
# an app). In this mode we do NOT link cryptolib_c — its symbols are resolved
# when the final app links (alongside cryptolib's own static lib). Only the
# header is needed. Forced on for iOS.
option(BLOCKCHAINER_STATIC "Build blockchainer_c as a STATIC archive" OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(BLOCKCHAINER_STATIC ON CACHE BOOL "Force STATIC on iOS" FORCE)
endif()

# Cross-compiles only want the library; skip tests/tools/example.
option(BLOCKCHAINER_BUILD_TESTS "Build the test suite + tools" ON)
if(BLOCKCHAINER_STATIC OR CMAKE_CROSSCOMPILING)
  set(BLOCKCHAINER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Locate the prebuilt cryptolib_c C ABI.
# Decision (ARCHITECTURE §3): link cryptolib's self-contained C ABI, not its
# header-only C++ API — decouples our build from cryptolib's transitive deps.
# Override the checkout location with -DCRYPTOLIB_ROOT=<path> if not a sibling.
# ─────────────────────────────────────────────────────────────────────────────
set(CRYPTOLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cryptolib"
    CACHE PATH "Path to the cryptolib checkout")

find_path(CRYPTOLIB_C_INCLUDE_DIR
  NAMES cryptolib_c.h
  HINTS "${CRYPTOLIB_ROOT}/bridge"
  NO_DEFAULT_PATH)

if(NOT CRYPTOLIB_C_INCLUDE_DIR)
  message(FATAL_ERROR "cryptolib_c.h not found — set -DCRYPTOLIB_ROOT=<path>.")
endif()
message(STATUS "blockchainer: cryptolib_c header  = ${CRYPTOLIB_C_INCLUDE_DIR}")

# The cryptolib BINARY is only needed when we actually link it (the host SHARED
# build, or an Android .so). For a STATIC archive its symbols resolve at app
# link, so the library is optional. Pass -DCRYPTOLIB_C_LIBRARY=<path> to override
# (e.g. point at the Android libcryptolib_c.so).
if(NOT BLOCKCHAINER_STATIC)
  if(NOT CRYPTOLIB_C_LIBRARY)
    find_library(CRYPTOLIB_C_LIBRARY
      NAMES cryptolib_c
      HINTS
        "${CRYPTOLIB_ROOT}/build/selfcontained"
        "${CRYPTOLIB_ROOT}/build/release"
        "${CRYPTOLIB_ROOT}/build"
      NO_DEFAULT_PATH)
  endif()
  if(NOT CRYPTOLIB_C_LIBRARY)
    message(FATAL_ERROR "cryptolib_c library not found — build it or pass -DCRYPTOLIB_C_LIBRARY=<path>.")
  endif()
  get_filename_component(CRYPTOLIB_C_LIBDIR "${CRYPTOLIB_C_LIBRARY}" DIRECTORY)
  message(STATUS "blockchainer: cryptolib_c library = ${CRYPTOLIB_C_LIBRARY}")
endif()

# ── Core static library ──────────────────────────────────────────────────────
add_library(blockchainer_core STATIC
  src/version.cpp
  src/crypto/cryptolib_backend.cpp
  src/crypto/keyring.cpp
  src/crypto/threshold.cpp
  src/codec/codec.cpp
  src/core/merkle.cpp
  src/core/types.cpp
  src/state/account_state.cpp
  src/state/utxo_state.cpp
  src/state/state_serialize.cpp
  src/core/ledger.cpp
  src/core/genesis.cpp
  src/core/assurance.cpp
  src/core/chain_blueprint.cpp
  src/core/subchain.cpp
  src/core/block_store.cpp
  src/core/fork_choice.cpp
  src/storage/file_store.cpp
  src/storage/kvstore.cpp
  src/consensus/consensus.cpp
  src/consensus/bft_engine.cpp
  src/consensus/round_state.cpp
  src/consensus/round_driver.cpp
  src/consensus/pow_engine.cpp
  src/consensus/pos_engine.cpp
  src/consensus/vrf_pos.cpp
  src/consensus/credential_gate.cpp
  src/p2p/mempool.cpp
  src/p2p/admission.cpp
  src/p2p/admission_socket.cpp
  src/p2p/sealed_mempool.cpp
  src/p2p/gossip.cpp
  src/p2p/peer_book.cpp
  src/p2p/armored.cpp
  src/p2p/networked_node.cpp
  src/p2p/tcp_transport.cpp
  src/p2p/secure_session.cpp
  src/p2p/noise_session.cpp
  src/p2p/wire.cpp
  src/observability/metrics.cpp
  src/observability/log.cpp
  src/client/light_client.cpp
  src/client/proofs.cpp
  src/interop/rlp.cpp
  src/interop/evm.cpp
  src/interop/abi.cpp
  src/interop/json.cpp
  src/interop/rpc.cpp
  src/interop/eth_client.cpp
  src/interop/encoding.cpp
  src/interop/solana.cpp
  src/interop/bitcoin.cpp
  src/interop/bridge.cpp
  src/node/profiled_validator.cpp
  src/node/chain_assembler.cpp
  src/node/foreign_engine.cpp
  src/node/subchain_relay.cpp
  src/node/remote_guard.cpp
  src/node/demo_node.cpp)
target_include_directories(blockchainer_core
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CRYPTOLIB_C_INCLUDE_DIR}")
set_target_properties(blockchainer_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT BLOCKCHAINER_STATIC)
  target_link_libraries(blockchainer_core PRIVATE "${CRYPTOLIB_C_LIBRARY}")
endif()

# The TCP transport reactor drives one node per thread; the multi-validator test
# spawns them with std::thread. Link the platform thread library publicly so
# tests inherit it.
find_package(Threads REQUIRED)
target_link_libraries(blockchainer_core PUBLIC Threads::Threads)

# ── C ABI library: SHARED on host (loaded by bindings), STATIC for XCFramework ─
if(BLOCKCHAINER_STATIC)
  add_library(blockchainer_c STATIC bridge/blockchainer_c.cpp)
else()
  add_library(blockchainer_c SHARED bridge/blockchainer_c.cpp)
  target_compile_definitions(blockchainer_c PRIVATE BLOCKCHAINER_BUILD_SHARED)
  set_target_properties(blockchainer_c PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")
endif()
target_include_directories(blockchainer_c
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/bridge"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(blockchainer_c PRIVATE blockchainer_core)

if(NOT BLOCKCHAINER_BUILD_TESTS)
  return()  # cross / static builds only need the library
endif()

# ── Golden-vector generator (not a test; documents how vectors are derived) ──
add_executable(print_vectors tools/print_vectors.cpp)
target_link_libraries(print_vectors PRIVATE blockchainer_core)
set_target_properties(print_vectors PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")

# ── State-commitment micro-benchmark (not a test; documents the P1 win) ──────
add_executable(bench_state tools/bench_state.cpp)
target_link_libraries(bench_state PRIVATE blockchainer_core)
set_target_properties(bench_state PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")

# ── Tests (zero-dependency runner, matching cryptolib's convention) ──────────
enable_testing()

# One executable per test file; each links the core and is registered with ctest.
function(blockchainer_add_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE blockchainer_core)
  target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  set_target_properties(${name} PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")
  add_test(NAME ${name} COMMAND ${name})
endfunction()

blockchainer_add_test(test_smoke tests/test_smoke.cpp)
blockchainer_add_test(test_codec tests/test_codec.cpp)
blockchainer_add_test(test_merkle tests/test_merkle.cpp)
blockchainer_add_test(test_types tests/test_types.cpp)
blockchainer_add_test(test_ledger tests/test_ledger.cpp)
blockchainer_add_test(test_utxo tests/test_utxo.cpp)
blockchainer_add_test(test_codec_fuzz tests/test_codec_fuzz.cpp)
blockchainer_add_test(test_bls tests/test_bls.cpp)
blockchainer_add_test(test_consensus tests/test_consensus.cpp)
blockchainer_add_test(test_bft tests/test_bft.cpp)
blockchainer_add_test(test_mempool tests/test_mempool.cpp)
blockchainer_add_test(test_secure_session tests/test_secure_session.cpp)
blockchainer_add_test(test_noise_session tests/test_noise_session.cpp)
blockchainer_add_test(test_wire tests/test_wire.cpp)
blockchainer_add_test(test_p2p_integration tests/test_p2p_integration.cpp)
blockchainer_add_test(test_observability tests/test_observability.cpp)
blockchainer_add_test(test_observability_integration tests/test_observability_integration.cpp)
blockchainer_add_test(test_verification tests/test_verification.cpp)
blockchainer_add_test(test_interop_crypto tests/test_interop_crypto.cpp)
blockchainer_add_test(test_rlp tests/test_rlp.cpp)
blockchainer_add_test(test_evm tests/test_evm.cpp)
blockchainer_add_test(test_abi tests/test_abi.cpp)
blockchainer_add_test(test_eth_rpc tests/test_eth_rpc.cpp)
blockchainer_add_test(test_encoding tests/test_encoding.cpp)
blockchainer_add_test(test_solana tests/test_solana.cpp)
blockchainer_add_test(test_bitcoin tests/test_bitcoin.cpp)
blockchainer_add_test(test_bridge tests/test_bridge.cpp)
blockchainer_add_test(test_demo_node tests/test_demo_node.cpp)
blockchainer_add_test(test_hybrid_sig tests/test_hybrid_sig.cpp)
blockchainer_add_test(test_state_incremental tests/test_state_incremental.cpp)
blockchainer_add_test(test_consensus_hardening tests/test_consensus_hardening.cpp)
blockchainer_add_test(test_keyring tests/test_keyring.cpp)
blockchainer_add_test(test_round_state tests/test_round_state.cpp)
blockchainer_add_test(test_round_driver tests/test_round_driver.cpp)
blockchainer_add_test(test_genesis tests/test_genesis.cpp)
blockchainer_add_test(test_receipts tests/test_receipts.cpp)
blockchainer_add_test(test_snapshot tests/test_snapshot.cpp)
blockchainer_add_test(test_block_store tests/test_block_store.cpp)
blockchainer_add_test(test_utxo_multisig tests/test_utxo_multisig.cpp)
blockchainer_add_test(test_storage tests/test_storage.cpp)
blockchainer_add_test(test_pow tests/test_pow.cpp)
blockchainer_add_test(test_pos tests/test_pos.cpp)
blockchainer_add_test(test_fork_choice tests/test_fork_choice.cpp)
blockchainer_add_test(test_gossip tests/test_gossip.cpp)
blockchainer_add_test(test_kvstore tests/test_kvstore.cpp)
blockchainer_add_test(test_networked_node tests/test_networked_node.cpp)
blockchainer_add_test(test_tcp_transport tests/test_tcp_transport.cpp)
blockchainer_add_test(test_byzantine tests/test_byzantine.cpp)
blockchainer_add_test(test_peer_book tests/test_peer_book.cpp)
blockchainer_add_test(test_threshold tests/test_threshold.cpp)
blockchainer_add_test(test_vrf_pos tests/test_vrf_pos.cpp)
blockchainer_add_test(test_armored tests/test_armored.cpp)
blockchainer_add_test(test_portable tests/test_portable.cpp)
blockchainer_add_test(test_assurance tests/test_assurance.cpp)
blockchainer_add_test(test_sealed_mempool tests/test_sealed_mempool.cpp)
blockchainer_add_test(test_profiled_validator tests/test_profiled_validator.cpp)
blockchainer_add_test(test_chain_blueprint tests/test_chain_blueprint.cpp)
blockchainer_add_test(test_subchains tests/test_subchains.cpp)
blockchainer_add_test(test_validator_rotation tests/test_validator_rotation.cpp)
blockchainer_add_test(test_light_client_skip tests/test_light_client_skip.cpp)
blockchainer_add_test(test_remote_guard tests/test_remote_guard.cpp)
blockchainer_add_test(test_bbs tests/test_bbs.cpp)
blockchainer_add_test(test_bbs_pseudonym tests/test_bbs_pseudonym.cpp)
blockchainer_add_test(test_credential_gate tests/test_credential_gate.cpp)
blockchainer_add_test(test_credential_gate_pseudonym tests/test_credential_gate_pseudonym.cpp)
blockchainer_add_test(test_admission tests/test_admission.cpp)
blockchainer_add_test(test_admission_socket tests/test_admission_socket.cpp)

if(BLOCKCHAINER_BUILD_FUZZERS)
  add_executable(fuzz_codec fuzz/fuzz_codec.cpp)
  target_link_libraries(fuzz_codec PRIVATE blockchainer_core)
  target_compile_options(fuzz_codec PRIVATE -fsanitize=fuzzer,address,undefined)
  target_link_options(fuzz_codec PRIVATE -fsanitize=fuzzer,address,undefined)
  set_target_properties(fuzz_codec PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")
endif()
