#include "aviary/dynamic_subchain.hpp"

#include <set>
#include <stdexcept>
#include <utility>

#include "aviary/codec.hpp"
#include "aviary/genesis.hpp"
#include "aviary/light_client.hpp"

namespace aviary {

namespace {
constexpr std::uint8_t kDynamicFormat = 2;
constexpr std::size_t kMaxHybridKey = 4096;
constexpr std::uint32_t kMaxCount = 0xFFFFFFFFu;

codec::ByteView bview(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hview(const Hash& h) { return codec::ByteView(h.data(), h.size()); }
Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < h.size() && i < b.size(); ++i) h[i] = b[i];
  return h;
}

void write_registrar(codec::CodecWriter& w, const Registrar& r) {
  w.write_u8(r.sig_algo);
  w.write_bytes(bview(r.public_key));
}

Registrar read_registrar(codec::CodecReader& r) {
  Registrar out;
  out.sig_algo = r.read_u8();
  out.public_key = r.read_bytes();
  return out;
}

void write_registration(codec::CodecWriter& w, const ChildRegistration& c) {
  const Bytes e = c.encode();
  w.write_bytes(bview(e));
}

ChildRegistration decode_registration(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  ChildRegistration c = ChildRegistration::decode(r);
  r.assert_fully_consumed();
  return c;
}

std::string registration_error(const ChildRegistration& c, std::uint64_t own_chain_id, std::uint8_t max_depth) {
  if (c.chain_id == own_chain_id) return "a chain cannot be its own child";
  if (c.validators.empty()) return "child has no validators";
  for (const consensus::Validator& v : c.validators)
    if (v.power == 0) return "child validator with zero power";
  if (c.genesis_header.height != 0) return "child genesis header must be height 0";
  if (c.genesis_header.chain_id != c.chain_id) return "child genesis header chain_id mismatch";
  if (static_cast<unsigned>(c.child_max_depth) + 1 > max_depth) return "child subtree deeper than max_depth allows";
  return {};
}

DynamicSubchainRegistry::Child genesis_child(const ChildRegistration& reg, std::uint32_t index,
                                             std::uint64_t registered_height) {
  auto set = std::make_shared<const consensus::ValidatorSet>(reg.validators);
  DynamicSubchainRegistry::Child c;
  c.tip = reg.genesis_header;
  c.vset = set;
  c.tip_vset = set;
  c.record.index = index;
  c.record.chain_id = reg.chain_id;
  c.record.status = ChildRecord::kOpen;
  c.record.child_max_depth = reg.child_max_depth;
  c.record.genesis_header_hash = reg.genesis_header.hash();
  c.record.registered_height = registered_height;
  c.record.tip_height = 0;
  c.record.tip_hash = c.record.genesis_header_hash;
  c.record.tip_state_root = reg.genesis_header.state_root;
  c.record.tip_subchain_root = reg.genesis_header.subchain_root;
  c.record.vset_hash = set->hash();
  c.record.tip_vset_hash = c.record.vset_hash;
  return c;
}
}  // namespace

Bytes RegistrarSet::encode() const {
  codec::CodecWriter w;
  w.write_u8(threshold);
  w.write_list(registrars, write_registrar);
  return w.take();
}

RegistrarSet RegistrarSet::decode(codec::CodecReader& r) {
  RegistrarSet s;
  s.threshold = r.read_u8();
  s.registrars = r.read_list<Registrar>(read_registrar);
  return s;
}

RegistrarSet RegistrarSet::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  RegistrarSet s = decode(r);
  r.assert_fully_consumed();
  return s;
}

crypto::Hash RegistrarSet::hash() const { return crypto::hash_tagged(crypto::Domain::kRegistrarSet, encode()); }

