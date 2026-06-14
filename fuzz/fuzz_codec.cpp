// libFuzzer harness for the canonical deserializers (the untrusted-input attack
// surface). Each decoder must either succeed or throw CodecError — anything else
// (or a crash caught by ASan/UBSan) is a bug.
//
// Build: cmake -DBLOCKCHAINER_BUILD_FUZZERS=ON (needs a libFuzzer-capable clang)
// Run:   ./build/fuzz_codec -max_total_time=60
#include <cstddef>
#include <cstdint>

#include "blockchainer/account_state.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/utxo_state.hpp"

using namespace blockchainer;

namespace {
template <class F>
void try_decode(F&& f) {
  try {
    f();
  } catch (const codec::CodecError&) {
    // expected for malformed input
  }
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  codec::ByteView v(data, size);
  Bytes b(data, data + size);
  try_decode([&] { (void)Transaction::decode_all(v); });
  try_decode([&] { (void)BlockHeader::decode_all(v); });
  try_decode([&] { (void)Block::decode_all(v); });
  try_decode([&] { (void)AccountStateMachine::decode_transfer(b); });
  try_decode([&] { (void)UtxoStateMachine::decode_body(b); });
  return 0;
}
