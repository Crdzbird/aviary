#include "blockchainer/interop/bridge.hpp"

#include <algorithm>
#include <stdexcept>

namespace blockchainer::interop::bridge {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }
Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}

constexpr std::uint8_t kPacketDomain = 0xB0;
constexpr std::uint8_t kAttestDomain = 0xB1;
}  // namespace

// ── Payload / packet ─────────────────────────────────────────────────────────

Bytes TransferPayload::encode() const {
  codec::CodecWriter w;
  w.write_u8(static_cast<std::uint8_t>(kind));
  w.write_fixed_bytes(view(asset), 32);
  w.write_u128(amount);
  w.write_fixed_bytes(view(recipient), 32);
  return w.take();
}

TransferPayload TransferPayload::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  TransferPayload p;
  const std::uint8_t k = r.read_u8();
  if (k > 1) throw codec::CodecError("bridge: bad transfer kind");
  p.kind = static_cast<TransferKind>(k);
  p.asset = to_hash(r.read_fixed_bytes(32));
  p.amount = r.read_u128();
  p.recipient = to_hash(r.read_fixed_bytes(32));
  r.assert_fully_consumed();
  return p;
}

Bytes Packet::encode() const {
  codec::CodecWriter w;
  w.write_u64(source_chain);
  w.write_u64(dest_chain);
  w.write_u64(nonce);
  w.write_bytes(view(payload));
  return w.take();
}

Packet Packet::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  Packet p;
  p.source_chain = r.read_u64();
  p.dest_chain = r.read_u64();
  p.nonce = r.read_u64();
  p.payload = r.read_bytes();
  r.assert_fully_consumed();
  return p;
}

Hash Packet::commitment() const {
  Bytes buf;
  buf.push_back(kPacketDomain);
  Bytes e = encode();
  buf.insert(buf.end(), e.begin(), e.end());
  return crypto::blake3_256(buf);
}

// ── Attestation ──────────────────────────────────────────────────────────────

Bytes attestation_signing_bytes(const Hash& commitment, std::uint32_t attestor_index) {
  codec::CodecWriter w;
  w.write_u8(kAttestDomain);
  w.write_fixed_bytes(view(commitment), 32);
  w.write_u32(attestor_index);
  return w.take();
}

AttestationVote sign_attestation(const Hash& commitment, std::uint32_t attestor_index,
                                 const crypto::KeyPair& bls_key) {
  Bytes msg = attestation_signing_bytes(commitment, attestor_index);
  AttestationVote v;
  v.attestor_index = attestor_index;
  v.signature = crypto::bls_sign(view(msg), view(bls_key.secret_key));
  return v;
}

std::optional<Attestation> aggregate_attestation(const consensus::ValidatorSet& attestors,
                                                 const Hash& commitment,
                                                 const std::vector<AttestationVote>& votes) {
  std::set<std::uint32_t> included;
  std::vector<std::uint32_t> signers;
  std::vector<Bytes> sigs;
  codec::u128 power = 0;

  for (const auto& v : votes) {
    if (v.attestor_index >= attestors.size()) continue;
    if (!included.insert(v.attestor_index).second) continue;
    Bytes msg = attestation_signing_bytes(commitment, v.attestor_index);
    if (!crypto::bls_verify(view(msg), view(v.signature),
                            view(attestors.at(v.attestor_index).bls_pubkey))) {
      included.erase(v.attestor_index);
      continue;
    }
    signers.push_back(v.attestor_index);
    power += attestors.at(v.attestor_index).power;
  }

  if (power < attestors.quorum()) return std::nullopt;

  std::sort(signers.begin(), signers.end());
  for (std::uint32_t idx : signers) {
    for (const auto& v : votes) {
      if (v.attestor_index == idx) { sigs.push_back(v.signature); break; }
    }
  }

  Attestation a;
  a.signers = std::move(signers);
  a.agg_signature = crypto::bls_aggregate(sigs);
  return a;
}

