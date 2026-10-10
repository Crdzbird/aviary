#include <optional>
#include "aviary/crypto.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "cryptolib_c.h"

namespace aviary::crypto {

Hash blake3_256(const std::uint8_t* data, std::size_t len) {
  // Pass a valid (non-null) pointer even for empty input, so the borrowed-input
  // contract holds regardless of how the backend treats (nullptr, 0).
  static const std::uint8_t kEmpty = 0;
  const std::uint8_t* ptr = (len == 0) ? &kEmpty : data;

  CryptoBufferResult r = cryptolib_blake3(ptr, len, 32);
  if (r.buf.data == nullptr) {
    std::string msg = r.error ? r.error : "cryptolib_blake3 failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error("blake3_256: " + msg);
  }
  if (r.buf.len != 32) {
    cryptolib_buffer_free(&r.buf);
    throw std::runtime_error("blake3_256: backend returned unexpected length");
  }

  Hash out{};
  std::copy(r.buf.data, r.buf.data + 32, out.begin());
  cryptolib_buffer_free(&r.buf);
  return out;
}

Hash blake3_256(const std::vector<std::uint8_t>& data) {
  return blake3_256(data.data(), data.size());
}

Hash hash_tagged(Domain tag, const std::vector<std::uint8_t>& data) {
  std::vector<std::uint8_t> buf;
  buf.reserve(1 + data.size());
  buf.push_back(static_cast<std::uint8_t>(tag));
  buf.insert(buf.end(), data.begin(), data.end());
  return blake3_256(buf);
}

Hash hash_node(const Hash& left, const Hash& right) {
  std::vector<std::uint8_t> buf;
  buf.reserve(1 + left.size() + right.size());
  buf.push_back(static_cast<std::uint8_t>(Domain::kNode));
  buf.insert(buf.end(), left.begin(), left.end());
  buf.insert(buf.end(), right.begin(), right.end());
  return blake3_256(buf);
}

Hash zero_hash() { return Hash{}; }

// ── Signatures ───────────────────────────────────────────────────────────────

namespace {
Bytes take_buffer(CryptoBuffer& b) {
  Bytes out(b.data, b.data + b.len);
  cryptolib_buffer_free(&b);
  return out;
}
}  // namespace

Bytes random_bytes(std::size_t n) {
  CryptoBufferResult r = cryptolib_random_bytes(n);
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "random_bytes failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

KeyPair ed25519_keygen() {
  CryptoKeyPair kp = cryptolib_ed25519_keygen();
  KeyPair out{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
  return out;
}

KeyPair ed25519_keygen_from_seed(ByteView seed) {
  CryptoKeyPair kp = cryptolib_ed25519_keygen_from_seed(seed.data(), seed.size());
  KeyPair out{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
  return out;
}

Bytes ed25519_sign(ByteView msg, ByteView secret_key) {
  CryptoBufferResult r =
      cryptolib_ed25519_sign(msg.data(), msg.size(), secret_key.data(), secret_key.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "ed25519_sign failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

bool ed25519_verify(ByteView msg, ByteView sig, ByteView public_key) {
  return cryptolib_ed25519_verify(msg.data(), msg.size(), sig.data(), sig.size(),
                                  public_key.data(), public_key.size()) == 1;
}

Hash address_of(ByteView public_key) {
  return blake3_256(public_key.data(), public_key.size());
}

// ── Hybrid signature: Ed25519 + ML-DSA-65 ────────────────────────────────────

KeyPair hybrid_sig_keygen() {
  CryptoKeyPair kp = cryptolib_hybrid_sig_keygen();
  KeyPair out{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
  return out;
}

Bytes hybrid_sig_sign(ByteView msg, ByteView secret_key) {
  CryptoBufferResult r =
      cryptolib_hybrid_sig_sign(msg.data(), msg.size(), secret_key.data(), secret_key.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "hybrid_sig_sign failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

bool hybrid_sig_verify(ByteView msg, ByteView sig, ByteView public_key) {
  return cryptolib_hybrid_sig_verify(msg.data(), msg.size(), sig.data(), sig.size(),
                                     public_key.data(), public_key.size()) == 1;
}

// ── ECVRF (RFC 9381) ─────────────────────────────────────────────────────────

KeyPair ecvrf_keygen() {
  CryptoKeyPair kp = cryptolib_ecvrf_keygen();
  KeyPair out{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
  return out;
}

Bytes ecvrf_public_key(ByteView sk) {
  CryptoBufferResult r = cryptolib_ecvrf_public_key(sk.data(), sk.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "ecvrf_public_key failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

Bytes ecvrf_prove(ByteView sk, ByteView alpha) {
  CryptoBufferResult r = cryptolib_ecvrf_prove(sk.data(), sk.size(), alpha.data(), alpha.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "ecvrf_prove failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

Bytes ecvrf_proof_to_hash(ByteView pi) {
  CryptoBufferResult r = cryptolib_ecvrf_proof_to_hash(pi.data(), pi.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "ecvrf_proof_to_hash failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

std::optional<Bytes> ecvrf_verify(ByteView pk, ByteView alpha, ByteView pi) {
  CryptoBufferResult r =
      cryptolib_ecvrf_verify(pk.data(), pk.size(), alpha.data(), alpha.size(), pi.data(), pi.size());
  if (r.buf.data == nullptr) {
    if (r.error) cryptolib_str_free(r.error);
    return std::nullopt;  // invalid proof — never throws
  }
  return take_buffer(r.buf);
}

// ── PQ forward-secret ratchet (cryptolib Session) ────────────────────────────

KeyPair Ratchet::generate_prekey() {
  CryptoKeyPair kp = cryptolib_session_generate_prekey();
  return KeyPair{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
}

std::pair<Ratchet, Bytes> Ratchet::initiate(ByteView responder_prekey_pub) {
  char* err = nullptr;
  CryptoSession h = cryptolib_session_initiate(responder_prekey_pub.data(),
                                               responder_prekey_pub.size(), &err);
  if (h == nullptr) {
    std::string m = err ? err : "session_initiate failed";
    if (err) cryptolib_str_free(err);
    throw std::runtime_error(m);
  }
  CryptoBufferResult r = cryptolib_session_handshake(h);
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "session_handshake failed";
    if (r.error) cryptolib_str_free(r.error);
    cryptolib_session_free(h);
    throw std::runtime_error(m);
  }
  return {Ratchet(h), take_buffer(r.buf)};
}

Ratchet Ratchet::accept(ByteView handshake, ByteView prekey_pub, ByteView prekey_sec) {
  char* err = nullptr;
  CryptoSession h = cryptolib_session_accept(handshake.data(), handshake.size(), prekey_pub.data(),
                                             prekey_pub.size(), prekey_sec.data(), prekey_sec.size(),
                                             &err);
  if (h == nullptr) {
    std::string m = err ? err : "session_accept failed";
    if (err) cryptolib_str_free(err);
    throw std::runtime_error(m);
  }
  return Ratchet(h);
}

Bytes Ratchet::encrypt(ByteView plaintext, ByteView aad) {
  CryptoBufferResult r = cryptolib_session_encrypt(h_, plaintext.data(), plaintext.size(), aad.data(),
                                                   aad.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "session_encrypt failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

Bytes Ratchet::decrypt(ByteView frame, ByteView aad) {
  CryptoBufferResult r = cryptolib_session_decrypt(h_, frame.data(), frame.size(), aad.data(),
                                                   aad.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "session_decrypt failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

Ratchet::Ratchet(Ratchet&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
Ratchet& Ratchet::operator=(Ratchet&& o) noexcept {
  if (this != &o) {
    if (h_) cryptolib_session_free(h_);
    h_ = o.h_;
    o.h_ = nullptr;
  }
  return *this;
}
Ratchet::~Ratchet() {
  if (h_) cryptolib_session_free(h_);
}

// ── Noise XX ─────────────────────────────────────────────────────────────────

namespace {
const std::uint8_t kNoiseEmpty = 0;
// Borrowed-input contract: hand the backend a valid pointer even for 0 bytes.
const std::uint8_t* nz(ByteView v) { return v.empty() ? &kNoiseEmpty : v.data(); }
Bytes noise_take(CryptoBufferResult r, const char* op) {
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(std::string(op) + ": " + m);
  }
  return take_buffer(r.buf);
}
}  // namespace

KeyPair x25519_keygen() {
  CryptoKeyPair kp = cryptolib_x25519_keygen();
  if (kp.public_key.data == nullptr || kp.secret_key.data == nullptr) {
    cryptolib_keypair_free(&kp);
    throw std::runtime_error("x25519_keygen failed");
  }
  return KeyPair{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
}

Noise::Noise(bool initiator, ByteView static_pub, ByteView static_sec, ByteView prologue)
    : h_(cryptolib_noise_create(initiator ? 1 : 0, static_pub.data(), static_pub.size(),
                                static_sec.data(), static_sec.size(), nz(prologue), prologue.size())) {
  if (h_ == nullptr) throw std::runtime_error("noise_create: bad static key");
}
Bytes Noise::write_message(ByteView payload) {
  return noise_take(cryptolib_noise_write_message(h_, nz(payload), payload.size()), "noise_write_message");
}
Bytes Noise::read_message(ByteView message) {
  return noise_take(cryptolib_noise_read_message(h_, nz(message), message.size()), "noise_read_message");
}
bool Noise::handshake_finished() const { return h_ != nullptr && cryptolib_noise_handshake_finished(h_) == 1; }
Hash Noise::handshake_hash() const {
  Bytes b = noise_take(cryptolib_noise_handshake_hash(h_), "noise_handshake_hash");
  if (b.size() != 32) throw std::runtime_error("noise_handshake_hash: unexpected length");
  Hash out{};
  std::copy(b.begin(), b.end(), out.begin());
  return out;
}
Bytes Noise::remote_static() const { return noise_take(cryptolib_noise_remote_static(h_), "noise_remote_static"); }
void Noise::split() {
  if (cryptolib_noise_split(h_) != 1) throw std::runtime_error("noise_split: handshake not finished or already split");
}
Bytes Noise::encrypt(ByteView plaintext, ByteView ad) {
  return noise_take(cryptolib_noise_encrypt(h_, nz(plaintext), plaintext.size(), nz(ad), ad.size()), "noise_encrypt");
}
Bytes Noise::decrypt(ByteView record, ByteView ad) {
  return noise_take(cryptolib_noise_decrypt(h_, nz(record), record.size(), nz(ad), ad.size()), "noise_decrypt");
}
Noise::Noise(Noise&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
Noise& Noise::operator=(Noise&& o) noexcept {
  if (this != &o) {
    if (h_) cryptolib_noise_free(h_);
    h_ = o.h_;
    o.h_ = nullptr;
  }
  return *this;
}
Noise::~Noise() {
  if (h_) cryptolib_noise_free(h_);
}

// ── SLH-DSA (FIPS 205) — fixed SLH-DSA-SHA2-128s profile ────────────────────

namespace {
constexpr int kSlhLevel = 0;       // 128s: smallest signature, slowest signing
constexpr int kSlhHashFamily = 0;  // SHA-2
}  // namespace

KeyPair slh_dsa_keygen() {
  CryptoKeyPair kp = cryptolib_slh_dsa_keygen(kSlhLevel, kSlhHashFamily);
  KeyPair out{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
  return out;
}

Bytes slh_dsa_sign(ByteView msg, ByteView secret_key) {
  CryptoBufferResult r = cryptolib_slh_dsa_sign(msg.data(), msg.size(), secret_key.data(),
                                                secret_key.size(), kSlhLevel, kSlhHashFamily);
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "slh_dsa_sign failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

bool slh_dsa_verify(ByteView msg, ByteView sig, ByteView public_key) {
  return cryptolib_slh_dsa_verify(msg.data(), msg.size(), sig.data(), sig.size(),
                                  public_key.data(), public_key.size(), kSlhLevel,
                                  kSlhHashFamily) == 1;
}

// ── OPAQUE aPAKE ─────────────────────────────────────────────────────────────

namespace {
// Consume a CryptoBuffer field, throwing with [op] context if it's null.
Bytes field_or_throw(CryptoBuffer& b, char** err, const char* op) {
  if (b.data == nullptr) {
    std::string m = err && *err ? *err : std::string(op) + " failed";
    if (err && *err) cryptolib_str_free(*err);
    throw std::runtime_error(m);
  }
  return take_buffer(b);
}
}  // namespace

KeyPair oprf_derive_keypair(ByteView seed, ByteView info) {
  CryptoKeyPair kp = cryptolib_oprf_derive_keypair(seed.data(), seed.size(), info.data(), info.size());
  return KeyPair{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
}

OpaqueRegRequest opaque_registration_request(ByteView password) {
  CryptoOprfBlind b = cryptolib_opaque_registration_request(password.data(), password.size());
  if (b.blind.data == nullptr) {
    std::string m = b.error ? b.error : "opaque_registration_request failed";
    if (b.error) cryptolib_str_free(b.error);
    throw std::runtime_error(m);
  }
  OpaqueRegRequest out{take_buffer(b.blind), take_buffer(b.blinded_element)};
  return out;
}

Bytes opaque_registration_response(ByteView request, ByteView server_public_key,
                                   ByteView credential_id, ByteView oprf_seed) {
  CryptoBufferResult r = cryptolib_opaque_registration_response(
      request.data(), request.size(), server_public_key.data(), server_public_key.size(),
      credential_id.data(), credential_id.size(), oprf_seed.data(), oprf_seed.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "opaque_registration_response failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

OpaqueRecord opaque_finalize_request(ByteView password, ByteView blind, ByteView response,
                                     ByteView server_identity, ByteView client_identity) {
  CryptoOpaqueRecord r = cryptolib_opaque_finalize_request(
      password.data(), password.size(), blind.data(), blind.size(), response.data(), response.size(),
      server_identity.data(), server_identity.size(), client_identity.data(), client_identity.size());
  OpaqueRecord out;
  out.record = field_or_throw(r.record, &r.error, "opaque_finalize_request");
  out.export_key = take_buffer(r.export_key);
  return out;
}

OpaqueKe1 opaque_client_init(ByteView password) {
  CryptoOpaqueKe1 k = cryptolib_opaque_client_init(password.data(), password.size());
  OpaqueKe1 out;
  out.ke1 = field_or_throw(k.ke1, &k.error, "opaque_client_init");
  out.client_state = take_buffer(k.client_state);
  return out;
}

OpaqueKe2 opaque_server_respond(ByteView context, ByteView server_private_key,
                                ByteView server_public_key, ByteView record, ByteView credential_id,
                                ByteView oprf_seed, ByteView ke1, ByteView server_identity,
                                ByteView client_identity) {
  CryptoOpaqueKe2 k = cryptolib_opaque_server_respond(
      context.data(), context.size(), server_private_key.data(), server_private_key.size(),
      server_public_key.data(), server_public_key.size(), record.data(), record.size(),
      credential_id.data(), credential_id.size(), oprf_seed.data(), oprf_seed.size(), ke1.data(),
      ke1.size(), server_identity.data(), server_identity.size(), client_identity.data(),
      client_identity.size());
  OpaqueKe2 out;
  out.ke2 = field_or_throw(k.ke2, &k.error, "opaque_server_respond");
  out.server_state = take_buffer(k.server_state);
  return out;
}

OpaqueLogin opaque_client_finish(ByteView client_state, ByteView ke2, ByteView context,
                                 ByteView server_identity, ByteView client_identity) {
  CryptoOpaqueKe3 k = cryptolib_opaque_client_finish(
      client_state.data(), client_state.size(), ke2.data(), ke2.size(), context.data(),
      context.size(), server_identity.data(), server_identity.size(), client_identity.data(),
      client_identity.size());
  OpaqueLogin out;
  out.ke3 = field_or_throw(k.ke3, &k.error, "opaque_client_finish (wrong password?)");
  out.session_key = take_buffer(k.session_key);
  out.export_key = take_buffer(k.export_key);
  return out;
}

Bytes opaque_server_finish(ByteView server_state, ByteView ke3) {
  CryptoBufferResult r =
      cryptolib_opaque_server_finish(server_state.data(), server_state.size(), ke3.data(), ke3.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "opaque_server_finish (client auth failed)";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

// ── BBS signatures ───────────────────────────────────────────────────────────

namespace {
// The parallel (pointers[], lengths[]) arrays cryptolib_bbs_* expect, built from
// a vector of messages. Empty vector → count 0 (nullptr data is fine).
struct MsgArrays {
  std::vector<const std::uint8_t*> ptrs;
  std::vector<std::size_t> lens;
};
MsgArrays msg_arrays(const std::vector<Bytes>& msgs) {
  MsgArrays a;
  a.ptrs.reserve(msgs.size());
  a.lens.reserve(msgs.size());
  for (const auto& m : msgs) {
    a.ptrs.push_back(m.data());
    a.lens.push_back(m.size());
  }
  return a;
}
}  // namespace

KeyPair bbs_keygen(ByteView key_material, ByteView key_info) {
  CryptoKeyPair kp = cryptolib_bbs_keygen(key_material.data(), key_material.size(), key_info.data(),
                                          key_info.size());
  if (kp.public_key.data == nullptr) throw std::runtime_error("bbs_keygen failed");
  return KeyPair{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
}

Bytes bbs_sk_to_pk(ByteView sk) {
  CryptoBufferResult r = cryptolib_bbs_sk_to_pk(sk.data(), sk.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "bbs_sk_to_pk failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

Bytes bbs_sign(ByteView sk, ByteView pk, ByteView header, const std::vector<Bytes>& msgs) {
  MsgArrays a = msg_arrays(msgs);
  CryptoBufferResult r =
      cryptolib_bbs_sign(sk.data(), sk.size(), pk.data(), pk.size(), header.data(), header.size(),
                         a.ptrs.data(), a.lens.data(), a.ptrs.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "bbs_sign failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

bool bbs_verify(ByteView pk, ByteView signature, ByteView header, const std::vector<Bytes>& msgs) {
  MsgArrays a = msg_arrays(msgs);
  return cryptolib_bbs_verify(pk.data(), pk.size(), signature.data(), signature.size(),
                              header.data(), header.size(), a.ptrs.data(), a.lens.data(),
                              a.ptrs.size()) == 1;
}

Bytes bbs_proof_gen(ByteView pk, ByteView signature, ByteView header, ByteView ph,
                    const std::vector<Bytes>& msgs,
                    const std::vector<std::uint64_t>& disclosed_indexes) {
  MsgArrays a = msg_arrays(msgs);
  CryptoBufferResult r = cryptolib_bbs_proof_gen(
      pk.data(), pk.size(), signature.data(), signature.size(), header.data(), header.size(),
      ph.data(), ph.size(), a.ptrs.data(), a.lens.data(), a.ptrs.size(), disclosed_indexes.data(),
      disclosed_indexes.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "bbs_proof_gen failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

bool bbs_proof_verify(ByteView pk, ByteView proof, ByteView header, ByteView ph,
                      const std::vector<Bytes>& disclosed_msgs,
                      const std::vector<std::uint64_t>& disclosed_indexes) {
  MsgArrays a = msg_arrays(disclosed_msgs);
  return cryptolib_bbs_proof_verify(pk.data(), pk.size(), proof.data(), proof.size(), header.data(),
                                    header.size(), ph.data(), ph.size(), a.ptrs.data(),
                                    a.lens.data(), a.ptrs.size(), disclosed_indexes.data(),
                                    disclosed_indexes.size()) == 1;
}

// ── BBS per-verifier pseudonyms + blind issuance ─────────────────────────────

namespace {
// Split a flat buffer of concatenated 32-byte scalars into a vector of scalars.
std::vector<Bytes> split_scalars(const Bytes& flat) {
  std::vector<Bytes> out;
  for (std::size_t off = 0; off + 32 <= flat.size(); off += 32)
    out.emplace_back(flat.begin() + off, flat.begin() + off + 32);
  return out;
}

// A CryptoBufferResult return + a CryptoBuffer out-param, checked and taken.
[[noreturn]] void throw_result(CryptoBufferResult& r, const char* op) {
  std::string m = r.error ? r.error : std::string(op) + " failed";
  if (r.error) cryptolib_str_free(r.error);
  throw std::runtime_error(m);
}
}  // namespace

BbsCommitment bbs_commit_with_nym(const std::vector<Bytes>& committed_msgs,
                                  const std::vector<Bytes>& prover_nyms) {
  MsgArrays c = msg_arrays(committed_msgs);
  MsgArrays n = msg_arrays(prover_nyms);
  CryptoBuffer spb{};
  CryptoBufferResult r = cryptolib_bbs_commit_with_nym(c.ptrs.data(), c.lens.data(), c.ptrs.size(),
                                                       n.ptrs.data(), n.lens.data(), n.ptrs.size(),
                                                       &spb);
  if (r.buf.data == nullptr) throw_result(r, "bbs_commit_with_nym");
  return BbsCommitment{take_buffer(r.buf), take_buffer(spb)};
}

Bytes bbs_blind_sign_with_nym(ByteView sk, ByteView pk, ByteView commitment, ByteView header,
                              const std::vector<Bytes>& msgs, ByteView signer_nym_entropy,
                              std::uint64_t length_nym_vector) {
  MsgArrays m = msg_arrays(msgs);
  CryptoBufferResult r = cryptolib_bbs_blind_sign_with_nym(
      sk.data(), sk.size(), pk.data(), pk.size(), commitment.data(), commitment.size(),
      header.data(), header.size(), m.ptrs.data(), m.lens.data(), m.ptrs.size(),
      signer_nym_entropy.data(), signer_nym_entropy.size(), length_nym_vector);
  if (r.buf.data == nullptr) throw_result(r, "bbs_blind_sign_with_nym");
  return take_buffer(r.buf);
}

std::vector<Bytes> bbs_finalize_nym_secrets(const std::vector<Bytes>& prover_nyms,
                                            ByteView signer_nym_entropy) {
  MsgArrays n = msg_arrays(prover_nyms);
  CryptoBufferResult r = cryptolib_bbs_finalize_nym_secrets(
      n.ptrs.data(), n.lens.data(), n.ptrs.size(), signer_nym_entropy.data(),
      signer_nym_entropy.size());
  if (r.buf.data == nullptr) throw_result(r, "bbs_finalize_nym_secrets");
  return split_scalars(take_buffer(r.buf));
}

Bytes bbs_calculate_pseudonym(ByteView context_id, const std::vector<Bytes>& nym_secrets) {
  MsgArrays n = msg_arrays(nym_secrets);
  CryptoBufferResult r = cryptolib_bbs_calculate_pseudonym(
      context_id.data(), context_id.size(), n.ptrs.data(), n.lens.data(), n.ptrs.size());
  if (r.buf.data == nullptr) throw_result(r, "bbs_calculate_pseudonym");
  return take_buffer(r.buf);
}

BbsPseudonymProof bbs_proof_gen_with_pseudonym(
    ByteView pk, ByteView signature, ByteView header, ByteView ph, ByteView context_id,
    const std::vector<Bytes>& signer_msgs, const std::vector<Bytes>& committed_msgs,
    ByteView secret_prover_blind, const std::vector<Bytes>& nym_secrets,
    const std::vector<std::uint64_t>& disclosed_signer_indexes,
    const std::vector<std::uint64_t>& disclosed_committed_indexes) {
  MsgArrays s = msg_arrays(signer_msgs);
  MsgArrays c = msg_arrays(committed_msgs);
  MsgArrays n = msg_arrays(nym_secrets);
  CryptoBuffer nym_out{};
  CryptoBufferResult r = cryptolib_bbs_proof_gen_with_pseudonym(
      pk.data(), pk.size(), signature.data(), signature.size(), header.data(), header.size(),
      ph.data(), ph.size(), context_id.data(), context_id.size(), s.ptrs.data(), s.lens.data(),
      s.ptrs.size(), c.ptrs.data(), c.lens.data(), c.ptrs.size(), secret_prover_blind.data(),
      secret_prover_blind.size(), n.ptrs.data(), n.lens.data(), n.ptrs.size(),
      disclosed_signer_indexes.data(), disclosed_signer_indexes.size(),
      disclosed_committed_indexes.data(), disclosed_committed_indexes.size(), &nym_out);
  if (r.buf.data == nullptr) throw_result(r, "bbs_proof_gen_with_pseudonym");
  return BbsPseudonymProof{take_buffer(r.buf), take_buffer(nym_out)};
}

bool bbs_proof_verify_with_pseudonym(ByteView pk, ByteView proof, ByteView header, ByteView ph,
                                     ByteView context_id, ByteView pseudonym, std::uint64_t L,
                                     std::uint64_t length_nym_vector,
                                     const std::vector<Bytes>& disclosed_msgs,
                                     const std::vector<std::uint64_t>& disclosed_indexes) {
  MsgArrays d = msg_arrays(disclosed_msgs);
  return cryptolib_bbs_proof_verify_with_pseudonym(
             pk.data(), pk.size(), proof.data(), proof.size(), header.data(), header.size(),
             ph.data(), ph.size(), context_id.data(), context_id.size(), pseudonym.data(),
             pseudonym.size(), L, length_nym_vector, d.ptrs.data(), d.lens.data(), d.ptrs.size(),
             disclosed_indexes.data(), disclosed_indexes.size()) == 1;
}

BbsCommitment bbs_blind_commit(const std::vector<Bytes>& committed_msgs) {
  MsgArrays c = msg_arrays(committed_msgs);
  CryptoBuffer spb{};
  CryptoBufferResult r =
      cryptolib_bbs_blind_commit(c.ptrs.data(), c.lens.data(), c.ptrs.size(), &spb);
  if (r.buf.data == nullptr) throw_result(r, "bbs_blind_commit");
  return BbsCommitment{take_buffer(r.buf), take_buffer(spb)};
}

Bytes bbs_blind_sign(ByteView sk, ByteView pk, ByteView commitment, ByteView header,
                     const std::vector<Bytes>& msgs) {
  MsgArrays m = msg_arrays(msgs);
  CryptoBufferResult r = cryptolib_bbs_blind_sign(sk.data(), sk.size(), pk.data(), pk.size(),
                                                  commitment.data(), commitment.size(),
                                                  header.data(), header.size(), m.ptrs.data(),
                                                  m.lens.data(), m.ptrs.size());
  if (r.buf.data == nullptr) throw_result(r, "bbs_blind_sign");
  return take_buffer(r.buf);
}

bool bbs_verify_blind_sign(ByteView pk, ByteView signature, ByteView header,
                           const std::vector<Bytes>& msgs, const std::vector<Bytes>& committed_msgs,
                           ByteView secret_prover_blind) {
  MsgArrays m = msg_arrays(msgs);
  MsgArrays c = msg_arrays(committed_msgs);
  return cryptolib_bbs_verify_blind_sign(pk.data(), pk.size(), signature.data(), signature.size(),
                                         header.data(), header.size(), m.ptrs.data(), m.lens.data(),
                                         m.ptrs.size(), c.ptrs.data(), c.lens.data(), c.ptrs.size(),
                                         secret_prover_blind.data(), secret_prover_blind.size()) == 1;
}

Bytes bbs_hash_to_scalar(ByteView msg, ByteView dst) {
  CryptoBufferResult r =
      cryptolib_bbs_hash_to_scalar(msg.data(), msg.size(), dst.data(), dst.size());
  if (r.buf.data == nullptr) throw_result(r, "bbs_hash_to_scalar");
  return take_buffer(r.buf);
}

Bytes bbs_random_scalar() {
  CryptoBufferResult r = cryptolib_bbs_random_scalar();
  if (r.buf.data == nullptr) throw_result(r, "bbs_random_scalar");
  return take_buffer(r.buf);
}

// ── BLS12-381 ────────────────────────────────────────────────────────────────

KeyPair bls_keygen_from_ikm(ByteView ikm) {
  CryptoKeyPair kp = cryptolib_bls_keygen_from_ikm(ikm.data(), ikm.size());
  KeyPair out{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
  return out;
}

Bytes bls_sign(ByteView msg, ByteView secret_key) {
  CryptoBufferResult r =
      cryptolib_bls_sign(msg.data(), msg.size(), secret_key.data(), secret_key.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "bls_sign failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

bool bls_verify(ByteView msg, ByteView sig, ByteView public_key) {
  return cryptolib_bls_verify(msg.data(), msg.size(), sig.data(), sig.size(),
                              public_key.data(), public_key.size()) == 1;
}

Bytes bls_aggregate(const std::vector<Bytes>& sigs) {
  if (sigs.empty()) throw std::runtime_error("bls_aggregate: no signatures");
  std::vector<const std::uint8_t*> ptrs;
  std::vector<std::size_t> lens;
  ptrs.reserve(sigs.size());
  lens.reserve(sigs.size());
  for (const auto& s : sigs) {
    ptrs.push_back(s.data());
    lens.push_back(s.size());
  }
  CryptoBufferResult r = cryptolib_bls_aggregate(ptrs.data(), lens.data(), sigs.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "bls_aggregate failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

bool bls_aggregate_verify(const std::vector<Bytes>& msgs,
                          const std::vector<Bytes>& pubkeys, ByteView agg_sig) {
  if (msgs.size() != pubkeys.size() || msgs.empty()) return false;
  std::vector<const std::uint8_t*> mptr, pptr;
  std::vector<std::size_t> mlen, plen;
  const std::size_t n = msgs.size();
  mptr.reserve(n); pptr.reserve(n); mlen.reserve(n); plen.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    mptr.push_back(msgs[i].data());
    mlen.push_back(msgs[i].size());
    pptr.push_back(pubkeys[i].data());
    plen.push_back(pubkeys[i].size());
  }
  return cryptolib_bls_aggregate_verify(mptr.data(), mlen.data(), pptr.data(),
                                        plen.data(), n, agg_sig.data(),
                                        agg_sig.size()) == 1;
}

// ── Hybrid KEM + SecretStream ────────────────────────────────────────────────

Bytes derive_key(const std::string& context, ByteView ikm) {
  CryptoBufferResult r =
      cryptolib_blake3_derive_key(context.c_str(), ikm.data(), ikm.size(), 32);
  if (r.buf.data == nullptr) {
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error("derive_key failed");
  }
  return take_buffer(r.buf);
}

KeyPair hybrid_kem_keygen() {
  CryptoKeyPair kp = cryptolib_hybrid_kem_keygen();
  return KeyPair{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
}

KemEncaps hybrid_kem_encapsulate(ByteView public_key) {
  char* err = nullptr;
  CryptoKemEncapsResult r =
      cryptolib_hybrid_kem_encapsulate(public_key.data(), public_key.size(), &err);
  if (r.ciphertext.data == nullptr || r.shared_secret.data == nullptr) {
    std::string m = err ? err : "hybrid_kem_encapsulate failed";
    if (err) cryptolib_str_free(err);
    cryptolib_kem_encaps_free(&r);
    throw std::runtime_error(m);
  }
  KemEncaps out{take_buffer(r.ciphertext), take_buffer(r.shared_secret)};
  return out;
}

Bytes hybrid_kem_decapsulate(ByteView ciphertext, ByteView secret_key) {
  CryptoBufferResult r = cryptolib_hybrid_kem_decapsulate(
      ciphertext.data(), ciphertext.size(), secret_key.data(), secret_key.size());
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : "hybrid_kem_decapsulate failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}

// ── Recipient-bound PQ seals (cryptolib Suite) ───────────────────────────────

namespace {
Bytes take_or_throw(CryptoBufferResult r, const char* op) {
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : std::string(op) + " failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}
}  // namespace

Bytes pq_seal_to(ByteView plaintext, ByteView recipient_kem_public, ByteView aad) {
  return take_or_throw(cryptolib_suite_seal_pq(plaintext.data(), plaintext.size(),
                                               recipient_kem_public.data(),
                                               recipient_kem_public.size(), aad.data(), aad.size()),
                       "pq_seal_to");
}

Bytes pq_open(ByteView envelope, ByteView recipient_kem_secret, ByteView aad) {
  return take_or_throw(cryptolib_suite_open_pq(envelope.data(), envelope.size(),
                                               recipient_kem_secret.data(),
                                               recipient_kem_secret.size(), aad.data(), aad.size()),
                       "pq_open");
}

Bytes pq_seal_signed_to(ByteView plaintext, ByteView recipient_kem_public, ByteView signer_sig_secret,
                        ByteView aad) {
  return take_or_throw(
      cryptolib_suite_seal_signed_pq(plaintext.data(), plaintext.size(), recipient_kem_public.data(),
                                     recipient_kem_public.size(), signer_sig_secret.data(),
                                     signer_sig_secret.size(), aad.data(), aad.size()),
      "pq_seal_signed_to");
}

Bytes pq_open_signed(ByteView envelope, ByteView recipient_kem_secret, ByteView signer_sig_public,
                     ByteView aad) {
  return take_or_throw(
      cryptolib_suite_open_signed_pq(envelope.data(), envelope.size(), recipient_kem_secret.data(),
                                     recipient_kem_secret.size(), signer_sig_public.data(),
                                     signer_sig_public.size(), aad.data(), aad.size()),
      "pq_open_signed");
}

// ── Interop primitives ───────────────────────────────────────────────────────

namespace {
Bytes require_buffer(CryptoBufferResult r, const char* what) {
  if (r.buf.data == nullptr) {
    std::string m = r.error ? r.error : what;
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error(m);
  }
  return take_buffer(r.buf);
}
}  // namespace

Hash sha256(ByteView msg) {
  Bytes b = require_buffer(cryptolib_sha256(msg.data(), msg.size()), "sha256 failed");
  if (b.size() != 32) throw std::runtime_error("sha256: unexpected length");
  Hash h{};
  std::copy(b.begin(), b.end(), h.begin());
  return h;
}

Hash keccak256(ByteView msg) {
  Bytes b = require_buffer(cryptolib_keccak256(msg.data(), msg.size()), "keccak256 failed");
  if (b.size() != 32) throw std::runtime_error("keccak256: unexpected length");
  Hash h{};
  std::copy(b.begin(), b.end(), h.begin());
  return h;
}

Bytes ripemd160(ByteView msg) {
  return require_buffer(cryptolib_ripemd160(msg.data(), msg.size()), "ripemd160 failed");
}

KeyPair secp256k1_keygen() {
  CryptoKeyPair kp = cryptolib_secp256k1_keygen();
  return KeyPair{take_buffer(kp.public_key), take_buffer(kp.secret_key)};
}

Bytes secp256k1_pubkey(ByteView secret_key, bool compressed) {
  return require_buffer(
      cryptolib_secp256k1_pubkey(secret_key.data(), secret_key.size(), compressed ? 1 : 0),
      "secp256k1_pubkey failed");
}

Bytes secp256k1_sign(ByteView digest32, ByteView secret_key) {
  return require_buffer(
      cryptolib_secp256k1_sign(digest32.data(), secret_key.data(), secret_key.size()),
      "secp256k1_sign failed");
}

bool secp256k1_verify(ByteView digest32, ByteView sig, ByteView pubkey) {
  return cryptolib_secp256k1_verify(digest32.data(), sig.data(), sig.size(),
                                    pubkey.data(), pubkey.size()) == 1;
}

Bytes secp256k1_recover(ByteView digest32, ByteView sig65) {
  return require_buffer(cryptolib_secp256k1_recover(digest32.data(), sig65.data()),
                        "secp256k1_recover failed");
}

StreamEncryptor::StreamEncryptor(ByteView key32) {
  h_ = cryptolib_stream_enc_create(key32.data());
  if (h_ == nullptr) throw std::runtime_error("stream_enc_create failed");
  CryptoBufferResult hr = cryptolib_stream_enc_header(h_);
  if (hr.buf.data == nullptr) {
    if (hr.error) cryptolib_str_free(hr.error);
    cryptolib_stream_enc_free(h_);
    h_ = nullptr;
    throw std::runtime_error("stream_enc_header failed");
  }
  header_ = take_buffer(hr.buf);
}

StreamEncryptor::~StreamEncryptor() {
  if (h_) cryptolib_stream_enc_free(h_);
}

StreamEncryptor::StreamEncryptor(StreamEncryptor&& o) noexcept
    : h_(o.h_), header_(std::move(o.header_)) {
  o.h_ = nullptr;
}

StreamEncryptor& StreamEncryptor::operator=(StreamEncryptor&& o) noexcept {
  if (this != &o) {
    if (h_) cryptolib_stream_enc_free(h_);
    h_ = o.h_;
    header_ = std::move(o.header_);
    o.h_ = nullptr;
  }
  return *this;
}

Bytes StreamEncryptor::push(ByteView plaintext, bool final_chunk) {
  const std::uint8_t tag = final_chunk ? 3 : 0;  // 3 = FINAL, 0 = MESSAGE
  CryptoBufferResult r =
      cryptolib_stream_enc_push(h_, plaintext.data(), plaintext.size(), tag);
  if (r.buf.data == nullptr) {
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error("stream_enc_push failed");
  }
  return take_buffer(r.buf);
}

StreamDecryptor::StreamDecryptor(ByteView key32, ByteView header24) {
  h_ = cryptolib_stream_dec_create(key32.data(), header24.data());
  if (h_ == nullptr) throw std::runtime_error("stream_dec_create failed");
}

StreamDecryptor::~StreamDecryptor() {
  if (h_) cryptolib_stream_dec_free(h_);
}

StreamDecryptor::StreamDecryptor(StreamDecryptor&& o) noexcept : h_(o.h_) {
  o.h_ = nullptr;
}

StreamDecryptor& StreamDecryptor::operator=(StreamDecryptor&& o) noexcept {
  if (this != &o) {
    if (h_) cryptolib_stream_dec_free(h_);
    h_ = o.h_;
    o.h_ = nullptr;
  }
  return *this;
}

Bytes StreamDecryptor::pull(ByteView ciphertext, bool* is_final) {
  std::uint8_t tag = 0;
  CryptoBufferResult r =
      cryptolib_stream_dec_pull(h_, ciphertext.data(), ciphertext.size(), &tag);
  if (r.buf.data == nullptr) {
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error("stream_dec_pull failed (auth/order/truncation)");
  }
  if (is_final) *is_final = (tag == 3);
  return take_buffer(r.buf);
}

std::string to_hex(const Hash& h) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string s;
  s.reserve(h.size() * 2);
  for (std::uint8_t b : h) {
    s.push_back(kHex[b >> 4]);
    s.push_back(kHex[b & 0x0F]);
  }
  return s;
}

}  // namespace aviary::crypto