std::string RegistrarSet::validate() const {
  if (registrars.size() > kMaxRegistrars) return "registrar set: more than 64 registrars";
  if (registrars.empty() && threshold != 0) return "registrar set: an empty set takes threshold 0";
  if (!registrars.empty() && (threshold == 0 || threshold > registrars.size()))
    return "registrar set: threshold must be between 1 and the number of registrars";
  std::set<std::pair<std::uint8_t, Bytes>> seen;
  for (const Registrar& r : registrars) {
    const SigAlgo algo = static_cast<SigAlgo>(r.sig_algo);
    const bool fixed = algo == SigAlgo::kEd25519 || algo == SigAlgo::kSlhDsa;
    if (!fixed && algo != SigAlgo::kHybridEd25519MlDsa) return "registrar set: unknown signature scheme";
    if (fixed && r.public_key.size() != 32) return "registrar set: this scheme takes a 32 byte public key";
    if (!fixed && (r.public_key.empty() || r.public_key.size() > kMaxHybridKey))
      return "registrar set: hybrid public key size out of range";
    if (!seen.insert({r.sig_algo, r.public_key}).second) return "registrar set: repeated registrar key";
  }
  return {};
}

std::uint8_t subchain_policy_format(crypto::ByteView policy) {
  if (policy.empty()) return 0;
  if (policy.size() < 3 || policy[0] != 0 || policy[1] != 0 || policy[2] != 0) return 1;
  if (policy.size() < 4 || policy[3] < kDynamicFormat) return 0xFF;
  return policy[3];
}

Bytes DynamicSubchainPolicy::encode() const {
  codec::CodecWriter w;
  w.write_u8(0);
  w.write_u8(0);
  w.write_u8(0);
  w.write_u8(kDynamicFormat);
  w.write_u8(max_depth);
  w.write_u32(max_children);
  w.write_u16(max_anchors_per_block);
  w.write_u16(max_headers_per_anchor);
  w.write_u16(max_ops_per_block);
  w.write_u8(authority.threshold);
  w.write_list(authority.registrars, write_registrar);
  w.write_list(children, write_registration);
  return w.take();
}

DynamicSubchainPolicy DynamicSubchainPolicy::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  for (int i = 0; i < 3; ++i)
    if (r.read_u8() != 0) throw codec::CodecError("dynamic subchain policy: missing format prefix");
  if (r.read_u8() != kDynamicFormat) throw codec::CodecError("dynamic subchain policy: unknown format");
  DynamicSubchainPolicy p;
  p.max_depth = r.read_u8();
  p.max_children = r.read_u32();
  p.max_anchors_per_block = r.read_u16();
  p.max_headers_per_anchor = r.read_u16();
  p.max_ops_per_block = r.read_u16();
  p.authority = RegistrarSet::decode(r);
  p.children = r.read_list<ChildRegistration>([](codec::CodecReader& rr) {
    const Bytes e = rr.read_bytes();
    return decode_registration(bview(e));
  });
  r.assert_fully_consumed();
  return p;
}

crypto::Hash DynamicSubchainPolicy::hash() const {
  return crypto::hash_tagged(crypto::Domain::kSubchainPolicy, encode());
}

std::string DynamicSubchainPolicy::validate(std::uint64_t own_chain_id) const {
  if (max_depth == 0) return "dynamic subchain policy: max_depth must be >= 1";
  if (std::string e = authority.validate(); !e.empty()) return "dynamic subchain policy: " + e;
  if (max_children != 0 && children.size() > max_children)
    return "dynamic subchain policy: more genesis children than max_children";
  for (std::size_t i = 0; i < children.size(); ++i) {
    if (i > 0 && !(children[i - 1].chain_id < children[i].chain_id))
      return "dynamic subchain policy: children must be ascending, unique by chain_id";
    if (std::string e = registration_error(children[i], own_chain_id, max_depth); !e.empty())
      return "dynamic subchain policy: " + e;
  }
  return {};
}

void DynamicSubchainPolicy::commit_to(GenesisSpec& g) const { g.subchain_policy = encode(); }

std::optional<DynamicSubchainPolicy> DynamicSubchainPolicy::from_genesis(const GenesisSpec& g) {
  const crypto::ByteView bytes(g.subchain_policy.data(), g.subchain_policy.size());
  if (subchain_policy_format(bytes) != kDynamicFormat) return std::nullopt;
  return decode_all(bytes);
}

Bytes CloseChild::encode() const {
  codec::CodecWriter w;
  w.write_u64(chain_id);
  w.write_u64(final_height);
  w.write_fixed_bytes(hview(final_tip_hash), 32);
  return w.take();
}

