cmake_minimum_required(VERSION 3.20)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" AVIARY_VERSION_FROM_FILE LIMIT_COUNT 1)
string(STRIP "${AVIARY_VERSION_FROM_FILE}" AVIARY_VERSION_FROM_FILE)
project(aviary VERSION ${AVIARY_VERSION_FROM_FILE} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Hidden by default; the C ABI marks its exports explicitly (AVIARY_API).
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Optional AddressSanitizer + UndefinedBehaviorSanitizer over all targets. The
# deterministic fuzz-lite test then runs under sanitizers in `ctest`.
option(AVIARY_SANITIZE "Build with ASan + UBSan" OFF)
if(AVIARY_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()
option(AVIARY_SANITIZE_THREAD "Build with ThreadSanitizer (exclusive with AVIARY_SANITIZE)" OFF)
if(AVIARY_SANITIZE_THREAD)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=thread)
endif()

# Optional libFuzzer targets (needs a libFuzzer-capable clang, e.g. LLVM clang).
option(AVIARY_BUILD_FUZZERS "Build libFuzzer targets" OFF)

# Build the C ABI as a STATIC archive (for an XCFramework / static linking into
# an app). In this mode we do NOT link cryptolib_c - its symbols are resolved
# when the final app links (alongside cryptolib's own static lib). Only the
# header is needed. Forced on for iOS.
option(AVIARY_STATIC "Build aviary_c as a STATIC archive" OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(AVIARY_STATIC ON CACHE BOOL "Force STATIC on iOS" FORCE)
endif()

# Cross-compiles only want the library; skip tests/tools/example.
option(AVIARY_BUILD_TESTS "Build the test suite + tools" ON)

if(AVIARY_STATIC OR CMAKE_CROSSCOMPILING)
  set(AVIARY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
endif()
# Distribution builds (tests off) default to link time optimization and dead
# section elimination so the shipped engines carry only what the C ABI reaches.
# Host builds with the test suite keep LTO off: every test links the core and
# would repeat the whole program optimization.
if(AVIARY_BUILD_TESTS)
  set(_aviary_lto_default OFF)
else()
  set(_aviary_lto_default ON)
endif()
option(AVIARY_LTO "Link time optimization for the core and the C ABI library" ${_aviary_lto_default})
option(AVIARY_GC_SECTIONS "Compile with function and data sections and let the linker drop unreferenced ones (ELF targets)" ON)
# A static archive must hold machine code: LTO would leave LLVM bitcode in it,
# which xcodebuild and app linkers of other toolchains refuse.
if(AVIARY_LTO AND NOT AVIARY_STATIC AND NOT AVIARY_SANITIZE AND NOT AVIARY_SANITIZE_THREAD AND NOT AVIARY_BUILD_FUZZERS)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT _aviary_ipo OUTPUT _aviary_ipo_msg LANGUAGES CXX)
  if(_aviary_ipo)
    message(STATUS "aviary: link time optimization ON")
  else()
    message(STATUS "aviary: link time optimization unavailable: ${_aviary_ipo_msg}")
  endif()
else()
  set(_aviary_ipo OFF)
endif()

# -----------------------------------------------------------------------------
# Locate the prebuilt cryptolib_c C ABI.
# Decision (ARCHITECTURE section 3): link cryptolib's self-contained C ABI, not its
# header-only C++ API - decouples our build from cryptolib's transitive deps.
# Override the checkout location with -DCRYPTOLIB_ROOT=<path> if not a sibling.
# -----------------------------------------------------------------------------
set(CRYPTOLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../cryptolib"
    CACHE PATH "Path to the cryptolib checkout")

find_path(CRYPTOLIB_C_INCLUDE_DIR
  NAMES cryptolib_c.h
  HINTS "${CRYPTOLIB_ROOT}/bridge"
  NO_DEFAULT_PATH)

if(NOT CRYPTOLIB_C_INCLUDE_DIR)
  message(FATAL_ERROR "cryptolib_c.h not found - set -DCRYPTOLIB_ROOT=<path>.")
endif()
message(STATUS "aviary: cryptolib_c header  = ${CRYPTOLIB_C_INCLUDE_DIR}")

# The cryptolib BINARY is only needed when we actually link it (the host SHARED
# build, or an Android .so). For a STATIC archive its symbols resolve at app
# link, so the library is optional. Pass -DCRYPTOLIB_C_LIBRARY=<path> to override
# (e.g. point at the Android libcryptolib_c.so).
if(NOT AVIARY_STATIC)
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
    message(FATAL_ERROR "cryptolib_c library not found - build it or pass -DCRYPTOLIB_C_LIBRARY=<path>.")
  endif()
  list(GET CRYPTOLIB_C_LIBRARY 0 _cryptolib_first)
  get_filename_component(CRYPTOLIB_C_LIBDIR "${_cryptolib_first}" DIRECTORY)
  message(STATUS "aviary: cryptolib_c library = ${CRYPTOLIB_C_LIBRARY}")
endif()

# -- Core static library ------------------------------------------------------
add_library(aviary_core STATIC
  src/version.cpp
  src/crypto/cryptolib_backend.cpp
  src/crypto/keyring.cpp
  src/crypto/custody_signing.cpp
  src/crypto/threshold.cpp
  src/codec/codec.cpp
  src/core/merkle.cpp
  src/core/types.cpp
  src/state/account_state.cpp
  src/state/utxo_state.cpp
  src/state/record_state.cpp
  src/state/state_serialize.cpp
  src/core/ledger.cpp
  src/core/signature_cache.cpp
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
  src/observability/event_log.cpp
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
  src/node/chain_tree.cpp
  src/node/remote_guard.cpp
  src/node/demo_node.cpp)
target_compile_definitions(aviary_core PRIVATE AVIARY_VERSION_STRING="${PROJECT_VERSION}")
target_include_directories(aviary_core
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CRYPTOLIB_C_INCLUDE_DIR}")
set_target_properties(aviary_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT AVIARY_STATIC)
  target_link_libraries(aviary_core PRIVATE ${CRYPTOLIB_C_LIBRARY})
endif()

# The TCP transport reactor drives one node per thread; the multi-validator test
# spawns them with std::thread. Link the platform thread library publicly so
# tests inherit it.
find_package(Threads REQUIRED)
target_link_libraries(aviary_core PUBLIC Threads::Threads)
if(_aviary_ipo)
  set_target_properties(aviary_core PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()
if(AVIARY_GC_SECTIONS AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android"))
  target_compile_options(aviary_core PRIVATE -ffunction-sections -fdata-sections)
endif()

# -- C ABI library: SHARED on host (loaded by bindings), STATIC for XCFramework -
if(AVIARY_STATIC)
  add_library(aviary_c STATIC bridge/aviary_c.cpp)
else()
  add_library(aviary_c SHARED bridge/aviary_c.cpp)
  target_compile_definitions(aviary_c PRIVATE AVIARY_BUILD_SHARED)
  set_target_properties(aviary_c PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")
  # On ELF targets keep the engine's internals (CipherBird and its statically
  # linked dependencies) out of the export table. This also lets the linker
  # resolve their relocations locally, which a shared object otherwise refuses
  # for objects that were not built position independent.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(aviary_c PRIVATE -Wl,--exclude-libs,ALL)
  endif()
  if(AVIARY_GC_SECTIONS AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Android"))
    target_compile_options(aviary_c PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(aviary_c PRIVATE -Wl,--gc-sections)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_options(aviary_c PRIVATE -Wl,--icf=all)
  endif()
endif()
if(_aviary_ipo)
  set_target_properties(aviary_c PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
endif()
target_include_directories(aviary_c
  PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/bridge"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(aviary_c PRIVATE aviary_core)

if(NOT AVIARY_BUILD_TESTS)
  return()  # cross / static builds only need the library
endif()

# -- Golden-vector generator (not a test; documents how vectors are derived) --
add_executable(print_vectors tools/print_vectors.cpp)
target_link_libraries(print_vectors PRIVATE aviary_core)
set_target_properties(print_vectors PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")

# -- State-commitment micro-benchmark (not a test; documents the P1 win) ------
add_executable(bench_state tools/bench_state.cpp)
target_link_libraries(bench_state PRIVATE aviary_core)
set_target_properties(bench_state PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")

# End to end engine throughput for the README's performance table.
add_executable(bench_engine tools/bench_engine.cpp)
target_link_libraries(bench_engine PRIVATE aviary_core)
set_target_properties(bench_engine PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")

# Plain and encrypted paths through the C ABI only, so the same file runs
# against any shipped libaviary_c (see scripts/android/bench_paths.sh).
add_executable(bench_paths tools/bench_paths.cpp)
target_link_libraries(bench_paths PRIVATE aviary_c)
target_include_directories(bench_paths PRIVATE bridge)

# -- Tests (zero-dependency runner, matching cryptolib's convention) ----------
enable_testing()

# One executable per test file; each links the core and is registered with ctest.
function(aviary_add_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE aviary_core)
  target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  set_target_properties(${name} PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")
  add_test(NAME ${name} COMMAND ${name})
endfunction()

aviary_add_test(test_smoke tests/test_smoke.cpp)
aviary_add_test(test_registry_threads tests/test_registry_threads.cpp)
aviary_add_test(test_codec tests/test_codec.cpp)
aviary_add_test(test_merkle tests/test_merkle.cpp)
aviary_add_test(test_signature_cache tests/test_signature_cache.cpp)
aviary_add_test(test_types tests/test_types.cpp)
aviary_add_test(test_ledger tests/test_ledger.cpp)
aviary_add_test(test_utxo tests/test_utxo.cpp)
aviary_add_test(test_record_state tests/test_record_state.cpp)
# The C ABI itself, through the shared library every binding loads.
if(NOT AVIARY_STATIC)
  add_executable(test_c_abi_extras tests/test_c_abi_extras.cpp)
  target_link_libraries(test_c_abi_extras PRIVATE aviary_c)
  target_include_directories(test_c_abi_extras PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  set_target_properties(test_c_abi_extras PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR};${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME test_c_abi_extras COMMAND test_c_abi_extras)
endif()
aviary_add_test(test_codec_fuzz tests/test_codec_fuzz.cpp)
aviary_add_test(test_bls tests/test_bls.cpp)
aviary_add_test(test_consensus tests/test_consensus.cpp)
aviary_add_test(test_bft tests/test_bft.cpp)
aviary_add_test(test_mempool tests/test_mempool.cpp)
aviary_add_test(test_secure_session tests/test_secure_session.cpp)
aviary_add_test(test_noise_session tests/test_noise_session.cpp)
aviary_add_test(test_wire tests/test_wire.cpp)
aviary_add_test(test_p2p_integration tests/test_p2p_integration.cpp)
aviary_add_test(test_observability tests/test_observability.cpp)
aviary_add_test(test_observability_integration tests/test_observability_integration.cpp)
aviary_add_test(test_verification tests/test_verification.cpp)
aviary_add_test(test_interop_crypto tests/test_interop_crypto.cpp)
aviary_add_test(test_rlp tests/test_rlp.cpp)
aviary_add_test(test_evm tests/test_evm.cpp)
aviary_add_test(test_abi tests/test_abi.cpp)
aviary_add_test(test_eth_rpc tests/test_eth_rpc.cpp)
aviary_add_test(test_encoding tests/test_encoding.cpp)
aviary_add_test(test_solana tests/test_solana.cpp)
aviary_add_test(test_bitcoin tests/test_bitcoin.cpp)
aviary_add_test(test_bridge tests/test_bridge.cpp)
aviary_add_test(test_demo_node tests/test_demo_node.cpp)
aviary_add_test(test_hybrid_sig tests/test_hybrid_sig.cpp)
aviary_add_test(test_state_incremental tests/test_state_incremental.cpp)
aviary_add_test(test_consensus_hardening tests/test_consensus_hardening.cpp)
aviary_add_test(test_keyring tests/test_keyring.cpp)
aviary_add_test(test_round_state tests/test_round_state.cpp)
aviary_add_test(test_round_driver tests/test_round_driver.cpp)
aviary_add_test(test_genesis tests/test_genesis.cpp)
aviary_add_test(test_receipts tests/test_receipts.cpp)
aviary_add_test(test_snapshot tests/test_snapshot.cpp)
aviary_add_test(test_block_store tests/test_block_store.cpp)
aviary_add_test(test_utxo_multisig tests/test_utxo_multisig.cpp)
aviary_add_test(test_storage tests/test_storage.cpp)
aviary_add_test(test_pow tests/test_pow.cpp)
aviary_add_test(test_pos tests/test_pos.cpp)
aviary_add_test(test_fork_choice tests/test_fork_choice.cpp)
aviary_add_test(test_gossip tests/test_gossip.cpp)
aviary_add_test(test_kvstore tests/test_kvstore.cpp)
aviary_add_test(test_networked_node tests/test_networked_node.cpp)
aviary_add_test(test_tcp_transport tests/test_tcp_transport.cpp)
aviary_add_test(test_byzantine tests/test_byzantine.cpp)
aviary_add_test(test_peer_book tests/test_peer_book.cpp)
aviary_add_test(test_threshold tests/test_threshold.cpp)
aviary_add_test(test_vrf_pos tests/test_vrf_pos.cpp)
aviary_add_test(test_armored tests/test_armored.cpp)
aviary_add_test(test_portable tests/test_portable.cpp)
aviary_add_test(test_assurance tests/test_assurance.cpp)
aviary_add_test(test_sealed_mempool tests/test_sealed_mempool.cpp)
aviary_add_test(test_profiled_validator tests/test_profiled_validator.cpp)
aviary_add_test(test_chain_blueprint tests/test_chain_blueprint.cpp)
aviary_add_test(test_subchains tests/test_subchains.cpp)
aviary_add_test(test_validator_rotation tests/test_validator_rotation.cpp)
aviary_add_test(test_light_client_skip tests/test_light_client_skip.cpp)
aviary_add_test(test_chain_tree tests/test_chain_tree.cpp)
aviary_add_test(test_remote_guard tests/test_remote_guard.cpp)
aviary_add_test(test_bbs tests/test_bbs.cpp)
aviary_add_test(test_bbs_pseudonym tests/test_bbs_pseudonym.cpp)
aviary_add_test(test_credential_gate tests/test_credential_gate.cpp)
aviary_add_test(test_credential_gate_pseudonym tests/test_credential_gate_pseudonym.cpp)
aviary_add_test(test_admission tests/test_admission.cpp)
aviary_add_test(test_admission_socket tests/test_admission_socket.cpp)

option(AVIARY_FUZZER_STANDALONE "Build the fuzz targets with a file driver instead of libFuzzer" OFF)
if(AVIARY_BUILD_FUZZERS)
  function(aviary_add_fuzzer name source)
    if(AVIARY_FUZZER_STANDALONE)
      add_executable(${name} ${source} fuzz/standalone_main.cpp)
    else()
      add_executable(${name} ${source})
      target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
      target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    endif()
    target_link_libraries(${name} PRIVATE ${ARGN} aviary_core)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/bridge")
    set_target_properties(${name} PROPERTIES BUILD_RPATH "${CRYPTOLIB_C_LIBDIR}")
  endfunction()
  aviary_add_fuzzer(fuzz_codec fuzz/fuzz_codec.cpp)
  aviary_add_fuzzer(fuzz_restore fuzz/fuzz_restore.cpp)
  aviary_add_fuzzer(fuzz_c_abi fuzz/fuzz_c_abi.cpp aviary_c)
  aviary_add_fuzzer(fuzz_peering fuzz/fuzz_peering.cpp aviary_c)
endif()
