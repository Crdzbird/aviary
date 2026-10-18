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

  return bctest::summary();
}
