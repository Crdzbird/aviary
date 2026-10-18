// Drives the C ABI directly (the path every binding takes) for record signing
// from a key store and with hybrid keys, the chain queue's transaction events,
// the address book, the Noise session and the admission handshake.
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "aviary_c.h"
#include "check.hpp"

namespace {
using Bytes = std::vector<std::uint8_t>;

Bytes take(AviaryBufferResult r) {
  if (r.error) {
    std::printf("  error: %s\n", r.error);
    aviary_str_free(r.error);
    return {};
  }
  Bytes out(r.buf.data, r.buf.data + r.buf.len);
  aviary_buffer_free(&r.buf);
  return out;
}

std::string take_error(AviaryBufferResult r) {
  std::string e = r.error ? r.error : "";
  if (r.error) aviary_str_free(r.error);
  if (r.buf.data) aviary_buffer_free(&r.buf);
  return e;
}

std::pair<Bytes, Bytes> split(const Bytes& b) {
  const std::uint32_t n = (std::uint32_t(b[0]) << 24) | (std::uint32_t(b[1]) << 16) | (std::uint32_t(b[2]) << 8) | b[3];
  return {Bytes(b.begin() + 4, b.begin() + 4 + n), Bytes(b.begin() + 4 + n, b.end())};
}

std::vector<Bytes> blobs(const Bytes& b) {
  std::vector<Bytes> out;
  std::size_t o = 4;
  const std::uint32_t n = (std::uint32_t(b[0]) << 24) | (std::uint32_t(b[1]) << 16) | (std::uint32_t(b[2]) << 8) | b[3];
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint32_t l = (std::uint32_t(b[o]) << 24) | (std::uint32_t(b[o + 1]) << 16) | (std::uint32_t(b[o + 2]) << 8) | b[o + 3];
    o += 4;
    out.emplace_back(b.begin() + o, b.begin() + o + l);
    o += l;
  }
  return out;
}

Bytes cat(std::initializer_list<Bytes> parts) {
  Bytes out;
  auto put32 = [&](std::uint32_t v) { for (int s = 24; s >= 0; s -= 8) out.push_back(std::uint8_t(v >> s)); };
  put32(static_cast<std::uint32_t>(parts.size()));
  for (const auto& p : parts) { put32(static_cast<std::uint32_t>(p.size())); out.insert(out.end(), p.begin(), p.end()); }
  return out;
}

Bytes address_of(const Bytes& pk) { return take(aviary_address_of(pk.data(), pk.size())); }

AviaryChain record_chain(const Bytes& bls_pub, const Bytes& bls_sec) {
  const std::uint64_t power = 1;
  Bytes g = take(aviary_genesis_build(41, 1, 1, 2, nullptr, nullptr, 0, bls_pub.data(), &power, 1));
  Bytes bp = take(aviary_blueprint_encode("bft", "record", "", nullptr, 0));
  Bytes gb = take(aviary_genesis_commit_blueprint(g.data(), g.size(), bp.data(), bp.size()));
  return aviary_chain_assemble(gb.data(), gb.size(), 0, bls_pub.data(), bls_pub.size(), bls_sec.data(), bls_sec.size());
}

bool produce(AviaryChain c, std::uint64_t ts) {
  Bytes blk = take(aviary_chain_propose(c, ts));
  Bytes cert = take(aviary_chain_certify(c, blk.data(), blk.size()));
  return aviary_chain_submit(c, blk.data(), blk.size(), cert.data(), cert.size()) == 1;
}
}  // namespace