CloseChild CloseChild::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  CloseChild c;
  c.chain_id = r.read_u64();
  c.final_height = r.read_u64();
  c.final_tip_hash = to_hash(r.read_fixed_bytes(32));
  r.assert_fully_consumed();
  return c;
}

namespace {
SubchainOp make_op(std::uint8_t kind, std::uint32_t authority_epoch, std::uint64_t valid_until, Bytes payload) {
  SubchainOp op;
  op.kind = kind;
  op.authority_epoch = authority_epoch;
  op.valid_until = valid_until;
  op.payload = std::move(payload);
  return op;
}
}  // namespace

SubchainOp make_register_op(std::uint32_t authority_epoch, std::uint64_t valid_until,
                            const ChildRegistration& registration) {
  return make_op(SubchainOp::kRegister, authority_epoch, valid_until, registration.encode());
}

SubchainOp make_close_op(std::uint32_t authority_epoch, std::uint64_t valid_until, const CloseChild& close) {
  return make_op(SubchainOp::kClose, authority_epoch, valid_until, close.encode());
}

SubchainOp make_set_registrars_op(std::uint32_t authority_epoch, std::uint64_t valid_until,
                                  const RegistrarSet& next) {
  return make_op(SubchainOp::kSetRegistrars, authority_epoch, valid_until, next.encode());
}

void add_op_approval(SubchainOp& op, std::uint16_t registrar_index, Bytes signature) {
  auto at = op.approvals.begin();
  while (at != op.approvals.end() && at->registrar_index < registrar_index) ++at;
  if (at != op.approvals.end() && at->registrar_index == registrar_index)
    throw std::invalid_argument("registry operation: this registrar already approved");
  op.approvals.insert(at, OpApproval{registrar_index, std::move(signature)});
}

void approve_op(SubchainOp& op, std::uint64_t parent_chain_id, const Hash& parent_genesis_hash,
                std::uint16_t registrar_index, SigAlgo algo, crypto::ByteView secret_key) {
  const Hash digest = op.auth_digest(parent_chain_id, parent_genesis_hash);
  add_op_approval(op, registrar_index, sign_message(algo, hview(digest), secret_key));
}

Bytes ChildRecord::encode() const {
  codec::CodecWriter w;
  w.write_u32(index);
  w.write_u64(chain_id);
  w.write_u8(status);
  w.write_u8(child_max_depth);
  w.write_fixed_bytes(hview(genesis_header_hash), 32);
  w.write_u64(registered_height);
  w.write_u64(closed_height);
  w.write_u64(tip_height);
  w.write_fixed_bytes(hview(tip_hash), 32);
  w.write_fixed_bytes(hview(tip_state_root), 32);
  w.write_fixed_bytes(hview(tip_subchain_root), 32);
  w.write_fixed_bytes(hview(vset_hash), 32);
  w.write_fixed_bytes(hview(tip_vset_hash), 32);
  return w.take();
}

ChildRecord ChildRecord::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  ChildRecord c;
  c.index = r.read_u32();
  c.chain_id = r.read_u64();
  c.status = r.read_u8();
  c.child_max_depth = r.read_u8();
  c.genesis_header_hash = to_hash(r.read_fixed_bytes(32));
  c.registered_height = r.read_u64();
  c.closed_height = r.read_u64();
  c.tip_height = r.read_u64();
  c.tip_hash = to_hash(r.read_fixed_bytes(32));
  c.tip_state_root = to_hash(r.read_fixed_bytes(32));
  c.tip_subchain_root = to_hash(r.read_fixed_bytes(32));
  c.vset_hash = to_hash(r.read_fixed_bytes(32));
  c.tip_vset_hash = to_hash(r.read_fixed_bytes(32));
  r.assert_fully_consumed();
  if (c.status != kOpen && c.status != kClosed) throw codec::CodecError("child record: unknown status");
  if ((c.status == kOpen) != (c.closed_height == 0))
    throw codec::CodecError("child record: closed_height does not fit the status");
  return c;
}

