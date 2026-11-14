// Entry points of the WebAssembly engine that a browser calls in place of the
// C ABI functions taking 64 bit integers. JavaScript numbers hold 53 bits, so
// each 64 bit argument arrives as its high and low 32 bit halves and is put
// back together here; the call then goes to the C ABI unchanged. Everything
// else the web binding uses is a C ABI function exported as is.

#include <cstdint>

#include "aviary_c.h"

namespace {

uint64_t join(uint32_t hi, uint32_t lo) {
  return (static_cast<uint64_t>(hi) << 32) | static_cast<uint64_t>(lo);
}

}  // namespace

extern "C" {

AVIARY_API AviaryBufferResult aviary_web_txid(
    uint8_t version, uint32_t chain_hi, uint32_t chain_lo,
    const uint8_t* body, size_t body_len,
    const uint8_t* author, size_t author_len,
    const uint8_t* signature, size_t sig_len) {
  return aviary_txid(version, join(chain_hi, chain_lo), body, body_len,
                     author, author_len, signature, sig_len);
}

AVIARY_API AviaryBufferResult aviary_web_header_hash(
    uint8_t version, uint32_t chain_hi, uint32_t chain_lo,
    uint32_t height_hi, uint32_t height_lo, const uint8_t* parent_hash,
    uint32_t timestamp_hi, uint32_t timestamp_lo,
    const uint8_t* tx_root, const uint8_t* state_root,
    const uint8_t* consensus_data, size_t consensus_len) {
  return aviary_header_hash(version, join(chain_hi, chain_lo),
                            join(height_hi, height_lo), parent_hash,
                            join(timestamp_hi, timestamp_lo), tx_root,
                            state_root, consensus_data, consensus_len);
}

AVIARY_API int aviary_web_verify_commit(
    uint32_t chain_hi, uint32_t chain_lo,
    const uint8_t* validator_set, size_t vset_len,
    const uint8_t* cert, size_t cert_len) {
  return aviary_verify_commit(join(chain_hi, chain_lo), validator_set,
                              vset_len, cert, cert_len);
}

}  // extern "C"
