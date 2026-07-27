#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"

/// Peer discovery + address book (ARCHITECTURE §9). The transport needs to know,
/// for each validator, where to reach it (host:port) and which keys authenticate
/// it (the KEM key to encapsulate to, the signing key to pin). Rather than every
/// node hard-coding the full set, a joining node starts from a seed and *learns*
/// the rest by exchanging address books (a pull-based peer-exchange / PEX).
///
/// Records are **self-signed**: the owner signs its record's canonical bytes
/// (domain-tagged `kPeerRecord`) with the same hybrid signature key the secure
/// session authenticates with. `merge` — the PEX trust boundary — verifies every
/// incoming record and rejects a forgery outright, so an attacker cannot invent
/// or alter a record for a key it doesn't hold; the most it can replay is a
/// record the owner really signed. On top of that, merge is
/// **first-writer-wins**: a known id is never overwritten, so even a *validly
/// signed* newer record can't rebind an identity mid-session. (`add` is the
/// local/trusted path — seeding yourself or configured peers — and does not
/// verify.) Serialize with the same deterministic codec as everything else, so
/// two nodes that have learned the same peers hold byte-identical books.
namespace blockchainer::p2p {

/// Everything needed to dial + authenticate one validator, self-signed.
struct PeerRecord {
  std::uint32_t id = 0;
  std::string host;
  std::uint16_t port = 0;
  Bytes kem_pk;      ///< hybrid KEM public key (to open a secure session to it)
  Bytes sig_pk;      ///< hybrid signing public key (identity / allow-list)
  Bytes prekey;      ///< ratchet prekey public key ([ArmoredChannel]); may be empty
  Bytes signature;   ///< owner's hybrid signature over signing_hash()

  bool operator==(const PeerRecord&) const = default;

  /// Domain-tagged hash of the canonical unsigned fields — what the owner signs.
  Hash signing_hash() const;
  /// Sign this record with the owner's hybrid signature secret key (must match
  /// [sig_pk]); fills [signature].
  void sign(const Bytes& sig_secret_key);
  /// Verify [signature] against this record's own [sig_pk]. Never throws.
  bool verify() const;
};

class AddressBook {
 public:
  AddressBook() = default;

  /// Trusted local insert (your own record, configured seeds). Does NOT verify.
  void add(const PeerRecord& r) { peers_[r.id] = r; }
  bool contains(std::uint32_t id) const { return peers_.count(id) != 0; }
  /// The record for [id], or nullptr if unknown.
  const PeerRecord* get(std::uint32_t id) const;
  std::size_t size() const { return peers_.size(); }
  /// All records in canonical (ascending-id) order.
  std::vector<PeerRecord> all() const;

  /// PEX trust boundary: learn records we don't already have, **verifying each
  /// signature** — a record that doesn't verify is rejected. First-writer-wins:
  /// an id we already know is left untouched. Returns how many were newly added.
  std::size_t merge(const AddressBook& other);
  std::size_t merge(const std::vector<PeerRecord>& records);

  /// Canonical serialization (records in ascending-id order).
  Bytes encode() const;
  /// Strict inverse; throws codec::CodecError on malformed / non-canonical input
  /// (out-of-order or duplicate ids, trailing bytes). Does NOT verify signatures
  /// — decode-then-merge is the untrusted path; the merge verifies.
  static AddressBook decode(codec::ByteView bytes);

 private:
  std::map<std::uint32_t, PeerRecord> peers_;
};

}  // namespace blockchainer::p2p