Hash ChildRecord::leaf() const { return crypto::hash_tagged(crypto::Domain::kSubchainLeaf, encode()); }

Hash dynamic_registry_root(std::uint32_t record_count, std::uint32_t open_count, std::uint32_t authority_epoch,
                           const Hash& authority_hash, const Hash& children_root) {
  codec::CodecWriter w;
  w.write_u32(record_count);
  w.write_u32(open_count);
  w.write_u32(authority_epoch);
  w.write_fixed_bytes(hview(authority_hash), 32);
  w.write_fixed_bytes(hview(children_root), 32);
  return crypto::hash_tagged(crypto::Domain::kDynamicRegistry, w.take());
}

Bytes ChildProof::encode() const {
  codec::CodecWriter w;
  const Bytes rec = record.encode();
  w.write_bytes(bview(rec));
  w.write_u32(record_count);
  w.write_u32(open_count);
  w.write_u32(authority_epoch);
  w.write_fixed_bytes(hview(authority_hash), 32);
  w.write_list(siblings, [](codec::CodecWriter& ww, const Hash& h) { ww.write_fixed_bytes(hview(h), 32); });
  return w.take();
}

ChildProof ChildProof::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  ChildProof p;
  const Bytes rec = r.read_bytes();
  p.record = ChildRecord::decode_all(bview(rec));
  p.record_count = r.read_u32();
  p.open_count = r.read_u32();
  p.authority_epoch = r.read_u32();
  p.authority_hash = to_hash(r.read_fixed_bytes(32));
  p.siblings = r.read_list<Hash>([](codec::CodecReader& rr) { return to_hash(rr.read_fixed_bytes(32)); });
  r.assert_fully_consumed();
  return p;
}

std::optional<ChildRecord> verify_child_proof(const Hash& subchain_root, const ChildProof& proof) {
  const ChildRecord& rec = proof.record;
  if (rec.status != ChildRecord::kOpen && rec.status != ChildRecord::kClosed) return std::nullopt;
  if ((rec.status == ChildRecord::kOpen) != (rec.closed_height == 0)) return std::nullopt;
  if (rec.index >= proof.record_count) return std::nullopt;
  if (proof.open_count > proof.record_count) return std::nullopt;
  const std::optional<Hash> children_root =
      merkle::root_from_proof(rec.leaf(), merkle::Proof{rec.index, proof.siblings}, proof.record_count);
  if (!children_root) return std::nullopt;
  if (dynamic_registry_root(proof.record_count, proof.open_count, proof.authority_epoch, proof.authority_hash,
                            *children_root) != subchain_root)
    return std::nullopt;
  return rec;
}

Bytes SubchainProofV2::encode() const {
  codec::CodecWriter w;
  w.write_list(hops, [](codec::CodecWriter& ww, const Hop& h) {
    ww.write_u8(h.kind);
    ww.write_bytes(bview(h.body));
  });
  const Bytes sp = state_proof.encode();
  w.write_bytes(bview(sp));
  return w.take();
}

SubchainProofV2 SubchainProofV2::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  SubchainProofV2 p;
  p.hops = r.read_list<Hop>([](codec::CodecReader& rr) {
    Hop h;
    h.kind = rr.read_u8();
    if (h.kind != kFlatHop && h.kind != kRecordHop) throw codec::CodecError("subchain proof: unknown hop kind");
    h.body = rr.read_bytes();
    return h;
  });
  const Bytes sp = r.read_bytes();
  p.state_proof = StateProof::decode_all(bview(sp));
  r.assert_fully_consumed();
  return p;
}

bool verify_subchain_proof_v2(const Hash& parent_subchain_root, const SubchainProofV2& proof) {
  if (proof.hops.empty()) return false;
  Hash root = parent_subchain_root;
  Hash state_root{};
  try {
    for (const SubchainProofV2::Hop& hop : proof.hops) {
      if (hop.kind == SubchainProofV2::kFlatHop) {
        const SubchainProof::Hop flat = SubchainProof::Hop::decode_all(bview(hop.body));
        const BlockHeader* tip = verify_subchain_hop(root, flat);
        if (tip == nullptr) return false;
        state_root = tip->state_root;
        root = tip->subchain_root;
        continue;
      }
      if (hop.kind != SubchainProofV2::kRecordHop) return false;
      const std::optional<ChildRecord> rec = verify_child_proof(root, ChildProof::decode_all(bview(hop.body)));
      if (!rec) return false;
      state_root = rec->tip_state_root;
      root = rec->tip_subchain_root;
    }
  } catch (const std::exception&) {
    return false;
  }
  return verify_state_proof(state_root, proof.state_proof);
}

