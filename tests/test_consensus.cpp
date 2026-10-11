#include <vector>

#include "aviary/consensus.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

crypto::KeyPair bls_key(std::uint8_t s) {
  Bytes ikm(32, s);
  return crypto::bls_keygen_from_ikm(view(ikm));
}

Hash block(std::uint8_t tag) {
  return crypto::hash_tagged(crypto::Domain::kLeaf, {tag});
}
}  // namespace

int main() {
  init();
  constexpr std::uint64_t kChain = 7;

  // 4 validators, equal power 1. total=4, quorum = floor(8/3)+1 = 3.
  std::vector<crypto::KeyPair> keys = {bls_key(1), bls_key(2), bls_key(3), bls_key(4)};
  std::vector<Validator> vs;
  for (auto& k : keys) vs.push_back(Validator{k.public_key, 1});
  ValidatorSet vset(vs);

  std::printf("== quorum math + intersection (safety foundation) ==\n");
  CHECK(vset.total_power() == 4);
  CHECK(vset.quorum() == 3);
  // Any two quorums intersect: 2*quorum > total.
  CHECK(2 * vset.quorum() > vset.total_power());

  const Hash A = block(0xAA);
  const Hash B = block(0xBB);

  std::printf("== happy path: 3-of-4 commit certificate ==\n");
  std::vector<Vote> votes_a;
  for (std::uint32_t i = 0; i < 4; ++i)
    votes_a.push_back(sign_vote(kChain, 1, 0, A, i, keys[i]));

  auto cert = aggregate_commit(kChain, vset, 1, 0, A, votes_a);
  CHECK(cert.has_value());
  CHECK(verify_commit(kChain, vset, *cert).ok);

  std::printf("== certificate round-trips ==\n");
  auto cert2 = CommitCertificate::decode_all(view(cert->encode()));
  CHECK(verify_commit(kChain, vset, cert2).ok);
  CHECK(cert2.encode() == cert->encode());

  std::printf("== below quorum: 2 votes form no certificate ==\n");
  auto none = aggregate_commit(kChain, vset, 1, 0, A, {votes_a[0], votes_a[1]});
  CHECK(!none.has_value());

  std::printf("== adversarial certificate forgery rejected ==\n");
  {
    // Tampered aggregate signature.
    CommitCertificate t = *cert;
    t.agg_signature[0] ^= 0x01;
    CHECK(!verify_commit(kChain, vset, t).ok);

    // Hand-built cert with only 2 signers (below quorum) but a real aggregate.
    CommitCertificate weak;
    weak.height = 1;
    weak.round = 0;
    weak.block_id = A;
    weak.signers = {0, 1};
    weak.agg_signature = crypto::bls_aggregate({votes_a[0].signature, votes_a[1].signature});
    CHECK(!verify_commit(kChain, vset, weak).ok);

    // Non-ascending signers rejected.
    CommitCertificate unsorted = *cert;
    std::swap(unsorted.signers[0], unsorted.signers[1]);
    CHECK(!verify_commit(kChain, vset, unsorted).ok);

    // Re-pointing the cert at a different block id breaks the signature.
    CommitCertificate wrong_block = *cert;
    wrong_block.block_id = B;
    CHECK(!verify_commit(kChain, vset, wrong_block).ok);
  }

  std::printf("== safety: f=1 byzantine cannot finalize a conflicting block ==\n");
  {
    // Honest validators 0,1,2 precommit A. Byzantine validator 3 double-signs
    // A and B. B receives only validator 3's vote -> power 1 < quorum 3.
    std::vector<Vote> votes_for_A = {
        sign_vote(kChain, 2, 0, A, 0, keys[0]),
        sign_vote(kChain, 2, 0, A, 1, keys[1]),
        sign_vote(kChain, 2, 0, A, 2, keys[2]),
        sign_vote(kChain, 2, 0, A, 3, keys[3]),  // byzantine also signs A
    };
    std::vector<Vote> votes_for_B = {
        sign_vote(kChain, 2, 0, B, 3, keys[3]),  // byzantine signs B too
    };
    auto certA = aggregate_commit(kChain, vset, 2, 0, A, votes_for_A);
    auto certB = aggregate_commit(kChain, vset, 2, 0, B, votes_for_B);
    CHECK(certA.has_value());            // A finalizes
    CHECK(verify_commit(kChain, vset, *certA).ok);
    CHECK(!certB.has_value());           // B cannot - no conflicting commit
  }

  std::printf("== weighted power: a >2/3 stakeholder commits alone ==\n");
  {
    std::vector<crypto::KeyPair> wk = {bls_key(1), bls_key(2), bls_key(3), bls_key(4)};
    std::vector<Validator> wv = {
        {wk[0].public_key, 1}, {wk[1].public_key, 1},
        {wk[2].public_key, 1}, {wk[3].public_key, 10}};
    ValidatorSet wset(wv);  // total 13, quorum = floor(26/3)+1 = 9
    CHECK(wset.quorum() == 9);
    auto solo = aggregate_commit(kChain, wset, 1, 0, A,
                                 {sign_vote(kChain, 1, 0, A, 3, wk[3])});
    CHECK(solo.has_value());  // validator 3's power 10 >= 9
    CHECK(verify_commit(kChain, wset, *solo).ok);
  }

  return bctest::summary();
}
