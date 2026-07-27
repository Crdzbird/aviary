#include "blockchainer/credential_gate.hpp"

#include <stdexcept>
#include <string>

#include "blockchainer/codec.hpp"

namespace blockchainer::consensus {

namespace {

// Deterministic byte-encoding of the disclosed attributes. These MUST be
// byte-identical issuer-side (what gets signed) and verifier-side (what the proof
// is checked against), so both build them through the same helpers.
Bytes enc_u8(std::uint8_t v) { return Bytes{v}; }
Bytes enc_u32(std::uint32_t v) {
  codec::CodecWriter w;
  w.write_u32(v);
  return w.take();
}
Bytes enc_u64(std::uint64_t v) {
  codec::CodecWriter w;
  w.write_u64(v);
  return w.take();
}

crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
crypto::ByteView str_view(std::string_view s) {
  return crypto::ByteView(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

// ph = hash_tagged(kCredentialPresentation, chain_id ‖ epoch ‖ scope ‖ challenge).
// Binds a presentation to its exact context so a proof for one (epoch, scope,
// challenge) never verifies against another (anti-replay; CREDENTIAL_GATE.md §6).
crypto::Hash presentation_header(std::uint64_t chain_id, std::uint32_t epoch,
                                 std::string_view scope, crypto::ByteView challenge) {
  codec::CodecWriter w;
  w.write_u64(chain_id);
  w.write_u32(epoch);
  w.write_bytes(str_view(scope));
  w.write_bytes(challenge);
  return crypto::hash_tagged(crypto::Domain::kCredentialPresentation, w.take());
}

// The indexes Option A discloses: {tier, not_after, issuer_epoch}. Ascending, as
// BBS requires. member_secret (index 0) and any extras stay hidden.
const std::vector<std::uint64_t> kDisclosedIndexes{
    credential_attr::kTier, credential_attr::kNotAfter, credential_attr::kIssuerEpoch};

}  // namespace

// ── CredentialPolicy ─────────────────────────────────────────────────────────

Bytes CredentialPolicy::encode() const {
  codec::CodecWriter w;
  w.write_u64(chain_id);
  w.write_bytes(view(issuer_public_key));
  w.write_bytes(view(header));
  w.write_u32(attribute_count);
  w.write_u8(min_tier);
  return w.take();
}

CredentialPolicy CredentialPolicy::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  CredentialPolicy p;
  p.chain_id = r.read_u64();
  p.issuer_public_key = r.read_bytes();
  p.header = r.read_bytes();
  p.attribute_count = r.read_u32();
  p.min_tier = r.read_u8();
  r.assert_fully_consumed();
  return p;
}

crypto::Hash CredentialPolicy::hash() const {
  return crypto::hash_tagged(crypto::Domain::kCredentialPolicy, encode());
}

// ── Issuance (trusted-issuer model; §5/§10.2) ────────────────────────────────

Credential issue_credential(const CredentialPolicy& policy, crypto::ByteView issuer_secret_key,
                            crypto::ByteView member_secret, std::uint8_t tier,
                            std::uint64_t not_after, std::uint32_t issuer_epoch,
                            const std::vector<Bytes>& extra) {
  if (member_secret.size() != 32)
    throw std::runtime_error("issue_credential: member_secret must be 32 bytes");
  if (credential_attr::kBaseCount + extra.size() != policy.attribute_count)
    throw std::runtime_error("issue_credential: attribute count does not match policy");

  std::vector<Bytes> attrs;
  attrs.reserve(policy.attribute_count);
  attrs.emplace_back(member_secret.begin(), member_secret.end());  // 0: member_secret
  attrs.push_back(enc_u8(tier));                                    // 1: tier
  attrs.push_back(enc_u64(not_after));                             // 2: not_after
  attrs.push_back(enc_u32(issuer_epoch));                         // 3: issuer_epoch
  for (const auto& e : extra) attrs.push_back(e);                  // 4…: policy-specific

  Bytes sig = crypto::bbs_sign(issuer_secret_key, view(policy.issuer_public_key),
                               view(policy.header), attrs);
  return Credential{std::move(sig), std::move(attrs)};
}

// ── Presentation (holder) ────────────────────────────────────────────────────

Bytes CredentialGate::present(const CredentialPolicy& policy, const Credential& cred,
                              std::uint32_t epoch, std::string_view scope,
                              crypto::ByteView challenge) {
  if (cred.attrs.size() < credential_attr::kBaseCount)
    throw std::runtime_error("present: credential is missing required attributes");

  crypto::Hash ph = presentation_header(policy.chain_id, epoch, scope, challenge);
  Bytes proof = crypto::bbs_proof_gen(view(policy.issuer_public_key), view(cred.signature),
                                      view(policy.header), crypto::ByteView(ph.data(), ph.size()),
                                      cred.attrs, kDisclosedIndexes);

  // The presentation carries the proof plus the disclosed attribute *values*; the
  // verifier re-derives their canonical byte-messages through the same helpers,
  // so what it checks the proof against is byte-identical to what was signed.
  codec::CodecWriter w;
  w.write_bytes(view(proof));
  w.write_u8(cred.attrs[credential_attr::kTier].empty() ? 0 : cred.attrs[credential_attr::kTier][0]);
  {
    codec::CodecReader nr(view(cred.attrs[credential_attr::kNotAfter]));
    w.write_u64(nr.read_u64());
  }
  {
    codec::CodecReader er(view(cred.attrs[credential_attr::kIssuerEpoch]));
    w.write_u32(er.read_u32());
  }
  return w.take();
}

// ── Admission (verifier) ─────────────────────────────────────────────────────

bool CredentialGate::admit(std::uint32_t epoch, std::string_view scope, crypto::ByteView challenge,
                           crypto::ByteView presentation, std::uint64_t now) const {
  Bytes proof;
  std::uint8_t tier;
  std::uint64_t not_after;
  std::uint32_t issuer_epoch;
  try {
    codec::CodecReader r(presentation);
    proof = r.read_bytes();
    tier = r.read_u8();
    not_after = r.read_u64();
    issuer_epoch = r.read_u32();
    r.assert_fully_consumed();
  } catch (...) {
    return false;  // malformed presentation
  }

  // Rebuild the disclosed byte-messages exactly as they were signed.
  std::vector<Bytes> disclosed{enc_u8(tier), enc_u64(not_after), enc_u32(issuer_epoch)};

  crypto::Hash ph = presentation_header(policy_.chain_id, epoch, scope, challenge);
  bool proof_ok;
  try {
    proof_ok = crypto::bbs_proof_verify(view(policy_.issuer_public_key), view(proof),
                                        view(policy_.header),
                                        crypto::ByteView(ph.data(), ph.size()), disclosed,
                                        kDisclosedIndexes);
  } catch (...) {
    return false;
  }
  if (!proof_ok) return false;               // 1. issuer-signed & context-bound
  if (not_after <= now) return false;        // 2. expiry / coarse revocation
  if (issuer_epoch_revoked(issuer_epoch)) return false;  // 3. bulk revocation
  if (tier < policy_.min_tier) return false;             // 4. attribute policy
  return true;
}

}  // namespace blockchainer::consensus
