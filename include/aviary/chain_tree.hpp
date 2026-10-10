#pragma once

#include <stdexcept>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aviary/block_store.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/genesis.hpp"
#include "aviary/subchain.hpp"
#include "aviary/types.hpp"

/// A TREE of chains, declared once and built together (docs/design/SUBCHAINS.md
/// + composable chains). Each node is a chain with its own MIXTURE (consensus
/// engine, state model, proposer — built-in or a registered custom/foreign
/// component), its own funded accounts and validators, and any number of child
/// chains; a child's finalized headers are anchored into its parent. The tree
/// resolves every ordering constraint for you:
///   1. genesis bottom-up — a parent registers each child by its genesis HEADER,
///   2. bindings top-down  — each child commits its parent's genesis hash,
///   3. assembly           — every node becomes a running engine,
/// and then drives the whole family in-process: produce() on any node proposes,
/// justifies (BFT certificate / PoS seal / PoW nonce) and submits a block that
/// automatically ANCHORS whatever its children finalized since the last one,
/// and prove() yields a cross-chain proof of any descendant's account from the
/// root header. Extend by subclassing (select_txs / on_finalized hooks) or by
/// naming custom components in a node's blueprint. Deterministic: same spec +
/// same timestamps → same headers on every machine.
namespace aviary::node {

struct ChainTreeSpec {
  struct Validator {
    Bytes bls_public;  ///< 48 B
    Bytes bls_secret;  ///< 32 B — empty for a validator this process does not hold (it cannot certify then)
    std::uint64_t power = 1;
    bool operator==(const Validator&) const = default;
  };
  struct Node {
    std::uint64_t chain_id = 0;
    ChainBlueprint blueprint{"bft", "account", "", {}};  ///< children MUST be BFT (anchoring); any engine may be a parent
    std::uint16_t assurance_level = 1;                   ///< 1 standard / 2 hardened / 3 fortress
    std::vector<GenesisAccount> accounts;                ///< funded at genesis (account model)
    std::vector<Validator> validators;                   ///< index-significant
    std::uint16_t max_anchors_per_block = 0;             ///< 0 = unlimited
    std::uint16_t max_headers_per_anchor = 0;            ///< 0 = unlimited
    std::vector<Node> children;

    Bytes encode() const;
    static Node decode(codec::CodecReader& r, unsigned depth = 0);
    bool operator==(const Node&) const = default;
  };
  Node root;

  /// Canonical encoding (recursive); decode_all is strict.
  Bytes encode() const;
  static ChainTreeSpec decode_all(crypto::ByteView bytes);
  /// Structural validity: unique chain ids, ≥1 validator per node, children
  /// are BFT, depth ≤ 8. Empty string = valid.
  std::string validate() const;
};

class ChainTree {
 public:
  /// A built node: its final genesis (policy + binding committed), engine,
  /// archive of finalized (block, cert) for BFT nodes, and tree position.
  struct Built {
    std::uint64_t chain_id = 0;
    std::uint64_t parent_id = 0;                 ///< 0 = root
    std::uint8_t depth = 0;                      ///< levels below this node
    GenesisSpec genesis;
    std::vector<ChainTreeSpec::Validator> validators;
    AssembledChain chain;
    BlockStore archive;                          ///< BFT nodes only (what a parent anchors)
    std::vector<std::uint64_t> children;
  };

  /// Build the whole family. Throws std::invalid_argument on an invalid spec
  /// or a mixture the registry / posture rejects (fail-closed, nothing built).
  static std::unique_ptr<ChainTree> build(const ChainTreeSpec& spec);
  virtual ~ChainTree() = default;
  ChainTree(ChainTree&&) = default;             // move-only (owns engines)
  ChainTree& operator=(ChainTree&&) = default;
  ChainTree(const ChainTree&) = delete;
  ChainTree& operator=(const ChainTree&) = delete;

  std::uint64_t root_id() const { return root_; }
  std::vector<std::uint64_t> chain_ids() const;               ///< pre-order (parents before children)
  const Built& node(std::uint64_t chain_id) const;
  bool has(std::uint64_t chain_id) const { return nodes_.count(chain_id) != 0; }
  std::uint64_t height(std::uint64_t chain_id) const { return node(chain_id).chain.engine->height(); }

  /// Produce + finalize the next block on [chain_id] at [timestamp]: proposes
  /// with select_txs() and with every pending anchor from its children,
  /// justifies (BFT: certificate from the validators this process holds — must
  /// reach quorum; PoS/VRF: seal; PoW: mine), submits, archives. Returns the
  /// finalized block. Throws with the reason on any refusal.
  Block produce(std::uint64_t chain_id, std::uint64_t timestamp);

  /// The anchors [chain_id] would include right now (what produce() will anchor).
  std::vector<Anchor> pending_anchors_for(std::uint64_t chain_id) const;

  /// Cross-chain proof of [address] on [chain_id] from the ROOT's current
  /// header (hops root → … → chain_id + the state proof). Verify with
  /// `verify_subchain_proof(root_tip().subchain_root, proof)`; for the root
  /// itself the proof has no hops — verify its state proof directly.
  SubchainProof prove_account(std::uint64_t chain_id, const Hash& address) const;
  const BlockHeader& root_tip() const { return node(root_).chain.engine->tip(); }
  /// True iff [proof] is committed by the root's CURRENT header (or, for a
  /// hop-less proof, holds under the root's state root).
  bool verify(const SubchainProof& proof) const;

 protected:
  ChainTree() = default;
  /// Hooks for subclasses. Defaults: no transactions; no-op.
  virtual std::vector<Transaction> select_txs(const Built& /*node*/, std::uint64_t /*timestamp*/) { return {}; }
  virtual void on_finalized(const Built& /*node*/, const Block& /*block*/) {}

 private:
  Built& mut(std::uint64_t chain_id);
  std::map<std::uint64_t, Built> nodes_;
  std::uint64_t root_ = 0;
  std::vector<std::uint64_t> order_;  // pre-order
};

}  // namespace aviary::node
