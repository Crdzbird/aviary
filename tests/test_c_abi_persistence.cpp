#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "aviary_c.h"
#include "c_abi_support.hpp"
#include "check.hpp"

using cabi::Bytes;
using cabi::take;
using cabi::take_error;
using cabi::take_text;

namespace {
struct Keys {
  Bytes pub;
  Bytes sec;
};

Keys keys(std::uint8_t seed) {
  auto [p, s] = cabi::bls(seed);
  return Keys{p, s};
}

Bytes record_genesis(std::uint64_t chain_id, const Keys& k) {
  const std::uint64_t power = 1;
  const Bytes g = take(aviary_genesis_build(chain_id, 1, 1, 2, nullptr, nullptr, 0, k.pub.data(), &power, 1));
  const Bytes bp = take(aviary_blueprint_encode("bft", "record", "", nullptr, 0));
  return take(aviary_genesis_commit_blueprint(g.data(), g.size(), bp.data(), bp.size()));
}

AviaryChain assemble(const Bytes& genesis, const Keys& k) {
  return aviary_chain_assemble(genesis.data(), genesis.size(), 0, k.pub.data(), k.pub.size(), k.sec.data(),
                               k.sec.size());
}

AviaryChain restore(const Bytes& genesis, const Keys& k, const Bytes& snapshot) {
  return aviary_chain_restore(genesis.data(), genesis.size(), 0, k.pub.data(), k.pub.size(), k.sec.data(),
                              k.sec.size(), snapshot.data(), snapshot.size());
}

Bytes ed_secret(std::uint8_t seed) {
  AviaryNode n = aviary_node_new(900 + seed);
  Bytes s = take(aviary_node_new_account(n));
  aviary_node_free(n);
  return s;
}

Bytes step(AviaryChain c, std::uint64_t ts, const Bytes& tx) {
  if (!tx.empty()) (void)take(aviary_chain_add_tx(c, tx.data(), tx.size()));
  const Bytes blk = take(aviary_chain_propose(c, ts));
  const Bytes cert = take(aviary_chain_certify(c, blk.data(), blk.size()));
  CHECK(aviary_chain_submit(c, blk.data(), blk.size(), cert.data(), cert.size()) == 1);
  return blk;
}

std::string snapshot_error(const Bytes& genesis, const Bytes& snapshot) {
  return take_text(aviary_chain_snapshot_error(genesis.data(), genesis.size(), snapshot.data(), snapshot.size()));
}
}  // namespace