DynamicSubchainRegistry DynamicSubchainRegistry::from_policy(std::uint64_t own_chain_id,
                                                             const Hash& own_genesis_hash,
                                                             DynamicSubchainPolicy policy) {
  if (std::string e = policy.validate(own_chain_id); !e.empty()) throw std::invalid_argument(e);
  if (own_genesis_hash == crypto::zero_hash())
    throw std::invalid_argument("dynamic subchain policy: the chain's genesis hash is required");
  DynamicSubchainRegistry r;
  r.own_chain_id_ = own_chain_id;
  r.own_genesis_hash_ = own_genesis_hash;
  r.base_ = std::make_shared<Base>();
  std::vector<Hash> leaves;
  leaves.reserve(policy.children.size());
  for (const ChildRegistration& reg : policy.children) {
    const std::uint32_t index = static_cast<std::uint32_t>(r.base_->children.size());
    r.base_->children.push_back(genesis_child(reg, index, 0));
    r.base_->index_of.emplace(reg.chain_id, index);
    leaves.push_back(r.base_->children.back().record.leaf());
  }
  r.base_->tree.reset(std::move(leaves));
  r.open_count_ = static_cast<std::uint32_t>(r.base_->children.size());
  r.authority_ = std::make_shared<const RegistrarSet>(policy.authority);
  r.authority_hash_ = r.authority_->hash();
  r.policy_ = std::make_shared<const DynamicSubchainPolicy>(std::move(policy));
  return r;
}

std::uint32_t DynamicSubchainRegistry::record_count() const {
  return static_cast<std::uint32_t>(base_->children.size() + appended_.size());
}

const DynamicSubchainRegistry::Child* DynamicSubchainRegistry::child_at(std::uint32_t index) const {
  const std::size_t base = base_->children.size();
  if (index >= base) return index - base < appended_.size() ? &appended_[index - base] : nullptr;
  auto it = changed_.find(index);
  return it != changed_.end() ? &it->second : &base_->children[index];
}

const DynamicSubchainRegistry::Child* DynamicSubchainRegistry::child(std::uint64_t chain_id) const {
  auto it = base_->index_of.find(chain_id);
  if (it != base_->index_of.end()) return child_at(it->second);
  auto added = appended_index_.find(chain_id);
  return added == appended_index_.end() ? nullptr : child_at(added->second);
}

const BlockHeader* DynamicSubchainRegistry::tip_of(std::uint64_t chain_id) const {
  const Child* c = child(chain_id);
  return c == nullptr ? nullptr : &c->tip;
}

DynamicSubchainRegistry::Child& DynamicSubchainRegistry::touch(std::uint32_t index) {
  const std::size_t base = base_->children.size();
  if (index >= base) return appended_.at(index - base);
  auto it = changed_.find(index);
  if (it == changed_.end()) it = changed_.emplace(index, base_->children[index]).first;
  return it->second;
}

merkle::AppendTree::Batch DynamicSubchainRegistry::batch() const {
  merkle::AppendTree::Batch b;
  for (const auto& [index, c] : changed_) b.changes[index] = c.record.leaf();
  b.appended.reserve(appended_.size());
  for (const Child& c : appended_) b.appended.push_back(c.record.leaf());
  return b;
}

void DynamicSubchainRegistry::compact() {
  if (changed_.empty() && appended_.empty()) return;
  if (base_.use_count() > 1) base_ = std::make_shared<Base>(*base_);
  const merkle::AppendTree::Batch b = batch();
  for (auto& [index, c] : changed_) base_->children[index] = std::move(c);
  for (Child& c : appended_) {
    base_->index_of.emplace(c.record.chain_id, c.record.index);
    base_->children.push_back(std::move(c));
  }
  base_->tree.apply(b);
  changed_.clear();
  appended_.clear();
  appended_index_.clear();
}