int main() {
  CHECK(aviary_init() == 0);

  // -- Records from a key store, Ed25519 and hybrid, plus queue events --------
  {
    Bytes ikm(32, 0x61);
    auto [bls_pub, bls_sec] = split(take(aviary_bls_keygen(ikm.data(), ikm.size())));
    AviaryChain c = record_chain(bls_pub, bls_sec);
    CHECK(c != nullptr);
    AviaryNode n = aviary_node_new(1);
    Bytes ed = take(aviary_node_new_account(n));
    Bytes ed_pub(ed.begin() + 32, ed.end());
    auto [hy_pub, hy_sec] = split(take(aviary_node_new_account_hybrid(n)));
    AviaryKeyStore ks = aviary_keystore_new("pass");
    std::uint8_t ed_addr[32], hy_addr[32];
    CHECK(aviary_keystore_add_account(ks, ed_pub.data(), ed_pub.size(), ed.data(), ed.size(), ed_addr) == AVIARY_OK);
    CHECK(aviary_keystore_add_account(ks, hy_pub.data(), hy_pub.size(), hy_sec.data(), hy_sec.size(), hy_addr) == AVIARY_OK);
    Bytes k1(32, 0x01), k2(32, 0x02), k3(32, 0x03);
    const char* v = "{\"status\":\"in service\"}";
    Bytes tx1 = take(aviary_keystore_sign_record(ks, 41, ed_addr, 0, k1.data(), 0, reinterpret_cast<const std::uint8_t*>(v), std::strlen(v)));
    Bytes tx2 = take(aviary_keystore_sign_record(ks, 41, hy_addr, 0, k2.data(), 0, reinterpret_cast<const std::uint8_t*>(v), std::strlen(v)));
    Bytes tx3 = take(aviary_record_tx_sign_hybrid(41, 1, k3.data(), 0, nullptr, 0, hy_pub.data(), hy_pub.size(), hy_sec.data(), hy_sec.size()));
    CHECK(!tx1.empty() && !tx2.empty() && !tx3.empty());
    CHECK(take(aviary_chain_add_tx(c, tx1.data(), tx1.size())).size() == 32);
    CHECK(take(aviary_chain_add_tx(c, tx2.data(), tx2.size())).size() == 32);
    CHECK(take(aviary_chain_add_tx(c, tx3.data(), tx3.size())).size() == 32);
    const std::string queued(reinterpret_cast<const char*>(take(aviary_chain_drain_events(c)).data()));
    CHECK(queued.find("tx_admitted") != std::string::npos && queued.find("\"mempool_size\":3") != std::string::npos);
    CHECK(produce(c, 2) && aviary_chain_pending_count(c) == 0);
    Bytes ev = take(aviary_chain_drain_events(c));
    const std::string after(ev.begin(), ev.end());
    CHECK(after.find("block_finalized") != std::string::npos && after.find("tx_removed") != std::string::npos);
    Bytes rec2 = take(aviary_chain_record(c, k2.data()));
    CHECK(rec2.size() > 44 && Bytes(rec2.begin(), rec2.begin() + 32) == address_of(hy_pub));
    CHECK(Bytes(rec2.begin(), rec2.begin() + 32) == Bytes(hy_addr, hy_addr + 32));
    CHECK(aviary_chain_record_nonce(c, hy_addr) == 2);
    Bytes unknown(32, 0xEE);
    CHECK(take_error(aviary_keystore_sign_record(ks, 41, unknown.data(), 0, k1.data(), 0, nullptr, 0)).find("unknown") != std::string::npos);
    aviary_keystore_lock(ks);
    CHECK(!take_error(aviary_keystore_sign_record(ks, 41, ed_addr, 1, k1.data(), 0, nullptr, 0)).empty());
    std::string metrics(reinterpret_cast<const char*>(take(aviary_chain_metrics(c)).data()));
    CHECK(metrics.find("aviary_mempool_size 0") != std::string::npos);
    aviary_keystore_free(ks);
    aviary_node_free(n);
    aviary_chain_free(c);
  }

  // -- Address book: signed records, trusted add, verified merge ---------------
  {
    Bytes alice = take(aviary_secure_identity_generate());
    Bytes bob = take(aviary_secure_identity_generate());
    auto a = blobs(alice), b = blobs(bob);
    Bytes ra = take(aviary_peer_record_sign(1, "10.0.0.1", 7001, a[0].data(), a[0].size(), a[2].data(), a[2].size(), nullptr, 0, a[3].data(), a[3].size()));
    Bytes rb = take(aviary_peer_record_sign(2, "10.0.0.2", 7002, b[0].data(), b[0].size(), b[2].data(), b[2].size(), nullptr, 0, b[3].data(), b[3].size()));
    CHECK(aviary_peer_record_verify(ra.data(), ra.size()) == 1);
    CHECK(!take_error(aviary_peer_record_sign(3, "x", 1, a[0].data(), a[0].size(), a[2].data(), a[2].size(), nullptr, 0, b[3].data(), b[3].size())).empty());
    auto f = blobs(take(aviary_peer_record_fields(rb.data(), rb.size())));
    CHECK(f.size() == 7 && f[0] == Bytes({0, 0, 0, 2}) && std::string(f[1].begin(), f[1].end()) == "10.0.0.2" && f[2] == Bytes({0x1B, 0x5A}));
    Bytes forged = rb;
    forged[forged.size() - 1] ^= 1;
    CHECK(aviary_peer_record_verify(forged.data(), forged.size()) == 0);

    AviaryAddressBook mine = aviary_address_book_new();
    CHECK(aviary_address_book_add(mine, ra.data(), ra.size()) == AVIARY_OK);
    AviaryAddressBook theirs = aviary_address_book_new();
    aviary_address_book_add(theirs, rb.data(), rb.size());
    aviary_address_book_add(theirs, ra.data(), ra.size());
    Bytes enc = take(aviary_address_book_encode(theirs));
    CHECK(aviary_address_book_merge(mine, enc.data(), enc.size()) == 1);
    CHECK(aviary_address_book_size(mine) == 2 && aviary_address_book_contains(mine, 2) == 1);
    CHECK(take(aviary_address_book_get(mine, 2)) == rb);
    CHECK(!take_error(aviary_address_book_get(mine, 9)).empty());
    CHECK(take(aviary_address_book_encode(mine)) == enc);
    CHECK(blobs(take(aviary_address_book_records(mine))).size() == 2);

    AviaryAddressBook fresh = aviary_address_book_new();
    AviaryAddressBook evil = aviary_address_book_new();
    aviary_address_book_add(evil, forged.data(), forged.size());
    Bytes evil_enc = take(aviary_address_book_encode(evil));
    CHECK(aviary_address_book_merge(fresh, evil_enc.data(), evil_enc.size()) == 0);
    Bytes junk{1, 2, 3};
    CHECK(aviary_address_book_merge(fresh, junk.data(), junk.size()) == -1);
    CHECK(aviary_address_book_decode(junk.data(), junk.size()) == nullptr);
    AviaryAddressBook back = aviary_address_book_decode(enc.data(), enc.size());
    CHECK(back != nullptr && aviary_address_book_size(back) == 2);
    for (AviaryAddressBook x : {mine, theirs, fresh, evil, back}) aviary_address_book_free(x);
  }

  // -- Noise session: three messages, identity proofs, records -----------------
  {
    Bytes ia = take(aviary_noise_identity_generate());
    Bytes ib = take(aviary_noise_identity_generate());
    CHECK(blobs(ia).size() == 4 && blobs(ia)[0].size() == 32);
    Bytes prologue{'a', 'v', 'i'};
    AviaryNoiseSession init = aviary_noise_initiate(ia.data(), ia.size(), prologue.data(), prologue.size());
    Bytes m0 = take(aviary_noise_handshake_message(init));
    AviaryNoiseSession resp = aviary_noise_accept(ib.data(), ib.size(), m0.data(), m0.size(), prologue.data(), prologue.size());
    CHECK(resp != nullptr);
    Bytes m1 = take(aviary_noise_handshake_message(resp));
    Bytes m2 = take(aviary_noise_complete_initiator(init, m1.data(), m1.size()));
    CHECK(aviary_noise_complete_responder(resp, m2.data(), m2.size()) == AVIARY_OK);
    const char* hi = "hello";
    CHECK(!take_error(aviary_noise_encrypt(init, reinterpret_cast<const std::uint8_t*>(hi), 5)).empty());
    Bytes pa = take(aviary_noise_identity_proof(init));
    Bytes pb = take(aviary_noise_identity_proof(resp));
    CHECK(aviary_noise_verify_identity_proof(resp, pa.data(), pa.size()) == AVIARY_OK);
    CHECK(aviary_noise_verify_identity_proof(init, pb.data(), pb.size()) == AVIARY_OK);
    CHECK(aviary_noise_open(init) == 1 && aviary_noise_open(resp) == 1);
    CHECK(take(aviary_noise_peer_identity(init)) == blobs(ib)[2]);
    CHECK(take(aviary_noise_peer_static(resp)) == blobs(ia)[0]);
    CHECK(take(aviary_noise_handshake_hash(init)) == take(aviary_noise_handshake_hash(resp)));
    Bytes rec = take(aviary_noise_encrypt(init, reinterpret_cast<const std::uint8_t*>(hi), 5));
    CHECK(take(aviary_noise_decrypt(resp, rec.data(), rec.size())) == Bytes(hi, hi + 5));
    rec[rec.size() - 1] ^= 1;
    CHECK(!take_error(aviary_noise_decrypt(resp, rec.data(), rec.size())).empty());
    Bytes other = {'x'};
    AviaryNoiseSession mismatch = aviary_noise_accept(ib.data(), ib.size(), m0.data(), m0.size(), other.data(), other.size());
    if (mismatch) {
      Bytes mm1 = take(aviary_noise_handshake_message(mismatch));
      AviaryNoiseSession init2 = aviary_noise_initiate(ia.data(), ia.size(), prologue.data(), prologue.size());
      CHECK(!take_error(aviary_noise_complete_initiator(init2, mm1.data(), mm1.size())).empty());
      aviary_noise_free(init2);
      aviary_noise_free(mismatch);
    }
    Bytes bad{9, 9};
    CHECK(aviary_noise_initiate(bad.data(), bad.size(), nullptr, 0) == nullptr);
    aviary_noise_free(init);
    aviary_noise_free(resp);
  }

  // -- Admission handshake --------------------------------------------------------
  {
    Bytes km(32, 0x11);
    auto [ipk, isk] = split(take(aviary_bbs_keygen(km.data(), km.size())));
    const char* hdr = "aviary.credential.v1";
    Bytes policy = take(aviary_credential_policy_encode(7, ipk.data(), ipk.size(), reinterpret_cast<const std::uint8_t*>(hdr), std::strlen(hdr), 4, 2));
    Bytes member(32, 0x77);
    Bytes cred = take(aviary_credential_issue(policy.data(), policy.size(), isk.data(), isk.size(), member.data(), member.size(), 3, 4102444800ULL, 1));
    AviaryCredentialGate gate = aviary_credential_gate_new(policy.data(), policy.size());
    Bytes ch = take(aviary_admission_challenge(5));
    CHECK(ch.size() > 32);
    Bytes nonce(ch.begin(), ch.begin() + 32), wire(ch.begin() + 32, ch.end());
    Bytes pres = take(aviary_admission_answer(policy.data(), policy.size(), cred.data(), cred.size(), wire.data(), wire.size()));
    CHECK(aviary_admission_granted(gate, 5, nonce.data(), nonce.size(), pres.data(), pres.size(), 1700000000ULL) == 1);
    CHECK(aviary_admission_granted(gate, 6, nonce.data(), nonce.size(), pres.data(), pres.size(), 1700000000ULL) == 0);
    Bytes other_nonce(32, 0);
    CHECK(aviary_admission_granted(gate, 5, other_nonce.data(), 32, pres.data(), pres.size(), 1700000000ULL) == 0);
    Bytes junk{1, 2};
    CHECK(aviary_admission_granted(gate, 5, nonce.data(), 32, junk.data(), junk.size(), 1700000000ULL) == 0);
    CHECK(!take_error(aviary_admission_answer(policy.data(), policy.size(), cred.data(), cred.size(), junk.data(), junk.size())).empty());
    aviary_credential_gate_free(gate);
  }
  return bctest::summary();
}
