#include <vector>

#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls_key(std::uint8_t s) {
  Bytes ikm(32, s);
  return crypto::bls_keygen_from_ikm(view(ikm));
}
}  // namespace

int main() {
  init();

  auto k = bls_key(1);
  CHECK(k.public_key.size() == 48);
  CHECK(k.secret_key.size() == 32);

  Bytes msg{'b', 'l', 'o', 'c', 'k', '-', 'i', 'd'};
  Bytes sig = crypto::bls_sign(view(msg), view(k.secret_key));
  CHECK(sig.size() == 96);
  CHECK(crypto::bls_verify(view(msg), view(sig), view(k.public_key)));

  Bytes other{'x'};
  CHECK(!crypto::bls_verify(view(other), view(sig), view(k.public_key)));

  std::printf("== aggregate over per-signer distinct messages (BFT votes) ==\n");
  // BLS core AggregateVerify requires distinct messages (rogue-key safety).
  // Our votes bind the validator index, so each signer's message is distinct.
  auto a = bls_key(1);
  auto b = bls_key(2);
  auto c = bls_key(3);
  Bytes block_id{'B', 'L', 'O', 'C', 'K', '#', '7'};
  auto vote_msg = [&](std::uint8_t idx) {
    Bytes m = block_id;
    m.push_back(idx);  // bind validator index -> distinct per signer
    return m;
  };
  std::vector<Bytes> sigs = {
      crypto::bls_sign(view(vote_msg(0)), view(a.secret_key)),
      crypto::bls_sign(view(vote_msg(1)), view(b.secret_key)),
      crypto::bls_sign(view(vote_msg(2)), view(c.secret_key)),
  };
  Bytes agg = crypto::bls_aggregate(sigs);
  CHECK(agg.size() == 96);

  std::vector<Bytes> msgs = {vote_msg(0), vote_msg(1), vote_msg(2)};
  std::vector<Bytes> pks = {a.public_key, b.public_key, c.public_key};
  CHECK(crypto::bls_aggregate_verify(msgs, pks, view(agg)));

  // Missing one signer -> fails.
  std::vector<Bytes> pks2 = {a.public_key, b.public_key};
  std::vector<Bytes> msgs2 = {vote_msg(0), vote_msg(1)};
  CHECK(!crypto::bls_aggregate_verify(msgs2, pks2, view(agg)));

  // Tampered aggregate -> fails.
  Bytes bad_agg = agg;
  bad_agg[0] ^= 0x01;
  CHECK(!crypto::bls_aggregate_verify(msgs, pks, view(bad_agg)));

  // Wrong signer set (replace c with a fresh key) -> fails.
  auto d = bls_key(9);
  std::vector<Bytes> pks3 = {a.public_key, b.public_key, d.public_key};
  CHECK(!crypto::bls_aggregate_verify(msgs, pks3, view(agg)));

  return bctest::summary();
}
