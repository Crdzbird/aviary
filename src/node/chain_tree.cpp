#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include "aviary/chain_tree.hpp"

#include <set>
#include <stdexcept>

#include "aviary/account_state.hpp"
#include "aviary/assurance.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/codec.hpp"
#include "aviary/pos_engine.hpp"
#include "aviary/pow_engine.hpp"
#include "aviary/subchain_relay.hpp"
#include "aviary/vrf_pos.hpp"

namespace aviary::node {

namespace {
codec::ByteView bv(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hv(const Hash& h) { return codec::ByteView(h.data(), h.size()); }
Hash to_hash(const Bytes& b) { Hash h{}; for (std::size_t i = 0; i < 32 && i < b.size(); ++i) h[i] = b[i]; return h; }
constexpr unsigned kMaxDepth = 8;
}  // namespace

// ── Spec codec ───────────────────────────────────────────────────────────────

Bytes ChainTreeSpec::Node::encode() const {
  codec::CodecWriter w;
  w.write_u64(chain_id);
  Bytes bp = blueprint.encode();
  w.write_bytes(bv(bp));
  w.write_u16(assurance_level);
  w.write_list(accounts, [](codec::CodecWriter& w, const GenesisAccount& a) {
    w.write_fixed_bytes(hv(a.address), 32);
    w.write_u128(a.balance);
  });
  w.write_list(validators, [](codec::CodecWriter& w, const Validator& v) {
    w.write_bytes(bv(v.bls_public));
    w.write_bytes(bv(v.bls_secret));
    w.write_u64(v.power);
  });
  w.write_u16(max_anchors_per_block);
  w.write_u16(max_headers_per_anchor);
  w.write_list(children, [](codec::CodecWriter& w, const Node& c) {
    Bytes e = c.encode();
    w.write_bytes(bv(e));
  });
  return w.take();
}

ChainTreeSpec::Node ChainTreeSpec::Node::decode(codec::CodecReader& r, unsigned depth) {
  if (depth > kMaxDepth) throw codec::CodecError("chain tree: nesting too deep");
  Node n;
  n.chain_id = r.read_u64();
  Bytes bp = r.read_bytes();
  n.blueprint = ChainBlueprint::decode_all(bv(bp));
  n.assurance_level = r.read_u16();
  n.accounts = r.read_list<GenesisAccount>([](codec::CodecReader& r) {
    GenesisAccount a;
    a.address = to_hash(r.read_fixed_bytes(32));
    a.balance = r.read_u128();
    return a;
  });
  n.validators = r.read_list<Validator>([](codec::CodecReader& r) {
    Validator v;
    v.bls_public = r.read_bytes();
    v.bls_secret = r.read_bytes();
    v.power = r.read_u64();
    return v;
  });
  n.max_anchors_per_block = r.read_u16();
  n.max_headers_per_anchor = r.read_u16();
  n.children = r.read_list<Node>([depth](codec::CodecReader& r) {
    Bytes e = r.read_bytes();
    codec::CodecReader cr(bv(e));
    Node c = Node::decode(cr, depth + 1);
    cr.assert_fully_consumed();
    return c;
  });
  return n;
}

Bytes ChainTreeSpec::encode() const { return root.encode(); }

ChainTreeSpec ChainTreeSpec::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  ChainTreeSpec s;
  s.root = Node::decode(r);
  r.assert_fully_consumed();
  return s;
}

std::string ChainTreeSpec::validate() const {
  std::set<std::uint64_t> ids;
  std::string err;
  std::function<void(const Node&, unsigned, bool)> walk = [&](const Node& n, unsigned depth, bool is_child) {
    if (!err.empty()) return;
    if (depth > kMaxDepth) { err = "chain tree: nesting deeper than 8"; return; }
    if (!ids.insert(n.chain_id).second) { err = "chain tree: duplicate chain id " + std::to_string(n.chain_id); return; }
    if (n.validators.empty()) { err = "chain tree: chain " + std::to_string(n.chain_id) + " has no validators"; return; }
    for (const auto& v : n.validators) {
      if (v.bls_public.size() != 48) { err = "chain tree: validator key must be 48 bytes"; return; }
      if (!v.bls_secret.empty() && v.bls_secret.size() != 32) { err = "chain tree: validator secret must be 32 bytes or empty"; return; }
      if (v.power == 0) { err = "chain tree: zero-power validator"; return; }
    }
    if (is_child && n.blueprint.consensus_id != "bft") {
      err = "chain tree: child chain " + std::to_string(n.chain_id) + " must be bft (only certificate-finalized chains can be anchored)";
      return;
    }
    if (n.blueprint.state_model_id != "account" && !n.accounts.empty()) {
      err = "chain tree: accounts need the account state model"; return;
    }
    for (const Node& c : n.children) walk(c, depth + 1, true);
  };
  walk(root, 0, false);
  return err;
}

// ── Build ────────────────────────────────────────────────────────────────────

namespace {
struct Draft {
  const ChainTreeSpec::Node* spec;
  GenesisSpec genesis;      // without binding (its header is final)
  std::uint8_t depth = 0;
};

GenesisSpec base_genesis(const ChainTreeSpec::Node& n) {
  GenesisSpec g;
  g.chain_id = n.chain_id;
  g.timestamp = 1;
  g.state_model = n.blueprint.state_model_id == "utxo" ? StateModel::kUtxo : StateModel::kAccount;
  g.accounts = n.accounts;
  for (const auto& v : n.validators) g.validators.push_back(consensus::Validator{v.bls_public, v.power});
  switch (n.assurance_level) {
    case 1: AssuranceProfile::standard().apply_to(g); break;
    case 2: AssuranceProfile::hardened().apply_to(g); break;
    case 3: AssuranceProfile::fortress().apply_to(g); break;
    default: throw std::invalid_argument("chain tree: assurance_level must be 1, 2 or 3");
  }
  n.blueprint.commit_to(g);
  return g;
}
}  // namespace

std::unique_ptr<ChainTree> ChainTree::build(const ChainTreeSpec& spec) {
  if (std::string e = spec.validate(); !e.empty()) throw std::invalid_argument(e);
  std::unique_ptr<ChainTree> t(new ChainTree());
  t->root_ = spec.root.chain_id;

  // Pass 1 (bottom-up): genesis WITHOUT binding, so each child's genesis header
  // is final before its parent registers it.
  std::map<std::uint64_t, Draft> drafts;
  std::function<std::uint8_t(const ChainTreeSpec::Node&)> pass1 = [&](const ChainTreeSpec::Node& n) -> std::uint8_t {
    std::uint8_t depth = 0;
    std::vector<ChildRegistration> regs;
    for (const ChainTreeSpec::Node& c : n.children) {
      std::uint8_t cd = pass1(c);
      depth = std::max<std::uint8_t>(depth, static_cast<std::uint8_t>(cd + 1));
      ChildRegistration reg;
      reg.chain_id = c.chain_id;
      reg.genesis_header = ledger_from_genesis(drafts.at(c.chain_id).genesis)->tip();
      for (const auto& v : c.validators) reg.validators.push_back(consensus::Validator{v.bls_public, v.power});
      reg.child_max_depth = cd;
      regs.push_back(std::move(reg));
    }
    std::sort(regs.begin(), regs.end(), [](const ChildRegistration& a, const ChildRegistration& b) { return a.chain_id < b.chain_id; });
    GenesisSpec g = base_genesis(n);
    if (!regs.empty()) {
      SubchainPolicy p;
      p.max_children = static_cast<std::uint16_t>(regs.size());
      p.max_depth = depth;
      p.max_anchors_per_block = n.max_anchors_per_block;
      p.max_headers_per_anchor = n.max_headers_per_anchor;
      p.children = std::move(regs);
      if (std::string e = p.validate(n.chain_id); !e.empty()) throw std::invalid_argument(e);
      p.commit_to(g);
    }
    drafts[n.chain_id] = Draft{&n, std::move(g), depth};
    return depth;
  };
  pass1(spec.root);

  // Pass 2 (top-down): bind children to their parent's FINAL genesis hash, then
  // assemble. Bindings change a child's hash, never its header (already registered).
  std::function<void(const ChainTreeSpec::Node&, std::uint64_t, const Hash*)> pass2 =
      [&](const ChainTreeSpec::Node& n, std::uint64_t parent, const Hash* parent_hash) {
        Draft& d = drafts.at(n.chain_id);
        if (parent_hash) ParentBinding{parent, *parent_hash}.commit_to(d.genesis);
        // Assemble as the first validator this process holds a secret for.
        std::uint32_t me = 0;
        crypto::KeyPair key;
        for (std::uint32_t i = 0; i < n.validators.size(); ++i)
          if (!n.validators[i].bls_secret.empty()) { me = i; key = crypto::KeyPair{n.validators[i].bls_public, n.validators[i].bls_secret}; break; }
        if (key.public_key.empty()) key = crypto::KeyPair{n.validators[0].bls_public, {}};
        AssembledChain chain = ChainAssembler::assemble(d.genesis, me, key);  // throws → fail-closed
        std::vector<std::uint64_t> kids;
        for (const auto& c : n.children) kids.push_back(c.chain_id);
        Built b{n.chain_id, parent, d.depth, d.genesis, n.validators, std::move(chain), BlockStore{}, std::move(kids)};
        const Hash my_hash = d.genesis.hash();
        t->nodes_.emplace(n.chain_id, std::move(b));
        t->order_.push_back(n.chain_id);
        for (const ChainTreeSpec::Node& c : n.children) pass2(c, n.chain_id, &my_hash);
      };
  pass2(spec.root, 0, nullptr);
  return t;
}

// ── Queries ──────────────────────────────────────────────────────────────────

std::vector<std::uint64_t> ChainTree::chain_ids() const { return order_; }

const ChainTree::Built& ChainTree::node(std::uint64_t chain_id) const {
  auto it = nodes_.find(chain_id);
  if (it == nodes_.end()) throw std::out_of_range("chain tree: unknown chain id " + std::to_string(chain_id));
  return it->second;
}
ChainTree::Built& ChainTree::mut(std::uint64_t chain_id) {
  auto it = nodes_.find(chain_id);
  if (it == nodes_.end()) throw std::out_of_range("chain tree: unknown chain id " + std::to_string(chain_id));
  return it->second;
}

std::vector<Anchor> ChainTree::pending_anchors_for(std::uint64_t chain_id) const {
  const Built& n = node(chain_id);
  const SubchainRegistry* reg = n.chain.engine->ledger().subchains();
  if (!reg) return {};
  std::vector<std::pair<std::uint64_t, const BlockStore*>> kids;
  for (std::uint64_t c : n.children) kids.emplace_back(c, &node(c).archive);
  return pending_anchors(*reg, kids);
}

// ── Produce ──────────────────────────────────────────────────────────────────

Block ChainTree::produce(std::uint64_t chain_id, std::uint64_t timestamp) {
  Built& n = mut(chain_id);
  consensus::ConsensusEngine& eng = *n.chain.engine;
  Block b = eng.propose_anchored(select_txs(n, timestamp), timestamp, pending_anchors_for(chain_id));

  if (auto* bft = dynamic_cast<consensus::BftEngine*>(&eng)) {
    // Certify with every validator this process holds a secret for, under the
    // set in force; refuse loudly if that does not reach quorum.
    const consensus::ValidatorSet& vs = bft->validators();
    std::vector<consensus::Vote> votes;
    for (const auto& v : n.validators) {
      if (v.bls_secret.empty()) continue;
      if (auto idx = vs.index_of(v.bls_public)) votes.push_back(bft->precommit(b, *idx, crypto::KeyPair{v.bls_public, v.bls_secret}));
    }
    auto cert = bft->try_commit(b, votes);
    if (!cert) throw std::runtime_error("chain tree: the validators held locally do not reach quorum on chain " + std::to_string(chain_id));
    if (BlockError e = bft->finalize(b, *cert); !e.ok) throw std::runtime_error("chain tree: finalize refused: " + e.error);
    if (!n.archive.put(b, *cert)) throw std::runtime_error("chain tree: archive refused the block");
  } else if (auto* pos = dynamic_cast<consensus::PosEngine*>(&eng)) {
    if (BlockError e = eng.submit(b, bv(pos->seal(b))); !e.ok) throw std::runtime_error("chain tree: submit refused: " + e.error);
  } else if (auto* vrf = dynamic_cast<consensus::VrfPosEngine*>(&eng)) {
    if (BlockError e = eng.submit(b, bv(vrf->seal(b))); !e.ok) throw std::runtime_error("chain tree: submit refused: " + e.error);
  } else if (auto* pow = dynamic_cast<consensus::PowEngine*>(&eng)) {
    if (!pow->mine(b)) throw std::runtime_error("chain tree: no proof-of-work solution within the bound");
    if (BlockError e = eng.submit(b, {}); !e.ok) throw std::runtime_error("chain tree: submit refused: " + e.error);
  } else {
    throw std::runtime_error("chain tree: engine cannot be justified in-process (register its justify via a foreign engine and drive it directly)");
  }
  on_finalized(n, b);
  return b;
}

// ── Proofs ───────────────────────────────────────────────────────────────────

SubchainProof ChainTree::prove_account(std::uint64_t chain_id, const Hash& address) const {
  const Built& target = node(chain_id);
  // Path root → target: collect ancestors, then hops from the top down.
  std::vector<std::uint64_t> path;
  for (std::uint64_t id = chain_id; id != 0; id = node(id).parent_id) path.push_back(id);
  SubchainProof p;
  for (std::size_t i = path.size(); i-- > 1;) {           // path[i] is the parent of path[i-1]
    const SubchainRegistry* reg = node(path[i]).chain.engine->ledger().subchains();
    if (!reg) throw std::runtime_error("chain tree: ancestor is not a parent");
    p.hops.push_back(reg->hop(path[i - 1]));
  }
  auto* acc = dynamic_cast<const AccountStateMachine*>(&target.chain.engine->ledger().state());
  if (!acc) throw std::runtime_error("chain tree: chain " + std::to_string(chain_id) + " is not an account-model chain");
  p.state_proof = acc->prove_account(address);
  return p;
}

bool ChainTree::verify(const SubchainProof& proof) const {
  const BlockHeader& tip = root_tip();
  if (proof.hops.empty()) return verify_state_proof(tip.state_root, proof.state_proof);
  return verify_subchain_proof(tip.subchain_root, proof);
}

}  // namespace aviary::node