Hash DynamicSubchainRegistry::root() const {
  return dynamic_registry_root(record_count(), open_count_, epoch_, authority_hash_, base_->tree.root_with(batch()));
}

ChildProof DynamicSubchainRegistry::prove(std::uint64_t chain_id) const {
  const Child* c = child(chain_id);
  if (c == nullptr) throw std::invalid_argument("prove: child not registered");
  ChildProof p;
  p.record = c->record;
  p.record_count = record_count();
  p.open_count = open_count_;
  p.authority_epoch = epoch_;
  p.authority_hash = authority_hash_;
  p.siblings = base_->tree.proof_with(batch(), c->record.index).siblings;
  return p;
}

std::string DynamicSubchainRegistry::apply(std::uint64_t height, const std::vector<SubchainOp>& ops,
                                           const std::vector<Anchor>& anchors) {
  compact();
  const DynamicSubchainPolicy& p = *policy_;
  if (p.max_ops_per_block != 0 && ops.size() > p.max_ops_per_block) return "too many registry operations in block";
  if (p.max_anchors_per_block != 0 && anchors.size() > p.max_anchors_per_block) return "too many anchors in block";
  for (const SubchainOp& op : ops)
    if (std::string e = apply_op(height, op); !e.empty()) return "registry operation rejected: " + e;
  return apply_anchors(anchors);
}

std::string DynamicSubchainRegistry::apply_op(std::uint64_t height, const SubchainOp& op) {
  if (op.kind != SubchainOp::kRegister && op.kind != SubchainOp::kClose && op.kind != SubchainOp::kSetRegistrars)
    return "unknown kind";
  if (op.authority_epoch != epoch_) return "signed for another authority epoch";
  if (op.valid_until != 0 && height > op.valid_until) return "expired";
  const RegistrarSet& authority = *authority_;
  if (authority.registrars.empty()) return "the registry is sealed";
  for (std::size_t i = 0; i < op.approvals.size(); ++i) {
    if (i > 0 && !(op.approvals[i - 1].registrar_index < op.approvals[i].registrar_index))
      return "approvals must be ascending and unique";
    if (op.approvals[i].registrar_index >= authority.registrars.size()) return "approval from an unknown registrar";
  }
  if (op.approvals.size() < authority.threshold) return "not enough approvals";

  std::optional<Child> registered;
  std::optional<std::uint32_t> closing;
  std::shared_ptr<const RegistrarSet> next;
  try {
    if (op.kind == SubchainOp::kRegister) {
      const ChildRegistration reg = decode_registration(bview(op.payload));
      if (std::string e = registration_error(reg, own_chain_id_, policy_->max_depth); !e.empty()) return e;
      if (child(reg.chain_id) != nullptr) return "chain id already registered";
      if (policy_->max_children != 0 && open_count_ >= policy_->max_children) return "max_children reached";
      if (record_count() == kMaxCount) return "the registry is full";
      registered = genesis_child(reg, record_count(), height);
    }
    if (op.kind == SubchainOp::kClose) {
      const CloseChild close = CloseChild::decode_all(bview(op.payload));
      const Child* c = child(close.chain_id);
      if (c == nullptr) return "close of an unregistered child";
      if (c->record.status != ChildRecord::kOpen) return "child already closed";
      if (close.final_height != c->record.tip_height || close.final_tip_hash != c->record.tip_hash)
        return "close does not name the child's recorded tip";
      closing = c->record.index;
    }
    if (op.kind == SubchainOp::kSetRegistrars) {
      RegistrarSet set = RegistrarSet::decode_all(bview(op.payload));
      if (std::string e = set.validate(); !e.empty()) return e;
      if (epoch_ == kMaxCount) return "authority epoch exhausted";
      next = std::make_shared<const RegistrarSet>(std::move(set));
    }
  } catch (const std::exception& e) {
    return std::string("malformed payload: ") + e.what();
  }

  const Hash digest = op.auth_digest(own_chain_id_, own_genesis_hash_);
  for (const OpApproval& a : op.approvals) {
    const Registrar& registrar = authority.registrars[a.registrar_index];
    if (!verify_signature(static_cast<SigAlgo>(registrar.sig_algo), hview(digest), bview(a.signature),
                          bview(registrar.public_key)))
      return "bad registrar signature";
  }

  if (registered) {
    appended_index_.emplace(registered->record.chain_id, registered->record.index);
    appended_.push_back(std::move(*registered));
    ++open_count_;
  }
  if (closing) {
    Child& c = touch(*closing);
    c.record.status = ChildRecord::kClosed;
    c.record.closed_height = height;
    c.vset.reset();
    c.tip_vset.reset();
    --open_count_;
  }
  if (next) {
    authority_ = std::move(next);
    authority_hash_ = authority_->hash();
    ++epoch_;
  }
  return {};
}

