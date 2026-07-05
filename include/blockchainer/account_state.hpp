#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/merkle.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/state.hpp"

namespace blockchainer {

/// Account-model state machine (ARCHITECTURE §5). State is a map
/// `address → {balance, nonce}`, committed as a Merkle root over canonically
/// encoded, address-sorted account leaves. Replay protection is a monotonic
/// per-account nonce.
///
/// Transaction body layout (canonical): `u64 nonce ‖ fixed32 recipient ‖ u128 amount`.
///
/// **Commitment performance (copy-on-write).** The committed accounts and their
/// cached Merkle tree live in an immutable, shared `Snapshot`; uncommitted
/// changes accumulate in a small `overlay_`. So `clone()` — done once per block
/// to apply it atomically — shares the snapshot and copies only the overlay
/// (O(changed accounts)), not the whole state. `state_root()` recomputes only
/// the changed leaves' root-paths against the cached tree (O(K·log N)) when the
/// block merely moves value between existing accounts, and rebuilds fully only
/// when new accounts appear or the overlay grows large. The committed root is
/// byte-identical to `merkle::compute_root` over the sorted leaves.
class AccountStateMachine : public StateMachine {
 public:
  struct Account {
    codec::u128 balance = 0;
    std::uint64_t nonce = 0;
  };

  /// [flat_fee] is a fixed protocol fee burned from the sender on every applied
  /// transfer (a chain parameter, not committed per-block state). 0 (the default)
  /// is a feeless chain and leaves the state transition byte-identical to a chain
  /// with no fee concept — so existing golden vectors are unaffected.
  explicit AccountStateMachine(codec::u128 flat_fee = 0);

  codec::u128 flat_fee() const { return flat_fee_; }

  /// Genesis helper: credit an address before the chain starts.
  void credit_genesis(const Hash& address, codec::u128 amount);

  Account account(const Hash& address) const;

  StateModel model() const override { return StateModel::kAccount; }
  Hash state_root() const override;
  ValidationResult validate(const Transaction& tx) const override;
  void apply(const Transaction& tx) override;
  codec::u128 tx_fee(const Transaction& tx) const override;
  std::unique_ptr<StateMachine> clone() const override;

  /// Decodes the account-model transaction body. Throws codec::CodecError on a
  /// malformed body.
  struct Transfer {
    std::uint64_t nonce = 0;
    Hash recipient{};
    codec::u128 amount = 0;
  };
  static Transfer decode_transfer(const Bytes& body);
  static Bytes encode_transfer(const Transfer& t);

  /// A state-inclusion proof that [address] is committed under state_root().
  /// present=false (no proof) if the address has no account.
  StateProof prove_account(const Hash& address) const;

  /// Decode the value committed in an account leaf preimage.
  struct AccountLeaf {
    Hash address{};
    codec::u128 balance = 0;
    std::uint64_t nonce = 0;
  };
  static AccountLeaf decode_account_leaf(const Bytes& preimage);

 private:
  // An immutable, shareable committed baseline: the accounts (sorted by
  // address), the same addresses in leaf order, and the cached Merkle tree over
  // their leaf hashes. Shared across clones via shared_ptr — never mutated once
  // published; a new Snapshot is built by seal().
  struct Snapshot {
    std::map<Hash, Account> accounts;    // std::map → deterministic sorted order
    std::vector<Hash> sorted_addrs;      // accounts' keys, in leaf order
    merkle::MerkleIndex tree;            // over leaf hashes, in the same order
  };

  // Committed baseline (shared) plus uncommitted absolute-valued changes.
  // Both are mutable so the const commitment path (state_root/prove_account) can
  // fold the overlay into a fresh snapshot (logical constness: the value is
  // unchanged, only the cache is materialised). Single-threaded, like the rest
  // of the node.
  mutable std::shared_ptr<const Snapshot> base_;
  mutable std::map<Hash, Account> overlay_;
  codec::u128 flat_fee_ = 0;  // burned per transfer; a fixed chain parameter

  const Account* lookup(const Hash& address) const;  // overlay-then-base
  void put(const Hash& address, const Account& acc); // write to overlay
  void seal() const;                                 // fold overlay → new base_
  std::map<Hash, Account> merged() const;            // base ∪ overlay
};

}  // namespace blockchainer
