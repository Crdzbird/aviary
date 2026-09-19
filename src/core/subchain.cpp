#include "blockchainer/subchain.hpp"

#include "blockchainer/codec.hpp"
#include "blockchainer/genesis.hpp"

namespace blockchainer {

namespace {
codec::ByteView bview(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hview(const Hash& h) { return codec::ByteView(h.data(), h.size()); }
Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32 && i < b.size(); ++i) h[i] = b[i];
  return h;
}
}  // namespace

// ── ChildRegistration ────────────────────────────────────────────────────────

Bytes ChildRegistration::encode() const {
  codec::CodecWriter w;
  w.write_u64(chain_id);
  w.write_fixed_bytes(hview(genesis_hash), 32);
  Bytes hdr = genesis_header.encode();
  w.write_bytes(bview(hdr));
  w.write_list(validators, [](codec::CodecWriter& w, const consensus::Validator& v) {
    w.write_bytes(bview(v.bls_pubkey));
    w.write_u64(v.power);
  });
  return w.take();
}

ChildRegistration ChildRegistration::decode(codec::CodecReader& r) {
  ChildRegistration c;
  c.chain_id = r.read_u64();
  c.genesis_hash = to_hash(r.read_fixed_bytes(32));
  Bytes hdr = r.read_bytes();
  c.genesis_header = BlockHeader::decode_all(bview(hdr));
  c.validators = r.read_list<consensus::Validator>([](codec::CodecReader& r) {
    consensus::Validator v;
    v.bls_pubkey = r.read_bytes();
    v.power = r.read_u64();
    return v;
  });
  return c;
}

// ── SubchainPolicy ───────────────────────────────────────────────────────────

Bytes SubchainPolicy::encode() const {
  codec::CodecWriter w;
  w.write_u16(max_children);
  w.write_u8(max_depth);
  w.write_u16(max_anchors_per_block);
  w.write_u16(max_headers_per_anchor);
  w.write_list(children, [](codec::CodecWriter& w, const ChildRegistration& c) {
    Bytes e = c.encode();
    w.write_bytes(bview(e));
  });
  return w.take();
}

SubchainPolicy SubchainPolicy::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  SubchainPolicy p;
  p.max_children = r.read_u16();
  p.max_depth = r.read_u8();
  p.max_anchors_per_block = r.read_u16();
  p.max_headers_per_anchor = r.read_u16();
  p.children = r.read_list<ChildRegistration>([](codec::CodecReader& r) {
    Bytes e = r.read_bytes();
    codec::CodecReader cr(bview(e));
    ChildRegistration c = ChildRegistration::decode(cr);
    cr.assert_fully_consumed();
    return c;
  });
  r.assert_fully_consumed();
  return p;
}

crypto::Hash SubchainPolicy::hash() const {
  return crypto::hash_tagged(crypto::Domain::kSubchainPolicy, encode());
}

std::string SubchainPolicy::validate(std::uint64_t own_chain_id) const {
  if (max_depth == 0) return "subchain policy: max_depth must be >= 1";
  if (children.size() > max_children) return "subchain policy: more children than max_children";
  for (std::size_t i = 0; i < children.size(); ++i) {
    const ChildRegistration& c = children[i];
    if (c.chain_id == own_chain_id) return "subchain policy: a chain cannot be its own child";
    if (i > 0 && !(children[i - 1].chain_id < c.chain_id))
      return "subchain policy: children must be ascending, unique by chain_id";
    if (c.validators.empty()) return "subchain policy: child has no validators";
    if (c.genesis_header.height != 0) return "subchain policy: child genesis header must be height 0";
    if (c.genesis_header.chain_id != c.chain_id) return "subchain policy: child genesis header chain_id mismatch";
    for (const auto& v : c.validators)
      if (v.power == 0) return "subchain policy: child validator with zero power";
  }
  return {};
}

void SubchainPolicy::commit_to(GenesisSpec& g) const { g.subchain_policy = encode(); }

std::optional<SubchainPolicy> SubchainPolicy::from_genesis(const GenesisSpec& g) {
  if (g.subchain_policy.empty()) return std::nullopt;
  return decode_all(crypto::ByteView(g.subchain_policy.data(), g.subchain_policy.size()));
}

// ── ParentBinding ────────────────────────────────────────────────────────────

Bytes ParentBinding::encode() const {
  codec::CodecWriter w;
  w.write_u64(parent_chain_id);
  w.write_fixed_bytes(hview(parent_genesis_hash), 32);
  return w.take();
}

ParentBinding ParentBinding::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  ParentBinding b;
  b.parent_chain_id = r.read_u64();
  b.parent_genesis_hash = to_hash(r.read_fixed_bytes(32));
  r.assert_fully_consumed();
  return b;
}

crypto::Hash ParentBinding::hash() const {
  return crypto::hash_tagged(crypto::Domain::kParentBinding, encode());
}

void ParentBinding::commit_to(GenesisSpec& g) const { g.parent_binding = encode(); }

std::optional<ParentBinding> ParentBinding::from_genesis(const GenesisSpec& g) {
  if (g.parent_binding.empty()) return std::nullopt;
  return decode_all(crypto::ByteView(g.parent_binding.data(), g.parent_binding.size()));
}

}  // namespace blockchainer
