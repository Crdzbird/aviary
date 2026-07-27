#include "blockchainer/crypto.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include "cryptolib_c.h"

namespace blockchainer::crypto {

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

}  // namespace blockchainer::crypto
