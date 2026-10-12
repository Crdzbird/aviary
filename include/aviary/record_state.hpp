#pragma once

#include <cstdint>
#include <map>
#include <memory>

#include "aviary/codec.hpp"
#include "aviary/proofs.hpp"
#include "aviary/state.hpp"

namespace aviary {

/// Record model state machine: a signed key value store for application
/// objects that are not money. A record is a 32 byte key chosen by the
/// application (for example BLAKE3 of an identifier), an owner (the address of
/// the author that created it), a version that grows by one per write, and an
/// opaque value of up to kMaxValue bytes (JSON, protobuf, a hash, anything).
/// Only the owner may update, transfer or delete a record; replay protection is
/// a per author nonce, as in the account model.
///
/// Transaction body (canonical): `u64 nonce || fixed32 key || u8 op || bytes value`
/// where op is kPut (create or update; value is the new content), kTransfer
/// (value is the 32 byte new owner address) or kDelete (value is empty).
///
/// State root: Merkle root over `0x01 || key || owner || u64 version || bytes
/// value` per record in key order, then `0x02 || address || u64 nonce` per
/// author in address order.
class RecordStateMachine : public StateMachine {
 public:
  enum Op : std::uint8_t { kPut = 0, kTransfer = 1, kDelete = 2 };
  static constexpr std::size_t kMaxValue = 65536;

  struct Record {
    Hash owner{};
    std::uint64_t version = 0;
    Bytes value;
  };

  struct Write {
    std::uint64_t nonce = 0;
    Hash key{};
    std::uint8_t op = kPut;
    Bytes value;
  };

  struct RecordLeaf {
    Hash key{};
    Hash owner{};
    std::uint64_t version = 0;
    Bytes value;
  };

  RecordStateMachine() = default;

  /// The record at [key], or null when none exists.
  const Record* record(const Hash& key) const;
  /// The next nonce [address] must use.
  std::uint64_t nonce(const Hash& address) const;
  std::size_t size() const { return records_.size(); }

  StateModel model() const override { return StateModel::kRecord; }
  Hash state_root() const override;
  ValidationResult validate(const Transaction& tx) const override;
  void apply(const Transaction& tx) override;
  std::unique_ptr<StateMachine> clone() const override;
  Bytes serialize() const override;
  static std::unique_ptr<RecordStateMachine> deserialize(codec::ByteView bytes);

  static Write decode_write(const Bytes& body);
  static Bytes encode_write(const Write& w);

  /// A state inclusion proof that [key] is committed under state_root();
  /// present is false when no such record exists.
  StateProof prove_record(const Hash& key) const;
  /// Reads the record committed in a record leaf preimage. Throws on a nonce
  /// leaf or a malformed preimage.
  static RecordLeaf decode_record_leaf(const Bytes& preimage);

 private:
  std::map<Hash, Record> records_;
  std::map<Hash, std::uint64_t> nonces_;

  std::vector<Bytes> leaf_preimages() const;
};

}  // namespace aviary
