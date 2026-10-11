#include <map>
#include "aviary/peer_book.hpp"

namespace aviary::p2p {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// The unsigned fields, canonically encoded - the byte string the owner signs
// (via the domain-tagged hash) and the prefix of the wire form.
void encode_unsigned(codec::CodecWriter& w, const PeerRecord& r) {
  w.write_u32(r.id);
  w.write_bytes(codec::ByteView(reinterpret_cast<const std::uint8_t*>(r.host.data()), r.host.size()));
  w.write_u16(r.port);
  w.write_bytes(view(r.kem_pk));
  w.write_bytes(view(r.sig_pk));
  w.write_bytes(view(r.prekey));  // ratchet prekey (may be empty) - signed like the rest
}

void encode_record(codec::CodecWriter& w, const PeerRecord& r) {
  encode_unsigned(w, r);
  w.write_bytes(view(r.signature));
}

PeerRecord decode_record(codec::CodecReader& rd) {
  PeerRecord r;
  r.id = rd.read_u32();
  Bytes host = rd.read_bytes();
  r.host.assign(host.begin(), host.end());
  r.port = rd.read_u16();
  r.kem_pk = rd.read_bytes();
  r.sig_pk = rd.read_bytes();
  r.prekey = rd.read_bytes();
  r.signature = rd.read_bytes();
  return r;
}
}  // namespace

Hash PeerRecord::signing_hash() const {
  codec::CodecWriter w;
  encode_unsigned(w, *this);
  return crypto::hash_tagged(crypto::Domain::kPeerRecord, w.bytes());
}

void PeerRecord::sign(const Bytes& sig_secret_key) {
  const Hash h = signing_hash();
  signature = crypto::hybrid_sig_sign(codec::ByteView(h.data(), h.size()), view(sig_secret_key));
}

bool PeerRecord::verify() const {
  if (signature.empty() || sig_pk.empty()) return false;
  const Hash h = signing_hash();
  return crypto::hybrid_sig_verify(codec::ByteView(h.data(), h.size()), view(signature), view(sig_pk));
}

const PeerRecord* AddressBook::get(std::uint32_t id) const {
  auto it = peers_.find(id);
  return it == peers_.end() ? nullptr : &it->second;
}

std::vector<PeerRecord> AddressBook::all() const {
  std::vector<PeerRecord> out;
  out.reserve(peers_.size());
  for (const auto& [id, r] : peers_) out.push_back(r);  // std::map -> ascending id
  return out;
}

std::size_t AddressBook::merge(const AddressBook& other) {
  std::size_t added = 0;
  for (const auto& [id, r] : other.peers_) {
    if (peers_.count(id)) continue;   // first-writer-wins: never overwrite
    if (!r.verify()) continue;        // PEX boundary: only owner-signed records
    peers_.emplace(id, r);
    ++added;
  }
  return added;
}

std::size_t AddressBook::merge(const std::vector<PeerRecord>& records) {
  std::size_t added = 0;
  for (const auto& r : records) {
    if (peers_.count(r.id)) continue;
    if (!r.verify()) continue;
    peers_.emplace(r.id, r);
    ++added;
  }
  return added;
}

Bytes AddressBook::encode() const {
  codec::CodecWriter w;
  w.write_list(all(), encode_record);
  return w.take();
}

AddressBook AddressBook::decode(codec::ByteView bytes) {
  codec::CodecReader rd(bytes);
  const std::vector<PeerRecord> records = rd.read_list<PeerRecord>(decode_record);
  rd.assert_fully_consumed();

  AddressBook book;
  bool have_prev = false;
  std::uint32_t prev = 0;
  for (const auto& r : records) {
    // Canonical ordering: ids strictly ascending (rejects duplicates + reorder).
    if (have_prev && r.id <= prev) throw codec::CodecError("peer records not in ascending id order");
    have_prev = true;
    prev = r.id;
    book.add(r);
  }
  return book;
}

}  // namespace aviary::p2p