bool verify_attestation(const consensus::ValidatorSet& attestors, const Hash& commitment,
                        const Attestation& attestation) {
  if (attestation.signers.empty()) return false;
  codec::u128 power = 0;
  std::vector<Bytes> msgs, pks;
  for (std::size_t i = 0; i < attestation.signers.size(); ++i) {
    const std::uint32_t idx = attestation.signers[i];
    if (i > 0 && idx <= attestation.signers[i - 1]) return false;  // strictly ascending
    if (idx >= attestors.size()) return false;
    power += attestors.at(idx).power;
    msgs.push_back(attestation_signing_bytes(commitment, idx));
    pks.push_back(attestors.at(idx).bls_pubkey);
  }
  if (power < attestors.quorum()) return false;
  return crypto::bls_aggregate_verify(msgs, pks, view(attestation.agg_signature));
}

// ── Endpoint ─────────────────────────────────────────────────────────────────

namespace {
constexpr codec::u128 kU128Max = ~static_cast<codec::u128>(0);
}

Packet BridgeEndpoint::lock(std::uint64_t dest_chain, const Hash& asset,
                            codec::u128 amount, const Hash& recipient) {
  codec::u128& bal = escrow_[asset];
  if (bal > kU128Max - amount) throw std::runtime_error("bridge: escrow overflow");
  bal += amount;  // escrow on the source chain
  TransferPayload p{TransferKind::kMint, asset, amount, recipient};
  Packet packet;
  packet.source_chain = local_chain_;
  packet.dest_chain = dest_chain;
  packet.nonce = next_nonce_++;
  packet.payload = p.encode();
  return packet;
}

Packet BridgeEndpoint::burn(std::uint64_t dest_chain, const Hash& asset,
                            codec::u128 amount, const Hash& recipient) {
  TransferPayload p{TransferKind::kRelease, asset, amount, recipient};
  Packet packet;
  packet.source_chain = local_chain_;
  packet.dest_chain = dest_chain;
  packet.nonce = next_nonce_++;
  packet.payload = p.encode();
  return packet;
}

BridgeResult BridgeEndpoint::receive(const Packet& packet, const Attestation& attestation) {
  if (packet.dest_chain != local_chain_) return BridgeResult::failure("packet not for this chain");
  const auto key = std::make_pair(packet.source_chain, packet.nonce);
  if (processed_.count(key) != 0) return BridgeResult::failure("packet already processed");
  if (!verify_attestation(attestors_, packet.commitment(), attestation)) {
    return BridgeResult::failure("attestation below quorum or invalid");
  }

  TransferPayload p;
  try {
    p = TransferPayload::decode_all(view(packet.payload));
  } catch (const codec::CodecError& e) {
    return BridgeResult::failure(std::string("malformed payload: ") + e.what());
  }

  if (p.kind == TransferKind::kMint) {
    auto& bal = minted_[{p.asset, p.recipient}];
    if (bal > kU128Max - p.amount) return BridgeResult::failure("mint overflow");
    bal += p.amount;
  } else {  // kRelease
    auto it = escrow_.find(p.asset);
    if (it == escrow_.end() || it->second < p.amount) {
      return BridgeResult::failure("insufficient escrow to release");
    }
    it->second -= p.amount;
    released_[{p.asset, p.recipient}] += p.amount;
  }

  processed_.insert(key);  // commit replay protection only on success
  return BridgeResult::success();
}

codec::u128 BridgeEndpoint::minted_balance(const Hash& asset, const Hash& recipient) const {
  auto it = minted_.find({asset, recipient});
  return it == minted_.end() ? 0 : it->second;
}
codec::u128 BridgeEndpoint::released_balance(const Hash& asset, const Hash& recipient) const {
  auto it = released_.find({asset, recipient});
  return it == released_.end() ? 0 : it->second;
}
codec::u128 BridgeEndpoint::escrow(const Hash& asset) const {
  auto it = escrow_.find(asset);
  return it == escrow_.end() ? 0 : it->second;
}
bool BridgeEndpoint::processed(std::uint64_t source_chain, std::uint64_t nonce) const {
  return processed_.count({source_chain, nonce}) != 0;
}

}  // namespace blockchainer::interop::bridge
