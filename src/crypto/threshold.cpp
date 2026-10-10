#include "aviary/threshold.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "cryptolib_c.h"

namespace aviary::custody {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

Bytes take(CryptoBufferResult r, const char* op) {
  if (r.buf.data == nullptr) {
    std::string msg = r.error ? r.error : std::string(op) + " failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error("threshold: " + msg);
  }
  Bytes out(r.buf.data, r.buf.data + r.buf.len);
  cryptolib_buffer_free(&r.buf);
  return out;
}

Bytes copy_buf(const CryptoBuffer& b) { return Bytes(b.data, b.data + b.len); }

// Sealed-share AAD: "aviary.share.v1" ‖ hex(group_pk) ‖ ":" ‖ index —
// binds the envelope to one slot of one group.
std::string share_aad(const Bytes& group_pk, std::uint16_t index) {
  static const char* kHex = "0123456789abcdef";
  std::string s = "aviary.share.v1:";
  for (std::uint8_t b : group_pk) {
    s.push_back(kHex[b >> 4]);
    s.push_back(kHex[b & 0x0F]);
  }
  s.push_back(':');
  s += std::to_string(index);
  return s;
}
}  // namespace

// ── ShareCard ───────────────────────────────────────────────────────────────

Bytes ShareCard::encode() const {
  codec::CodecWriter w;
  w.write_u8(1);  // ShareCard format version
  w.write_u16(index);
  w.write_u16(threshold);
  w.write_u16(total);
  w.write_fixed_bytes(view(group_public_key), 32);
  w.write_fixed_bytes(view(secret_share), 32);
  w.write_fixed_bytes(view(public_share), 32);
  w.write_list(all_public_shares,
               [](codec::CodecWriter& w, const Bytes& pk) { w.write_fixed_bytes(codec::ByteView(pk.data(), pk.size()), 32); });
  return w.take();
}

ShareCard ShareCard::decode(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  if (r.read_u8() != 1) throw codec::CodecError("unsupported ShareCard version");
  ShareCard c;
  c.index = r.read_u16();
  c.threshold = r.read_u16();
  c.total = r.read_u16();
  c.group_public_key = r.read_fixed_bytes(32);
  c.secret_share = r.read_fixed_bytes(32);
  c.public_share = r.read_fixed_bytes(32);
  c.all_public_shares = r.read_list<Bytes>([](codec::CodecReader& r) { return r.read_fixed_bytes(32); });
  r.assert_fully_consumed();
  if (c.index == 0 || c.threshold == 0 || c.total < c.threshold || c.index > c.total ||
      c.all_public_shares.size() != c.total) {
    throw codec::CodecError("inconsistent ShareCard parameters");
  }
  return c;
}

// The AAD is derived from group + index, which the opener only knows after
// opening — so the envelope carries a cleartext binding prefix that reproduces
// it: [u16 index][32 B group_pk][molecular envelope]. Tampering with the prefix
// changes the derived AAD and the open fails closed; a successful open is then
// cross-checked against the card inside.
Bytes ShareCard::seal(std::string_view passphrase, std::uint64_t ops, std::size_t mem) const {
  const Bytes plain = encode();
  const std::string aad = share_aad(group_public_key, index);
  const std::string p(passphrase);
  const Bytes sealed = take(
      cryptolib_molecular_seal(plain.data(), plain.size(), p.c_str(),
                               reinterpret_cast<const std::uint8_t*>(aad.data()), aad.size(), ops, mem),
      "share seal");
  codec::CodecWriter w;
  w.write_u16(index);
  w.write_fixed_bytes(view(group_public_key), 32);
  w.write_bytes(view(sealed));
  return w.take();
}

ShareCard ShareCard::unseal(codec::ByteView envelope, std::string_view passphrase) {
  codec::CodecReader r(envelope);
  const std::uint16_t index = r.read_u16();
  const Bytes group_pk = r.read_fixed_bytes(32);
  const Bytes inner = r.read_bytes();
  r.assert_fully_consumed();

  const std::string aad = share_aad(group_pk, index);
  const std::string p(passphrase);
  const Bytes plain = take(cryptolib_molecular_open(inner.data(), inner.size(), p.c_str(),
                                                    reinterpret_cast<const std::uint8_t*>(aad.data()),
                                                    aad.size()),
                           "share unseal");
  ShareCard c = decode(view(plain));
  if (c.index != index || c.group_public_key != group_pk) {
    throw std::runtime_error("threshold: share envelope binding mismatch");
  }
  return c;
}

// ── ThresholdAccount ────────────────────────────────────────────────────────