int main() {
  CHECK(aviary_init() == 0);

  std::printf("== chain restart through the C ABI ==\n");
  {
    const Keys k = keys(0x71);
    const Bytes genesis = record_genesis(61, k);
    const Bytes secret = ed_secret(1);
    std::vector<Bytes> txs;
    for (std::uint64_t i = 0; i < 6; ++i) {
      Bytes key(32, static_cast<std::uint8_t>(0x10 + i));
      const std::string value = "v" + std::to_string(i);
      txs.push_back(take(aviary_record_tx_sign(61, i, key.data(), 0,
                                               reinterpret_cast<const std::uint8_t*>(value.data()), value.size(),
                                               secret.data(), secret.size())));
    }

    AviaryChain straight = assemble(genesis, k);
    CHECK(straight != nullptr);
    CHECK(aviary_chain_archive_enable(straight) == 1);
    std::vector<Bytes> expected;
    for (std::uint64_t i = 0; i < 6; ++i) expected.push_back(step(straight, 10 + i, txs[i]));

    AviaryChain first = assemble(genesis, k);
    CHECK(aviary_chain_archive_enable(first) == 1 && aviary_chain_archive_enable(first) == 1);
    std::vector<Bytes> got;
    for (std::uint64_t i = 0; i < 3; ++i) got.push_back(step(first, 10 + i, txs[i]));
    const Bytes snap = take(aviary_chain_snapshot(first));
    CHECK(!snap.empty());
    aviary_chain_free(first);

    CHECK(snapshot_error(genesis, snap).empty());
    AviaryChain second = restore(genesis, k, snap);
    CHECK(second != nullptr);
    CHECK(aviary_chain_height(second) == 3);
    CHECK(take(aviary_chain_snapshot(second)) == snap);
    for (std::uint64_t i = 3; i < 6; ++i) got.push_back(step(second, 10 + i, txs[i]));
    CHECK(got == expected);
    CHECK(take(aviary_chain_tip(second)) == take(aviary_chain_tip(straight)));
    CHECK(take(aviary_chain_state_root(second)) == take(aviary_chain_state_root(straight)));
    CHECK(take(aviary_chain_snapshot(second)) == take(aviary_chain_snapshot(straight)));
    Bytes probe(32, 0x14);
    CHECK(take(aviary_chain_record(second, probe.data())) == take(aviary_chain_record(straight, probe.data())));

    std::uint64_t lo = 0, hi = 0;
    CHECK(aviary_chain_archive_range(second, &lo, &hi) == 1 && lo == 1 && hi == 6);
    CHECK(take(aviary_chain_archived_block(second, 2)) == expected[1]);
    const Bytes cert4 = take(aviary_chain_archived_certificate(second, 4));
    const Bytes header4 = take(aviary_block_header(expected[3].data(), expected[3].size()));
    CHECK(!take(aviary_certified_header_encode(header4.data(), header4.size(), cert4.data(), cert4.size())).empty());
    CHECK(!take_error(aviary_chain_archived_block(second, 9)).empty());
    CHECK(aviary_chain_archive_prune(second, 5) == 4);
    CHECK(aviary_chain_archive_range(second, &lo, &hi) == 1 && lo == 5 && hi == 6);
    CHECK(!take_error(aviary_chain_archived_block(second, 2)).empty());
    const Bytes pruned = take(aviary_chain_snapshot(second));
    AviaryChain third = restore(genesis, k, pruned);
    CHECK(third != nullptr && take(aviary_chain_snapshot(third)) == pruned);
    aviary_chain_free(third);

    std::printf("== rejections name their reason and build nothing ==\n");
    Bytes trailing = snap;
    trailing.push_back(0);
    CHECK(restore(genesis, k, trailing) == nullptr && !snapshot_error(genesis, trailing).empty());
    Bytes cut(snap.begin(), snap.end() - 7);
    CHECK(restore(genesis, k, cut) == nullptr && !snapshot_error(genesis, cut).empty());
    Bytes magic = snap;
    magic[1] ^= 0x20;
    CHECK(restore(genesis, k, magic) == nullptr && snapshot_error(genesis, magic).find("magic") != std::string::npos);
    Bytes version = snap;
    version[4] = 9;
    CHECK(restore(genesis, k, version) == nullptr &&
          snapshot_error(genesis, version).find("version") != std::string::npos);
    const Bytes other_genesis = record_genesis(62, k);
    CHECK(restore(other_genesis, k, snap) == nullptr &&
          snapshot_error(other_genesis, snap).find("another genesis") != std::string::npos);
    Bytes flipped = snap;
    flipped[snap.size() / 3] ^= 0x01;
    CHECK(restore(genesis, k, flipped) == nullptr && !snapshot_error(genesis, flipped).empty());
    CHECK(aviary_chain_restore(nullptr, 0, 0, k.pub.data(), k.pub.size(), k.sec.data(), k.sec.size(), snap.data(),
                               snap.size()) == nullptr);
    CHECK(!snapshot_error(Bytes{1, 2, 3}, snap).empty());

    std::printf("== a flipped byte in any archived certificate is refused ==\n");
    AviaryChain source = restore(genesis, k, snap);
    int certificate_bytes = 0, accepted = 0;
    for (std::uint64_t h = 1; h <= 3; ++h) {
      const Bytes block = take(aviary_chain_archived_block(source, h));
      const Bytes cert = take(aviary_chain_archived_certificate(source, h));
      const auto stored = std::search(snap.begin(), snap.end(), block.begin(), block.end());
      CHECK(!block.empty() && !cert.empty() && stored != snap.end());
      const auto after = stored + static_cast<std::ptrdiff_t>(block.size());
      const auto at = std::search(after, snap.end(), cert.begin(), cert.end());
      CHECK(at - stored < static_cast<std::ptrdiff_t>(block.size() + 8));
      const std::size_t offset = static_cast<std::size_t>(at - snap.begin());
      for (std::size_t i = 0; i < cert.size(); ++i) {
        Bytes forged = snap;
        forged[offset + i] ^= 0x01;
        ++certificate_bytes;
        AviaryChain back = restore(genesis, k, forged);
        if (back != nullptr || snapshot_error(genesis, forged).empty()) ++accepted;
        aviary_chain_free(back);
      }
    }
    std::printf("  %d certificate bytes flipped, %d accepted\n", certificate_bytes, accepted);
    CHECK(certificate_bytes > 300 && accepted == 0);
    aviary_chain_free(source);

    aviary_chain_free(second);
    aviary_chain_free(straight);
  }

  std::printf("== a chain that never enables the archive keeps no blocks ==\n");
  {
    const Keys k = keys(0x72);
    const Bytes genesis = record_genesis(63, k);
    AviaryChain c = assemble(genesis, k);
    step(c, 10, {});
    std::uint64_t lo = 0, hi = 0;
    CHECK(aviary_chain_archive_range(c, &lo, &hi) == 0);
    CHECK(take_error(aviary_chain_archived_block(c, 1)).find("not enabled") != std::string::npos);
    CHECK(aviary_chain_archive_prune(c, 10) == 0);
    const Bytes snap = take(aviary_chain_snapshot(c));
    AviaryChain back = restore(genesis, k, snap);
    CHECK(back != nullptr && aviary_chain_archive_range(back, &lo, &hi) == 0);
    CHECK(step(back, 11, {}) == step(c, 11, {}));
    CHECK(aviary_chain_archive_enable(back) == 1);
    step(back, 12, {});
    CHECK(aviary_chain_archive_range(back, &lo, &hi) == 1 && lo == 3 && hi == 3);
    aviary_chain_free(back);
    aviary_chain_free(c);
    CHECK(take_error(aviary_chain_snapshot(nullptr)).find("null") != std::string::npos);
    CHECK(aviary_chain_archive_enable(nullptr) == 0);
  }

  std::printf("== chain tree restart through the C ABI ==\n");
  {
    const Keys root = keys(0x01), child = keys(0x10);
    const Bytes pos = take(aviary_blueprint_encode("pos", "account", "round-robin", nullptr, 0));
    const Bytes bft = take(aviary_blueprint_encode("bft", "account", "", nullptr, 0));
    const std::uint64_t power = 1;
    const Bytes leaf = take(aviary_chain_tree_node_encode(42, bft.data(), bft.size(), 1, nullptr, nullptr, 0,
                                                          child.pub.data(), child.sec.data(), &power, 1, 0, 0,
                                                          nullptr, 0));
    const Bytes children = cabi::list({leaf});
    const Bytes spec = take(aviary_chain_tree_node_encode(1, pos.data(), pos.size(), 1, nullptr, nullptr, 0,
                                                          root.pub.data(), root.sec.data(), &power, 1, 0, 0,
                                                          children.data(), children.size()));
    AviaryChainTree straight = aviary_chain_tree_build(spec.data(), spec.size());
    AviaryChainTree first = aviary_chain_tree_build(spec.data(), spec.size());
    CHECK(straight != nullptr && first != nullptr);
    auto round = [](AviaryChainTree t, std::uint64_t ts) {
      Bytes a = take(aviary_chain_tree_produce(t, 42, ts));
      Bytes b = take(aviary_chain_tree_produce(t, 1, ts));
      a.insert(a.end(), b.begin(), b.end());
      return a;
    };
    CHECK(round(straight, 10) == round(first, 10));
    const Bytes snap = take(aviary_chain_tree_snapshot(first));
    aviary_chain_tree_free(first);
    AviaryChainTree second = aviary_chain_tree_restore(spec.data(), spec.size(), snap.data(), snap.size());
    CHECK(second != nullptr);
    CHECK(take(aviary_chain_tree_snapshot(second)) == snap);
    CHECK(round(straight, 11) == round(second, 11));
    CHECK(take(aviary_chain_tree_tip(second, 1)) == take(aviary_chain_tree_tip(straight, 1)));
    CHECK(take(aviary_chain_tree_snapshot(second)) == take(aviary_chain_tree_snapshot(straight)));
    Bytes bad = snap;
    bad.push_back(1);
    CHECK(aviary_chain_tree_restore(spec.data(), spec.size(), bad.data(), bad.size()) == nullptr);
    CHECK(aviary_chain_tree_restore(leaf.data(), leaf.size(), snap.data(), snap.size()) == nullptr);
    aviary_chain_tree_free(second);
    aviary_chain_tree_free(straight);
  }

  return bctest::summary();
}