std::string DynamicSubchainRegistry::apply_anchors(const std::vector<Anchor>& anchors) {
  const DynamicSubchainPolicy& p = *policy_;
  for (std::size_t i = 0; i < anchors.size(); ++i) {
    const Anchor& a = anchors[i];
    if (i > 0 && !(anchors[i - 1].child_chain_id < a.child_chain_id))
      return "anchors must be ascending, unique by child chain_id";
    if (a.headers.empty()) return "empty anchor";
    if (p.max_headers_per_anchor != 0 && a.headers.size() > p.max_headers_per_anchor)
      return "anchor segment exceeds max_headers_per_anchor";
    const Child* c = child(a.child_chain_id);
    if (c == nullptr) return "anchor for an unregistered child chain";
    if (c->record.status != ChildRecord::kOpen) return "anchor for a closed child chain";
  }
  for (const Anchor& a : anchors) {
    Child& c = touch(child(a.child_chain_id)->record.index);
    client::LightClient lc(c.record.chain_id, *c.vset, c.tip, *c.tip_vset);
    for (const CertifiedHeader& ch : a.headers) {
      consensus::CommitCertificate cert;
      try {
        cert = consensus::CommitCertificate::decode_all(bview(ch.certificate));
      } catch (const std::exception& ex) {
        return std::string("malformed child certificate: ") + ex.what();
      }
      const client::LightClient::Result r = lc.accept(ch.header, cert);
      if (!r.ok) return "child " + std::to_string(a.child_chain_id) + " header rejected: " + r.error;
    }
    const Hash next_hash = lc.validators().hash();
    const Hash certifying_hash = lc.tip_validators().hash();
    std::shared_ptr<const consensus::ValidatorSet> certifying = c.tip_vset;
    if (certifying_hash == c.record.vset_hash) certifying = c.vset;
    if (certifying_hash != c.record.vset_hash && certifying_hash != c.record.tip_vset_hash)
      certifying = std::make_shared<const consensus::ValidatorSet>(lc.tip_validators());
    if (next_hash != c.record.vset_hash) c.vset = std::make_shared<const consensus::ValidatorSet>(lc.validators());
    c.tip_vset = std::move(certifying);
    c.tip = lc.tip();
    c.record.tip_height = c.tip.height;
    c.record.tip_hash = c.tip.hash();
    c.record.tip_state_root = c.tip.state_root;
    c.record.tip_subchain_root = c.tip.subchain_root;
    c.record.vset_hash = next_hash;
    c.record.tip_vset_hash = certifying_hash;
  }
  return {};
}

Bytes DynamicSubchainRegistry::serialize() const {
  codec::CodecWriter w;
  w.write_u32(epoch_);
  w.write_u8(authority_->threshold);
  w.write_list(authority_->registrars, write_registrar);
  const std::uint32_t count = record_count();
  w.write_u32(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const Child& c = *child_at(i);
    w.write_u64(c.record.chain_id);
    w.write_u8(c.record.status);
    w.write_u8(c.record.child_max_depth);
    w.write_fixed_bytes(hview(c.record.genesis_header_hash), 32);
    w.write_u64(c.record.registered_height);
    w.write_u64(c.record.closed_height);
    const Bytes tip = c.tip.encode();
    w.write_bytes(bview(tip));
    if (c.record.status == ChildRecord::kOpen) {
      const Bytes next = c.vset->encode();
      const Bytes certifying = c.tip_vset->encode();
      w.write_bytes(bview(next));
      w.write_bytes(bview(certifying));
      continue;
    }
    w.write_fixed_bytes(hview(c.record.vset_hash), 32);
    w.write_fixed_bytes(hview(c.record.tip_vset_hash), 32);
  }
  return w.take();
}