std::vector<ShareCard> ThresholdAccount::create(std::uint16_t n, std::uint16_t t) {
  CryptoFrostKeyGen kg = cryptolib_frost_keygen(n, t);
  if (kg.error != nullptr || kg.group_public_key.data == nullptr) {
    std::string msg = kg.error ? kg.error : "frost keygen failed";
    cryptolib_frost_keygen_free(&kg);
    throw std::runtime_error("threshold: " + msg);
  }

  const Bytes gpk = copy_buf(kg.group_public_key);
  std::vector<Bytes> pub_shares;
  pub_shares.reserve(kg.count);
  for (std::size_t i = 0; i < kg.count; ++i) {
    pub_shares.emplace_back(kg.public_shares.data + i * 32, kg.public_shares.data + (i + 1) * 32);
  }

  std::vector<ShareCard> cards;
  cards.reserve(kg.count);
  for (std::size_t i = 0; i < kg.count; ++i) {
    ShareCard c;
    c.index = static_cast<std::uint16_t>(i + 1);  // identifier k+1 per the ABI
    c.threshold = t;
    c.total = n;
    c.group_public_key = gpk;
    c.secret_share.assign(kg.secret_shares.data + i * 32, kg.secret_shares.data + (i + 1) * 32);
    c.public_share = pub_shares[i];
    c.all_public_shares = pub_shares;
    cards.push_back(std::move(c));
  }
  cryptolib_frost_keygen_free(&kg);  // zeroises the dealer's secret material
  return cards;
}

// ── SigningCeremony ─────────────────────────────────────────────────────────

std::pair<SigningCeremony::Nonces, SigningCeremony::Commitment> SigningCeremony::commit(
    const ShareCard& share) {
  CryptoFrostCommit c = cryptolib_frost_commit(share.secret_share.data(), share.secret_share.size(),
                                               share.index);
  if (c.error != nullptr || c.hiding_nonce.data == nullptr) {
    std::string msg = c.error ? c.error : "frost commit failed";
    cryptolib_frost_commit_free(&c);
    throw std::runtime_error("threshold: " + msg);
  }
  Nonces n{copy_buf(c.hiding_nonce), copy_buf(c.binding_nonce)};
  Commitment cm{share.index, copy_buf(c.hiding_commit), copy_buf(c.binding_commit)};
  cryptolib_frost_commit_free(&c);  // zeroises the native nonce copies
  return {std::move(n), std::move(cm)};
}

SigningCeremony::SigningCeremony(Bytes group_public_key, Bytes message,
                                 std::vector<Commitment> roster)
    : gpk_(std::move(group_public_key)), msg_(std::move(message)), roster_(std::move(roster)) {
  if (roster_.empty()) throw std::runtime_error("threshold: empty ceremony roster");
  std::sort(roster_.begin(), roster_.end(),
            [](const Commitment& a, const Commitment& b) { return a.id < b.id; });
  for (std::size_t i = 0; i + 1 < roster_.size(); ++i) {
    if (roster_[i].id == roster_[i + 1].id) throw std::runtime_error("threshold: duplicate roster id");
  }
  ids_.reserve(roster_.size());
  hiding_flat_.reserve(roster_.size() * 32);
  binding_flat_.reserve(roster_.size() * 32);
  for (const Commitment& c : roster_) {
    if (c.hiding.size() != 32 || c.binding.size() != 32) {
      throw std::runtime_error("threshold: malformed roster commitment");
    }
    ids_.push_back(c.id);
    hiding_flat_.insert(hiding_flat_.end(), c.hiding.begin(), c.hiding.end());
    binding_flat_.insert(binding_flat_.end(), c.binding.begin(), c.binding.end());
  }
}

Bytes SigningCeremony::sign(const ShareCard& share, const Nonces& nonces) const {
  return take(cryptolib_frost_sign(share.index, share.secret_share.data(), share.secret_share.size(),
                                   gpk_.data(), gpk_.size(), nonces.hiding.data(), nonces.hiding.size(),
                                   nonces.binding.data(), nonces.binding.size(), msg_.data(), msg_.size(),
                                   ids_.data(), hiding_flat_.data(), binding_flat_.data(), ids_.size()),
              "frost sign");
}

bool SigningCeremony::verify_share(std::uint16_t id, const Bytes& public_share,
                                   const Bytes& sig_share) const {
  // Locate this participant's own round-1 commitment in the roster.
  auto it = std::find_if(roster_.begin(), roster_.end(),
                         [&](const Commitment& c) { return c.id == id; });
  if (it == roster_.end()) return false;
  return cryptolib_frost_verify_share(id, public_share.data(), public_share.size(), sig_share.data(),
                                      sig_share.size(), it->hiding.data(), it->hiding.size(),
                                      it->binding.data(), it->binding.size(), gpk_.data(), gpk_.size(),
                                      msg_.data(), msg_.size(), ids_.data(), hiding_flat_.data(),
                                      binding_flat_.data(), ids_.size()) == 1;
}

Bytes SigningCeremony::aggregate(const std::vector<Bytes>& sig_shares) const {
  if (sig_shares.size() != roster_.size()) {
    throw std::runtime_error("threshold: sig share count does not match the roster");
  }
  Bytes flat;
  flat.reserve(sig_shares.size() * 32);
  for (const Bytes& s : sig_shares) {
    if (s.size() != 32) throw std::runtime_error("threshold: malformed signature share");
    flat.insert(flat.end(), s.begin(), s.end());
  }
  return take(cryptolib_frost_aggregate(gpk_.data(), gpk_.size(), msg_.data(), msg_.size(), ids_.data(),
                                        hiding_flat_.data(), binding_flat_.data(), ids_.size(),
                                        flat.data()),
              "frost aggregate");
}

}  // namespace aviary::custody
