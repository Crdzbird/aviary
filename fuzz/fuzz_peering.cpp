#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "aviary_c.h"

namespace {
using Bytes = std::vector<std::uint8_t>;

Bytes take(AviaryBufferResult r) {
  Bytes out;
  if (r.buf.data) {
    out.assign(r.buf.data, r.buf.data + r.buf.len);
    aviary_buffer_free(&r.buf);
  }
  if (r.error) aviary_str_free(r.error);
  return out;
}

std::pair<Bytes, Bytes> split(const Bytes& b) {
  if (b.size() < 4) return {};
  const std::size_t n = (std::size_t(b[0]) << 24) | (std::size_t(b[1]) << 16) | (std::size_t(b[2]) << 8) | b[3];
  if (4 + n > b.size()) return {};
  return {Bytes(b.begin() + 4, b.begin() + 4 + n), Bytes(b.begin() + 4 + n, b.end())};
}

struct Fixture {
  AviaryAddressBook book = nullptr;
  Bytes noise_a, noise_b, msg0;
  AviaryNoiseSession pending = nullptr;
  AviaryNoiseSession open = nullptr;
  Bytes policy, credential, nonce, wire;
  AviaryCredentialGate gate = nullptr;
  AviaryChain chain = nullptr;

  Fixture() {
    aviary_init();
    book = aviary_address_book_new();
    noise_a = take(aviary_noise_identity_generate());
    noise_b = take(aviary_noise_identity_generate());
    AviaryNoiseSession init = aviary_noise_initiate(noise_a.data(), noise_a.size(), nullptr, 0);
    msg0 = take(aviary_noise_handshake_message(init));
    AviaryNoiseSession resp = aviary_noise_accept(noise_b.data(), noise_b.size(), msg0.data(), msg0.size(), nullptr, 0);
    Bytes msg1 = take(aviary_noise_handshake_message(resp));
    Bytes msg2 = take(aviary_noise_complete_initiator(init, msg1.data(), msg1.size()));
    aviary_noise_complete_responder(resp, msg2.data(), msg2.size());
    Bytes proof_a = take(aviary_noise_identity_proof(init));
    take(aviary_noise_identity_proof(resp));
    aviary_noise_verify_identity_proof(resp, proof_a.data(), proof_a.size());
    pending = init;
    open = resp;

    Bytes km(32, 0x11);
    auto [ipk, isk] = split(take(aviary_bbs_keygen(km.data(), km.size())));
    const char* hdr = "aviary.credential.v1";
    policy = take(aviary_credential_policy_encode(7, ipk.data(), ipk.size(),
                                                  reinterpret_cast<const std::uint8_t*>(hdr), std::strlen(hdr), 4, 2));
    Bytes member(32, 0x77);
    credential = take(aviary_credential_issue(policy.data(), policy.size(), isk.data(), isk.size(),
                                              member.data(), member.size(), 3, 4102444800ULL, 1));
    gate = aviary_credential_gate_new(policy.data(), policy.size());
    Bytes ch = take(aviary_admission_challenge(5));
    if (ch.size() > 32) {
      nonce.assign(ch.begin(), ch.begin() + 32);
      wire.assign(ch.begin() + 32, ch.end());
    }

    Bytes ikm(32, 0x61);
    auto [bls_pub, bls_sec] = split(take(aviary_bls_keygen(ikm.data(), ikm.size())));
    const std::uint64_t power = 1;
    Bytes g = take(aviary_genesis_build(41, 1, 1, 2, nullptr, nullptr, 0, bls_pub.data(), &power, 1));
    Bytes bp = take(aviary_blueprint_encode("bft", "record", "", nullptr, 0));
    Bytes gb = take(aviary_genesis_commit_blueprint(g.data(), g.size(), bp.data(), bp.size()));
    chain = aviary_chain_assemble(gb.data(), gb.size(), 0, bls_pub.data(), bls_pub.size(), bls_sec.data(), bls_sec.size());
  }
};

Fixture& fixture() {
  static Fixture f;
  return f;
}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  Fixture& f = fixture();
  aviary_peer_record_verify(data, size);
  take(aviary_peer_record_fields(data, size));
  AviaryAddressBook decoded = aviary_address_book_decode(data, size);
  if (decoded) {
    take(aviary_address_book_encode(decoded));
    aviary_address_book_free(decoded);
  }
  AviaryAddressBook scratch = aviary_address_book_new();
  aviary_address_book_add(scratch, data, size);
  aviary_address_book_merge(scratch, data, size);
  aviary_address_book_free(scratch);
  aviary_address_book_merge(f.book, data, size);

  AviaryNoiseSession acc = aviary_noise_accept(f.noise_b.data(), f.noise_b.size(), data, size, nullptr, 0);
  if (acc) aviary_noise_free(acc);
  AviaryNoiseSession init = aviary_noise_initiate(f.noise_a.data(), f.noise_a.size(), data, size);
  if (init) {
    take(aviary_noise_complete_initiator(init, data, size));
    aviary_noise_free(init);
  }
  AviaryNoiseSession resp = aviary_noise_accept(f.noise_b.data(), f.noise_b.size(), f.msg0.data(), f.msg0.size(), nullptr, 0);
  if (resp) {
    aviary_noise_complete_responder(resp, data, size);
    aviary_noise_free(resp);
  }
  AviaryNoiseSession bad_id = aviary_noise_initiate(data, size, nullptr, 0);
  if (bad_id) aviary_noise_free(bad_id);
  aviary_noise_verify_identity_proof(f.pending, data, size);
  take(aviary_noise_decrypt(f.open, data, size));

  aviary_admission_granted(f.gate, 5, f.nonce.data(), f.nonce.size(), data, size, 1700000000ULL);
  take(aviary_admission_answer(f.policy.data(), f.policy.size(), f.credential.data(), f.credential.size(), data, size));
  take(aviary_admission_answer(f.policy.data(), f.policy.size(), data, size, f.wire.data(), f.wire.size()));

  take(aviary_record_from_proof(data, size));
  take(aviary_chain_add_tx(f.chain, data, size));
  if (size >= 32) {
    take(aviary_chain_record(f.chain, data));
    take(aviary_chain_prove_record(f.chain, data));
  }
  return 0;
}
