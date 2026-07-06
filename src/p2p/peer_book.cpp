#include "blockchainer/peer_book.hpp"

namespace blockchainer::p2p {

namespace {
void encode_record(codec::CodecWriter& w, const PeerRecord& r) {
  w.write_u32(r.id);
  w.write_bytes(codec::ByteView(reinterpret_cast<const std::uint8_t*>(r.host.data()), r.host.size()));
  w.write_u16(r.port);
  w.write_bytes(codec::ByteView(r.kem_pk.data(), r.kem_pk.size()));
  w.write_bytes(codec::ByteView(r.sig_pk.data(), r.sig_pk.size()));
}

PeerRecord decode_record(codec::CodecReader& rd) {
  PeerRecord r;
  r.id = rd.read_u32();
  Bytes host = rd.read_bytes();
  r.host.assign(host.begin(), host.end());
  r.port = rd.read_u16();
  r.kem_pk = rd.read_bytes();
  r.sig_pk = rd.read_bytes();
  return r;
}
}  // namespace

const PeerRecord* AddressBook::get(std::uint32_t id) const {
  auto it = peers_.find(id);
  return it == peers_.end() ? nullptr : &it->second;
}

std::vector<PeerRecord> AddressBook::all() const {
  std::vector<PeerRecord> out;
  out.reserve(peers_.size());
  for (const auto& [id, r] : peers_) out.push_back(r);  // std::map → ascending id
  return out;
}

std::size_t AddressBook::merge(const AddressBook& other) {
  std::size_t added = 0;
  for (const auto& [id, r] : other.peers_)
    if (peers_.emplace(id, r).second) ++added;  // first-writer-wins: emplace won't overwrite
  return added;
}

std::size_t AddressBook::merge(const std::vector<PeerRecord>& records) {
  std::size_t added = 0;
  for (const auto& r : records)
    if (peers_.emplace(r.id, r).second) ++added;
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

}  // namespace blockchainer::p2p
