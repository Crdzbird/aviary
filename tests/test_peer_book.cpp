// Peer discovery + address book: a canonical, mergeable directory of validators
// that a joining node fills in by exchanging books with peers (PEX). Tests the
// codec, first-writer-wins merge (an attacker can't rebind a known identity),
// convergence of PEX over a sparse topology, and that a converged book feeds the
// transport's SecureConfig.
#include <string>
#include <vector>

#include "blockchainer/peer_book.hpp"
#include "blockchainer/tcp_transport.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using p2p::AddressBook;
using p2p::PeerRecord;
using p2p::SecureConfig;
using p2p::SecureSession;

namespace {
PeerRecord rec(std::uint32_t id, Bytes kem, Bytes sig) {
  return PeerRecord{id, "127.0.0.1", static_cast<std::uint16_t>(47000 + id), std::move(kem), std::move(sig)};
}
}  // namespace

int main() {
  init();

  std::printf("== canonical codec: round-trips, re-encodes identically, rejects junk ==\n");
  {
    AddressBook b;
    b.add(rec(2, {1, 2, 3}, {4, 5}));
    b.add(rec(0, {9}, {8, 7}));
    b.add(rec(5, {}, {1}));
    const Bytes enc = b.encode();
    AddressBook d = AddressBook::decode(codec::ByteView(enc.data(), enc.size()));
    CHECK(d.size() == 3);
    CHECK_MSG(d.all() == b.all(), "decode is the exact inverse of encode");
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

  std::printf("== first-writer-wins merge: a known identity can't be rebound ==\n");
  {
    AddressBook mine;
    mine.add(rec(1, {0x01}, {0x01}));  // the true record for validator 1

    AddressBook hostile;
    hostile.add(rec(1, {0x99, 0x99}, {0x99, 0x99}));  // tries to rebind id 1 ...
    hostile.add(rec(2, {0x02}, {0x02}));              // ... and introduces id 2

    const std::size_t added = mine.merge(hostile);
    CHECK_MSG(added == 1, "only the genuinely-new record (id 2) is learned");
    CHECK_MSG(mine.get(1)->kem_pk == (Bytes{0x01}), "the known identity is NOT overwritten");
    CHECK(mine.contains(2));
  }

  std::printf("== PEX convergence: sparse seeds → every node learns the whole set ==\n");
  const std::uint32_t n = 6;
  std::vector<SecureSession::Identity> ids;
  std::vector<AddressBook> books(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    ids.push_back(SecureSession::generate_identity());
    books[i].add(rec(i, ids[i].kem.public_key, ids[i].sig.public_key));  // each starts knowing only itself
  }
  {
    // Exchange topology: a line 0-1-2-3-4-5. Knowledge must hop end to end.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;
    for (std::uint32_t i = 0; i + 1 < n; ++i) edges.push_back({i, i + 1});

    bool changed = true;
    int rounds = 0;
    for (; changed && rounds < 100; ++rounds) {
      changed = false;
      for (auto [a, b] : edges) {  // pull in both directions
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
