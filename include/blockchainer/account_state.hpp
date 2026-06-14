#pragma once

#include <cstdint>
#include <map>

#include "blockchainer/codec.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/state.hpp"

namespace blockchainer {

/// Account-model state machine (ARCHITECTURE §5). State is a map
/// `address → {balance, nonce}`, committed as a Merkle root over canonically
/// encoded, address-sorted account leaves. Replay protection is a monotonic
/// per-account nonce.
///
/// Transaction body layout (canonical): `u64 nonce ‖ fixed32 recipient ‖ u128 amount`.
class AccountStateMachine : public StateMachine {
 public:
  struct Account {
    codec::u128 balance = 0;
    std::uint64_t nonce = 0;
  };

  /// Genesis helper: credit an address before the chain starts.
  void credit_genesis(const Hash& address, codec::u128 amount);

  Account account(const Hash& address) const;

  StateModel model() const override { return StateModel::kAccount; }
  Hash state_root() const override;
  ValidationResult validate(const Transaction& tx) const override;
  void apply(const Transaction& tx) override;
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
  // std::map keeps keys sorted → deterministic state_root.
  std::map<Hash, Account> accounts_;
};

}  // namespace blockchainer
