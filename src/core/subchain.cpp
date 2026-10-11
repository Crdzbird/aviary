#include <optional>
#include "aviary/subchain.hpp"

#include "aviary/codec.hpp"
#include "aviary/genesis.hpp"
#include "aviary/light_client.hpp"

#include <stdexcept>

namespace aviary {

namespace {
codec::ByteView bview(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hview(const Hash& h) { return codec::ByteView(h.data(), h.size()); }
Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32 && i < b.size(); ++i) h[i] = b[i];
  return h;
}
}  // namespace

// -- ChildRegistration --------------------------------------------------------

Bytes ChildRegistration::encode() const {
  codec::CodecWriter w;
  w.write_u64(chain_id);
  Bytes hdr = genesis_header.encode();
  w.write_bytes(bview(hdr));
  w.write_list(validators, [](codec::CodecWriter& w, const consensus::Validator& v) {
    w.write_bytes(bview(v.bls_pubkey));
    w.write_u64(v.power);
  });
  w.write_u8(child_max_depth);
  return w.take();
}

ChildRegistration ChildRegistration::decode(codec::CodecReader& r) {
  ChildRegistration c;
  c.chain_id = r.read_u64();
  Bytes hdr = r.read_bytes();
  c.genesis_header = BlockHeader::decode_all(bview(hdr));
  c.validators = r.read_list<consensus::Validator>([](codec::CodecReader& r) {
    consensus::Validator v;
    v.bls_pubkey = r.read_bytes();
    v.power = r.read_u64();
    return v;
  });
  c.child_max_depth = r.read_u8();
  return c;
}

// -- SubchainPolicy -----------------------------------------------------------

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
    if (static_cast<unsigned>(c.child_max_depth) + 1 > max_depth)
      return "subchain policy: child subtree deeper than max_depth allows";
  }
  return {};
}

void SubchainPolicy::commit_to(GenesisSpec& g) const { g.subchain_policy = encode(); }

std::optional<SubchainPolicy> SubchainPolicy::from_genesis(const GenesisSpec& g) {
  if (g.subchain_policy.empty()) return std::nullopt;
  return decode_all(crypto::ByteView(g.subchain_policy.data(), g.subchain_policy.size()));
}

// -- ParentBinding ------------------------------------------------------------

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

// -- SubchainRegistry ---------------------------------------------------------

SubchainRegistry SubchainRegistry::from_policy(std::uint64_t own_chain_id, SubchainPolicy policy) {
  if (std::string e = policy.validate(own_chain_id); !e.empty()) throw std::invalid_argument(e);
  SubchainRegistry r;
  r.policy_ = std::move(policy);
  r.children_.reserve(r.policy_.children.size());
  for (const ChildRegistration& c : r.policy_.children) {
    // ValidatorSet's constructor rejects a malformed set (bad key length, no
    // power) - surfaced here, at boot, never at anchor time.
    consensus::ValidatorSet vs(c.validators);
    r.children_.push_back(ChildEntry{c, vs, vs, c.genesis_header});
  }
  return r;
}

const BlockHeader* SubchainRegistry::tip_of(std::uint64_t child_chain_id) const {
  for (const ChildEntry& e : children_)
    if (e.reg.chain_id == child_chain_id) return &e.tip;
  return nullptr;
}

std::string SubchainRegistry::apply_anchors(const std::vector<Anchor>& anchors) {
  // Cheap structural rules first (docs/design/SUBCHAINS.md section 6 step 3b), so a
  // peer cannot make us do BLS work on a block that is malformed anyway.
  if (policy_.max_anchors_per_block != 0 && anchors.size() > policy_.max_anchors_per_block)
    return "too many anchors in block";
  for (std::size_t i = 0; i < anchors.size(); ++i) {
    const Anchor& a = anchors[i];
    if (i > 0 && !(anchors[i - 1].child_chain_id < a.child_chain_id))
      return "anchors must be ascending, unique by child chain_id";
    if (a.headers.empty()) return "empty anchor";
    if (policy_.max_headers_per_anchor != 0 && a.headers.size() > policy_.max_headers_per_anchor)
      return "anchor segment exceeds max_headers_per_anchor";
    bool known = false;
    for (const ChildEntry& e : children_) known |= (e.reg.chain_id == a.child_chain_id);
    if (!known) return "anchor for an unregistered child chain";
  }
  // Then the light-client rule per header (step 3c).
  for (const Anchor& a : anchors) {
    ChildEntry* e = nullptr;
    for (ChildEntry& c : children_)
      if (c.reg.chain_id == a.child_chain_id) e = &c;
    client::LightClient lc(e->reg.chain_id, e->vset, e->tip, e->tip_vset);
    for (const CertifiedHeader& ch : a.headers) {
      consensus::CommitCertificate cert;
      try {
        cert = consensus::CommitCertificate::decode_all(bview(ch.certificate));
      } catch (const std::exception& ex) {
        return std::string("malformed child certificate: ") + ex.what();
      }
      client::LightClient::Result r = lc.accept(ch.header, cert);
      if (!r.ok) return "child " + std::to_string(a.child_chain_id) + " header rejected: " + r.error;
    }
    e->tip = lc.tip();
    e->vset = lc.validators();          // a child's finalized rotation is followed here too
    e->tip_vset = lc.tip_validators();
  }
  return {};
}

Hash registry_root(const std::vector<ChildSummary>& summaries) {
  codec::CodecWriter w;
  w.write_u32(static_cast<std::uint32_t>(summaries.size()));
  for (const ChildSummary& s : summaries) {
    w.write_u64(s.chain_id);
    w.write_fixed_bytes(hview(s.genesis_header_hash), 32);
    const Hash th = s.tip.hash();
    w.write_fixed_bytes(hview(th), 32);
    w.write_u64(s.tip.height);
    w.write_fixed_bytes(hview(s.tip.state_root), 32);
    w.write_fixed_bytes(hview(s.vset_hash), 32);
    w.write_fixed_bytes(hview(s.tip_vset_hash), 32);
  }
  return crypto::hash_tagged(crypto::Domain::kSubchainRegistry, w.take());
}