void DynamicSubchainRegistry::restore(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  const std::uint32_t epoch = r.read_u32();
  RegistrarSet authority = RegistrarSet::decode(r);
  if (std::string e = authority.validate(); !e.empty()) throw std::invalid_argument("subchain snapshot: " + e);
  const std::uint32_t count = r.read_u32();
  if (count > r.remaining()) throw codec::CodecError("subchain snapshot: record count exceeds remaining bytes");

  auto base = std::make_shared<Base>();
  base->children.reserve(count);
  std::vector<Hash> leaves;
  leaves.reserve(count);
  std::uint32_t open = 0;
  for (std::uint32_t i = 0; i < count; ++i) {
    Child c;
    c.record.index = i;
    c.record.chain_id = r.read_u64();
    c.record.status = r.read_u8();
    c.record.child_max_depth = r.read_u8();
    c.record.genesis_header_hash = to_hash(r.read_fixed_bytes(32));
    c.record.registered_height = r.read_u64();
    c.record.closed_height = r.read_u64();
    const Bytes tip = r.read_bytes();
    c.tip = BlockHeader::decode_all(bview(tip));
    if (c.record.status != ChildRecord::kOpen && c.record.status != ChildRecord::kClosed)
      throw std::invalid_argument("subchain snapshot: unknown child status");
    if ((c.record.status == ChildRecord::kOpen) != (c.record.closed_height == 0))
      throw std::invalid_argument("subchain snapshot: closed_height does not fit the status");
    if (c.record.chain_id == own_chain_id_ || c.tip.chain_id != c.record.chain_id)
      throw std::invalid_argument("subchain snapshot: child id mismatch");
    if (c.tip.height == 0 && c.tip.hash() != c.record.genesis_header_hash)
      throw std::invalid_argument("subchain snapshot: an unanchored child's tip must be its genesis header");
    if (c.record.status == ChildRecord::kOpen) {
      const Bytes next = r.read_bytes();
      const Bytes certifying = r.read_bytes();
      c.vset = std::make_shared<const consensus::ValidatorSet>(consensus::ValidatorSet::decode_all(bview(next)));
      c.record.vset_hash = c.vset->hash();
      c.tip_vset = c.vset;
      c.record.tip_vset_hash = c.record.vset_hash;
      if (certifying != next) {
        c.tip_vset =
            std::make_shared<const consensus::ValidatorSet>(consensus::ValidatorSet::decode_all(bview(certifying)));
        c.record.tip_vset_hash = c.tip_vset->hash();
      }
      ++open;
    }
    if (c.record.status == ChildRecord::kClosed) {
      c.record.vset_hash = to_hash(r.read_fixed_bytes(32));
      c.record.tip_vset_hash = to_hash(r.read_fixed_bytes(32));
    }
    c.record.tip_height = c.tip.height;
    c.record.tip_hash = c.tip.hash();
    c.record.tip_state_root = c.tip.state_root;
    c.record.tip_subchain_root = c.tip.subchain_root;
    if (!base->index_of.emplace(c.record.chain_id, i).second)
      throw std::invalid_argument("subchain snapshot: chain id registered twice");
    leaves.push_back(c.record.leaf());
    base->children.push_back(std::move(c));
  }
  r.assert_fully_consumed();

  base->tree.reset(std::move(leaves));
  base_ = std::move(base);
  changed_.clear();
  appended_.clear();
  appended_index_.clear();
  authority_ = std::make_shared<const RegistrarSet>(std::move(authority));
  authority_hash_ = authority_->hash();
  epoch_ = epoch;
  open_count_ = open;
}

}  // namespace aviary
