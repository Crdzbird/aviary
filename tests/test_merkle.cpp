#include <cstdint>
#include <vector>

#include "aviary/crypto.hpp"
#include "aviary/merkle.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using aviary::crypto::Hash;
using namespace aviary::merkle;

namespace {
Hash leaf(std::uint8_t i) {
  return aviary::crypto::hash_tagged(aviary::crypto::Domain::kLeaf,
                                           {i});
}
std::vector<Hash> leaves(std::uint32_t n) {
  std::vector<Hash> v;
  for (std::uint32_t i = 0; i < n; ++i) v.push_back(leaf(static_cast<std::uint8_t>(i)));
  return v;
}
}  // namespace

int main() {
  aviary::init();  // crypto backend

  std::printf("== structural ==\n");
  // Single leaf is its own root.
  CHECK(compute_root(leaves(1)) == leaf(0));
  // Two leaves -> node hash.
  CHECK(compute_root(leaves(2)) ==
        aviary::crypto::hash_node(leaf(0), leaf(1)));
  // Empty root is stable and equals the domain-separated empty constant.
  CHECK(compute_root({}) ==
        aviary::crypto::hash_tagged(aviary::crypto::Domain::kNode, {}));
  // Determinism.
  CHECK(compute_root(leaves(5)) == compute_root(leaves(5)));

  // Carry-up: 3 leaves. Root = node( node(l0,l1), l2 ).
  {
    auto ls = leaves(3);
    Hash expect = aviary::crypto::hash_node(
        aviary::crypto::hash_node(ls[0], ls[1]), ls[2]);
    CHECK(compute_root(ls) == expect);
  }

  std::printf("== proofs round-trip (every index, sizes 1..9) ==\n");
  bool all_ok = true;
  for (std::uint32_t n = 1; n <= 9; ++n) {
    auto ls = leaves(n);
    Hash root = compute_root(ls);
    for (std::uint32_t i = 0; i < n; ++i) {
      Proof p = make_proof(ls, i);
      if (!verify_proof(root, ls[i], p, n)) all_ok = false;
    }
  }
  CHECK_MSG(all_ok, "all inclusion proofs verify for sizes 1..9");

  std::printf("== negative cases ==\n");
  {
    auto ls = leaves(6);
    Hash root = compute_root(ls);
    Proof p = make_proof(ls, 2);
    // Wrong leaf rejected.
    CHECK(!verify_proof(root, leaf(99), p, 6));
    // Wrong root rejected.
    CHECK(!verify_proof(leaf(0), ls[2], p, 6));
    // Tampered sibling rejected.
    Proof tampered = p;
    if (!tampered.siblings.empty()) tampered.siblings[0] = leaf(123);
    CHECK(!verify_proof(root, ls[2], tampered, 6));
    // A leaf_count that changes the path shape is rejected (sibling count no
    // longer matches). NOTE: a count that preserves the path can still verify - 
    // the binding is the sibling set, not leaf_count alone.
    CHECK(!verify_proof(root, ls[2], p, 3));
    // Too-few siblings rejected.
    Proof short_p = p;
    if (!short_p.siblings.empty()) short_p.siblings.pop_back();
    CHECK(!verify_proof(root, ls[2], short_p, 6));
    // Too-many siblings rejected.
    Proof long_p = p;
    long_p.siblings.push_back(leaf(7));
    CHECK(!verify_proof(root, ls[2], long_p, 6));
  }
  // Out-of-range proof request throws.
  CHECK_THROWS(make_proof(leaves(4), 4));

  for (std::uint32_t n = 1; n <= 40; ++n) {
    auto ls = leaves(n);
    MerkleIndex index;
    index.reset(ls);
    for (std::uint32_t i = 0; i < n; ++i) {
      const Proof want = make_proof(ls, i);
      const Proof got = index.proof(i);
      CHECK(got.index == want.index);
      CHECK(got.siblings == want.siblings);
    }
    CHECK_THROWS(index.proof(n));
  }
  CHECK_THROWS(MerkleIndex{}.proof(0));

  std::printf("== root_from_proof agrees with verify_proof ==\n");
  {
    bool agree = true;
    for (std::uint32_t n = 1; n <= 33; ++n) {
      auto ls = leaves(n);
      const Hash root = compute_root(ls);
      for (std::uint32_t i = 0; i < n; ++i) {
        const Proof pr = make_proof(ls, i);
        const std::optional<Hash> got = root_from_proof(ls[i], pr, n);
        agree &= got.has_value() && *got == root;
        Proof longer = pr;
        longer.siblings.push_back(leaf(200));
        agree &= !root_from_proof(ls[i], longer, n).has_value();
        if (pr.siblings.empty()) continue;
        Proof shorter = pr;
        shorter.siblings.pop_back();
        agree &= !root_from_proof(ls[i], shorter, n).has_value();
      }
      agree &= !root_from_proof(ls[0], make_proof(ls, 0), 0).has_value();
      Proof out_of_range = make_proof(ls, 0);
      out_of_range.index = n;
      agree &= !root_from_proof(ls[0], out_of_range, n).has_value();
    }
    CHECK_MSG(agree, "root_from_proof returns the root exactly when the proof has the shape the tree needs");
  }

  std::printf("== AppendTree equals compute_root and make_proof under appends and rewrites ==\n");
  {
    AppendTree empty;
    CHECK(empty.leaf_count() == 0 && empty.root() == compute_root({}));
    CHECK_THROWS(empty.leaf(0));
    CHECK_THROWS(empty.proof_with({}, 0));
    AppendTree::Batch bad;
    bad.changes[0] = leaf(1);
    CHECK_THROWS(empty.root_with(bad));

    std::uint64_t state = 0x9E3779B97F4A7C15ull;
    auto rnd = [&](std::uint64_t n) {
      state ^= state << 13;
      state ^= state >> 7;
      state ^= state << 17;
      return n == 0 ? 0 : state % n;
    };
    bool roots = true, proofs = true, pure = true;
    for (std::uint32_t start = 0; start <= 20; ++start) {
      std::vector<Hash> model = leaves(start);
      AppendTree tree;
      tree.reset(model);
      roots &= tree.root() == compute_root(model);
      std::uint8_t next_tag = 100;
      for (int step = 0; step < 30; ++step) {
        AppendTree::Batch batch;
        std::vector<Hash> after = model;
        const std::uint64_t rewrites = model.empty() ? 0 : rnd(4);
        for (std::uint64_t k = 0; k < rewrites; ++k) {
          const std::size_t at = static_cast<std::size_t>(rnd(model.size()));
          batch.changes[at] = leaf(next_tag);
          after[at] = leaf(next_tag++);
        }
        const std::uint64_t appends = rnd(5);
        for (std::uint64_t k = 0; k < appends; ++k) {
          batch.appended.push_back(leaf(next_tag));
          after.push_back(leaf(next_tag++));
        }
        const Hash before = tree.root();
        roots &= tree.root_with(batch) == compute_root(after);
        pure &= tree.root() == before && tree.leaf_count() == model.size();
        for (std::uint32_t i = 0; i < after.size(); ++i) {
          const Proof want = make_proof(after, i);
          const Proof got = tree.proof_with(batch, i);
          proofs &= got.index == want.index && got.siblings == want.siblings;
        }
        if (!after.empty()) {
          bool threw = false;
          try {
            (void)tree.proof_with(batch, static_cast<std::uint32_t>(after.size()));
          } catch (const std::out_of_range&) {
            threw = true;
          }
          proofs &= threw;
        }
        tree.apply(batch);
        model = after;
        roots &= tree.root() == compute_root(model) && tree.leaf_count() == model.size();
        for (std::size_t i = 0; i < model.size(); ++i) roots &= tree.leaf(i) == model[i];
      }
    }
    CHECK_MSG(roots, "AppendTree roots match compute_root before and after every batch");
    CHECK_MSG(proofs, "AppendTree proofs match make_proof for every leaf of the batched tree");
    CHECK_MSG(pure, "root_with and proof_with leave the tree untouched");
  }

  return bctest::summary();
}