std::vector<ChildSummary> SubchainRegistry::summaries() const {
  std::vector<ChildSummary> out;
  out.reserve(children_.size());
  for (const ChildEntry& e : children_)
    out.push_back(ChildSummary{e.reg.chain_id, e.reg.genesis_header.hash(), e.tip, e.vset.hash(), e.tip_vset.hash()});
  return out;
}

Hash SubchainRegistry::root() const { return registry_root(summaries()); }

SubchainProof::Hop SubchainRegistry::hop(std::uint64_t child_chain_id) const {
  if (tip_of(child_chain_id) == nullptr) throw std::invalid_argument("hop: child not registered");
  return SubchainProof::Hop{summaries(), child_chain_id};
}

// -- SubchainProof ------------------------------------------------------------

Bytes SubchainProof::Hop::encode() const {
  codec::CodecWriter w;
  w.write_list(summaries, [](codec::CodecWriter& w, const ChildSummary& s) {
    w.write_u64(s.chain_id);
    w.write_fixed_bytes(hview(s.genesis_header_hash), 32);
    Bytes t = s.tip.encode();
    w.write_bytes(bview(t));
    w.write_fixed_bytes(hview(s.vset_hash), 32);
    w.write_fixed_bytes(hview(s.tip_vset_hash), 32);
  });
  w.write_u64(child_chain_id);
  return w.take();
}

SubchainProof::Hop SubchainProof::Hop::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  Hop h;
  h.summaries = r.read_list<ChildSummary>([](codec::CodecReader& r) {
    ChildSummary s;
    s.chain_id = r.read_u64();
    s.genesis_header_hash = to_hash(r.read_fixed_bytes(32));
    Bytes t = r.read_bytes();
    s.tip = BlockHeader::decode_all(bview(t));
    s.vset_hash = to_hash(r.read_fixed_bytes(32));
    s.tip_vset_hash = to_hash(r.read_fixed_bytes(32));
    return s;
  });
  h.child_chain_id = r.read_u64();
  r.assert_fully_consumed();
  return h;
}

Bytes SubchainProof::encode() const {
  codec::CodecWriter w;
  w.write_list(hops, [](codec::CodecWriter& w, const Hop& h) {
    Bytes e = h.encode();
    w.write_bytes(bview(e));
  });
  Bytes sp = state_proof.encode();
  w.write_bytes(bview(sp));
  return w.take();
}

SubchainProof SubchainProof::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  SubchainProof p;
  p.hops = r.read_list<Hop>([](codec::CodecReader& r) {
    Bytes e = r.read_bytes();
    return Hop::decode_all(bview(e));
  });
  Bytes sp = r.read_bytes();
  p.state_proof = StateProof::decode_all(bview(sp));
  r.assert_fully_consumed();
  return p;
}

bool verify_subchain_proof(const Hash& parent_subchain_root, const SubchainProof& p) {
  if (p.hops.empty()) return false;
  Hash root = parent_subchain_root;
  const BlockHeader* tip = nullptr;
  for (const SubchainProof::Hop& h : p.hops) {
    // The list must be canonical (ascending, unique) - otherwise two lists
    // could not be told apart from the root alone.
    for (std::size_t i = 1; i < h.summaries.size(); ++i)
      if (!(h.summaries[i - 1].chain_id < h.summaries[i].chain_id)) return false;
    if (registry_root(h.summaries) != root) return false;
    tip = nullptr;
    for (const ChildSummary& s : h.summaries)
      if (s.chain_id == h.child_chain_id) tip = &s.tip;
    if (tip == nullptr) return false;
    if (tip->chain_id != h.child_chain_id) return false;
    root = tip->subchain_root;  // descend (zero unless that child is itself a parent)
  }
  return verify_state_proof(tip->state_root, p.state_proof);
}

Bytes SubchainRegistry::serialize_tips() const {
  codec::CodecWriter w;
  w.write_list(children_, [](codec::CodecWriter& w, const ChildEntry& e) {
    w.write_u64(e.reg.chain_id);
    Bytes t = e.tip.encode();
    w.write_bytes(bview(t));
    Bytes vs = e.vset.encode();
    w.write_bytes(bview(vs));
    Bytes ts = e.tip_vset.encode();
    w.write_bytes(bview(ts));
  });
  return w.take();
}

void SubchainRegistry::restore_tips(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  struct Tip { std::uint64_t id; BlockHeader h; Bytes vs; Bytes ts; };
  auto tips = r.read_list<Tip>([](codec::CodecReader& r) {
    Tip t;
    t.id = r.read_u64();
    Bytes h = r.read_bytes();
    t.h = BlockHeader::decode_all(bview(h));
    t.vs = r.read_bytes();
    t.ts = r.read_bytes();
    return t;
  });
  r.assert_fully_consumed();
  if (tips.size() != children_.size()) throw std::invalid_argument("subchain snapshot: child count mismatch");
  for (std::size_t i = 0; i < tips.size(); ++i) {
    if (tips[i].id != children_[i].reg.chain_id || tips[i].h.chain_id != tips[i].id)
      throw std::invalid_argument("subchain snapshot: child id mismatch");
    children_[i].tip = tips[i].h;
    children_[i].vset = consensus::ValidatorSet::decode_all(bview(tips[i].vs));      // throws on a malformed set
    children_[i].tip_vset = consensus::ValidatorSet::decode_all(bview(tips[i].ts));
  }
}

}  // namespace aviary
