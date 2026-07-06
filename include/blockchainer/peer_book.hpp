#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/types.hpp"

/// Peer discovery + address book (ARCHITECTURE §9). The transport needs to know,
/// for each validator, where to reach it (host:port) and which keys authenticate
/// it (the KEM key to encapsulate to, the signing key to pin). Rather than every
/// node hard-coding the full set, a joining node starts from a seed and *learns*
/// the rest by exchanging address books (a pull-based peer-exchange / PEX).
///
/// The book is a canonical, mergeable set of [PeerRecord]s. `merge` is
/// **first-writer-wins**: once a node knows a peer's record it will not let a
/// later gossip overwrite it, so an attacker can't rebind a known identity to a
/// new address/key via PEX — it can only introduce genuinely-new ids. Serialize
/// with the same deterministic codec as everything else, so two nodes that have
/// learned the same peers hold byte-identical books.
namespace blockchainer::p2p {

/// Everything needed to dial + authenticate one validator.
struct PeerRecord {
  std::uint32_t id = 0;
  std::string host;
  std::uint16_t port = 0;
  Bytes kem_pk;  ///< hybrid KEM public key (to open a secure session to it)
  Bytes sig_pk;  ///< hybrid signing public key (identity / allow-list)

  bool operator==(const PeerRecord&) const = default;
};

class AddressBook {
 public:
  AddressBook() = default;

  void add(const PeerRecord& r) { peers_[r.id] = r; }
  bool contains(std::uint32_t id) const { return peers_.count(id) != 0; }
  /// The record for [id], or nullptr if unknown.
  const PeerRecord* get(std::uint32_t id) const;
  std::size_t size() const { return peers_.size(); }
  /// All records in canonical (ascending-id) order.
  std::vector<PeerRecord> all() const;

  /// Learn the records in [other] we don't already have. First-writer-wins: an
  /// id we already know is left untouched. Returns how many were newly added.
  std::size_t merge(const AddressBook& other);
  std::size_t merge(const std::vector<PeerRecord>& records);

  /// Canonical serialization (records in ascending-id order).
  Bytes encode() const;
  /// Strict inverse; throws codec::CodecError on malformed / non-canonical input
  /// (out-of-order or duplicate ids, trailing bytes).
  static AddressBook decode(codec::ByteView bytes);

 private:
  std::map<std::uint32_t, PeerRecord> peers_;
};

}  // namespace blockchainer::p2p
