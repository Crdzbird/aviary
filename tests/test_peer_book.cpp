// Peer discovery + address book: a canonical, mergeable directory of validators
// that a joining node fills in by exchanging books with peers (PEX). Records are
// SELF-SIGNED (hybrid sig over the domain-tagged canonical bytes): merge - the
// PEX trust boundary - verifies every incoming record, so an attacker can't
// invent or alter a record for a key it doesn't hold; first-writer-wins protects
// already-known ids on top. Tests the codec, the signature, hostile merges,
// convergence over a sparse topology, and composition with the transport config.
#include <string>
#include <vector>

#include "aviary/peer_book.hpp"
#include "aviary/tcp_transport.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using p2p::AddressBook;
using p2p::PeerRecord;
using p2p::SecureConfig;
using p2p::SecureSession;

namespace {
PeerRecord make_record(std::uint32_t id, const SecureSession::Identity& owner) {
  PeerRecord r{id, "127.0.0.1", static_cast<std::uint16_t>(47000 + id),
               owner.kem.public_key, owner.sig.public_key, /*signature=*/{}};
  r.sign(owner.sig.secret_key);
  return r;
}
}  // namespace

int main() {
  init();

  // Long-term identities for six validators (used across all sections).
  const std::uint32_t n = 6;
  std::vector<SecureSession::Identity> ids;
  for (std::uint32_t i = 0; i < n; ++i) ids.push_back(SecureSession::generate_identity());

  std::printf("== canonical codec: round-trips, re-encodes identically, rejects junk ==\n");
  {
    AddressBook b;
    b.add(make_record(2, ids[2]));
    b.add(make_record(0, ids[0]));
    b.add(make_record(5, ids[5]));
    const Bytes enc = b.encode();
    AddressBook d = AddressBook::decode(codec::ByteView(enc.data(), enc.size()));
    CHECK(d.size() == 3);
    CHECK_MSG(d.all() == b.all(), "decode is the exact inverse of encode (incl. signatures)");
    CHECK_MSG(d.encode() == enc, "one book has exactly one encoding (canonical)");

    Bytes trailing = enc;
    trailing.push_back(0x00);
    bool threw = false;
    try {
      AddressBook::decode(codec::ByteView(trailing.data(), trailing.size()));
    } catch (const codec::CodecError&) {
      threw = true;
    }
    CHECK_MSG(threw, "trailing bytes are rejected (non-canonical)");
  }

  std::printf("== self-signature: verifies, and any field tamper breaks it ==\n");
  {
    PeerRecord r = make_record(1, ids[1]);
    CHECK_MSG(r.verify(), "an owner-signed record verifies");

    PeerRecord moved = r;
    moved.port = 9999;  // rebind the address...
    CHECK_MSG(!moved.verify(), "...and the signature no longer verifies");

    PeerRecord swapped = r;
    swapped.kem_pk = ids[2].kem.public_key;  // swap in a different KEM key
    CHECK(!swapped.verify());

    PeerRecord unsigned_rec = r;
    unsigned_rec.signature.clear();
    CHECK(!unsigned_rec.verify());

    // A signature by the WRONG key (attacker's) over an honest-looking record.
    PeerRecord forged = r;
    forged.sign(ids[3].sig.secret_key);  // attacker signs with its own key
    CHECK_MSG(!forged.verify(), "a signature by a key other than sig_pk is rejected");
  }

  std::printf("== hostile merge: forgeries rejected; known ids never overwritten ==\n");
  {
    AddressBook mine;
    mine.add(make_record(1, ids[1]));  // the true record for validator 1

    // A hostile book: a tampered copy of id 1, an unsigned id 2, a wrong-key id 3,
    // and one genuinely valid new record (id 4).
    AddressBook hostile;
    PeerRecord tampered = make_record(1, ids[1]);
    tampered.host = "6.6.6.6";  // signature now stale
    hostile.add(tampered);
    PeerRecord unsigned_rec{2, "127.0.0.1", 47002, ids[2].kem.public_key, ids[2].sig.public_key, {}};
    hostile.add(unsigned_rec);
    PeerRecord wrong_key = make_record(3, ids[3]);
    wrong_key.sign(ids[0].sig.secret_key);  // not the owner's key
    hostile.add(wrong_key);
    hostile.add(make_record(4, ids[4]));

    const std::size_t added = mine.merge(hostile);
    CHECK_MSG(added == 1, "only the genuinely valid new record survives the merge");
    CHECK(mine.contains(4) && !mine.contains(2) && !mine.contains(3));
    CHECK_MSG(mine.get(1)->host == "127.0.0.1", "the known identity is NOT overwritten");

    // Even a *validly signed* record can't rebind a known id (first-writer-wins):
    AddressBook rebind;
    PeerRecord moved = make_record(1, ids[1]);
    moved.port = 50000;
    moved.sign(ids[1].sig.secret_key);  // owner-valid, but we already know id 1
    rebind.add(moved);
    CHECK(mine.merge(rebind) == 0);
    CHECK_MSG(mine.get(1)->port == 47001, "a validly-signed newer record still can't rebind mid-session");
  }

  std::printf("== PEX convergence: sparse seeds -> every node learns the whole set ==\n");
  std::vector<AddressBook> books(n);
  for (std::uint32_t i = 0; i < n; ++i)
    books[i].add(make_record(i, ids[i]));  // each starts knowing only itself
  {
    // Exchange topology: a line 0-1-2-3-4-5. Knowledge must hop end to end.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;
    for (std::uint32_t i = 0; i + 1 < n; ++i) edges.push_back({i, i + 1});

    bool changed = true;
    int rounds = 0;
    for (; changed && rounds < 100; ++rounds) {
      changed = false;
      for (auto [a, b] : edges) {  // pull in both directions (verified merges)
        if (books[b].merge(books[a]) > 0) changed = true;
        if (books[a].merge(books[b]) > 0) changed = true;
      }
    }
    for (std::uint32_t i = 0; i < n; ++i) CHECK_MSG(books[i].size() == n, "every node discovered all peers");
    const Bytes ref = books[0].encode();
    for (std::uint32_t i = 1; i < n; ++i)
      CHECK_MSG(books[i].encode() == ref, "converged books are byte-identical across nodes");
    CHECK_MSG(rounds <= n, "converged within a linear number of rounds");
  }

  std::printf("== composition: a converged book yields the transport's SecureConfig ==\n");
  {
    const std::uint32_t self = 2;
    const AddressBook& book = books[self];  // fully converged from the PEX above
    std::vector<p2p::PeerAddr> peers;
    SecureConfig sc;
    sc.identity = ids[self];
    for (const auto& r : book.all()) {
      if (r.id == self) continue;
      peers.push_back(p2p::PeerAddr{r.id, r.host, r.port});
      sc.peer_kem_pk[r.id] = r.kem_pk;
      sc.peer_sig_pk[r.id] = r.sig_pk;
    }
    CHECK(peers.size() == n - 1);
    CHECK(sc.peer_kem_pk.size() == n - 1 && sc.peer_sig_pk.size() == n - 1);
    // The keys wired into the transport are exactly the discovered identities.
    CHECK(sc.peer_sig_pk[0] == ids[0].sig.public_key);
    CHECK(sc.peer_kem_pk[5] == ids[5].kem.public_key);
  }

  return bctest::summary();
}
