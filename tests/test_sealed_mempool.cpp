// SealedMempool: transactions stay sealed to the block proposer until it opens
// them at build time — a front-runner watching the pool sees only ciphertext and
// a fee hint, never the contents. Ordering by fee is preserved; a lying fee hint
// or a tampered envelope is dropped on reveal.
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/sealed_mempool.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using p2p::SealedMempool;
using p2p::SealedTx;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }

// A v2 (fee-carrying) signed transfer.
Transaction transfer(const crypto::KeyPair& kp, std::uint64_t nonce, codec::u128 fee) {
  Transaction tx;
  tx.version = 2;
  tx.chain_id = 7;
  tx.fee = fee;
  Hash to{};
  to[0] = 0xCC;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, 10});
  sign_transaction(tx, kp);
  return tx;
}
}  // namespace

int main() {
  init();
  const crypto::KeyPair alice = ed(1);
  const crypto::KeyPair proposer = crypto::hybrid_kem_keygen();  // the block proposer's KEM key

  std::printf("== the pool holds only ciphertext (no plaintext leaks) ==\n");
  {
    Transaction tx = transfer(alice, 0, 100);
    SealedTx sealed = SealedTx::seal(tx, view(proposer.public_key));
    CHECK_MSG(sealed.envelope != tx.encode(), "the sealed envelope is not the plaintext tx");
    // The fee hint is public (needed for ordering); the body is not recoverable
    // from the envelope without the proposer's secret (exercised below).
    CHECK(sealed.fee_hint == 100);
  }

  std::printf("== fee-priority ordering + proposer reveal returns the real txs ==\n");
  {
    SealedMempool pool;
    // Submit out of fee order; the pool must order highest-fee first.
    CHECK(pool.add(SealedTx::seal(transfer(alice, 0, 50), view(proposer.public_key))));
    CHECK(pool.add(SealedTx::seal(transfer(alice, 1, 300), view(proposer.public_key))));
    CHECK(pool.add(SealedTx::seal(transfer(alice, 2, 120), view(proposer.public_key))));
    CHECK(pool.size() == 3);

    std::vector<Transaction> opened = pool.reveal(view(proposer.secret_key), 10);
    CHECK(opened.size() == 3);
    CHECK_MSG(opened[0].declared_fee() == 300, "highest fee revealed first");
    CHECK(opened[1].declared_fee() == 120);
    CHECK(opened[2].declared_fee() == 50);
    for (const auto& tx : opened) CHECK(verify_transaction_signature(tx));

    // Taking the top 2 respects the cap.
    CHECK(pool.reveal(view(proposer.secret_key), 2).size() == 2);
  }

  std::printf("== only the proposer can open: a stranger reveals nothing ==\n");
  {
    SealedMempool pool;
    pool.add(SealedTx::seal(transfer(alice, 0, 100), view(proposer.public_key)));
    const crypto::KeyPair stranger = crypto::hybrid_kem_keygen();
    CHECK_MSG(pool.reveal(view(stranger.secret_key), 10).empty(),
              "a non-proposer key opens nothing — the pool is confidential to it");
  }

  std::printf("== a lying fee hint is dropped on reveal (can't buy priority) ==\n");
  {
    // Seal a low-fee tx but advertise a huge hint. To even decrypt, the envelope
    // must have been sealed under that inflated hint (the AAD) — and then the
    // real fee (5) won't match the hint (100000), so reveal drops it.
    Transaction cheap = transfer(alice, 0, 5);
    codec::CodecWriter w;
    w.write_u128(100000);
    const Bytes aad = w.take();  // the inflated hint, used as the seal AAD
    SealedTx liar;
    liar.fee_hint = 100000;
    liar.envelope = crypto::pq_seal_to(view(cheap.encode()), view(proposer.public_key), view(aad));

    SealedMempool pool;
    pool.add(liar);
    std::vector<Transaction> opened = pool.reveal(view(proposer.secret_key), 10);
    CHECK_MSG(opened.empty(), "a submission whose real fee != its hint is discarded");
  }

  std::printf("== a tampered envelope fails to open and is skipped ==\n");
  {
    SealedTx s = SealedTx::seal(transfer(alice, 0, 100), view(proposer.public_key));
    s.envelope[s.envelope.size() / 2] ^= 0x01;
    SealedMempool pool;
    pool.add(s);
    CHECK(pool.reveal(view(proposer.secret_key), 10).empty());
  }

  std::printf("== SealedTx codec round-trips; remove drops by envelope ==\n");
  {
    SealedTx s = SealedTx::seal(transfer(alice, 0, 77), view(proposer.public_key));
    SealedTx back = SealedTx::decode(view(s.encode()));
    CHECK(back.fee_hint == s.fee_hint && back.envelope == s.envelope);

    SealedMempool pool;
    pool.add(s);
    pool.add(SealedTx::seal(transfer(alice, 1, 88), view(proposer.public_key)));
    pool.remove({s.envelope});
    CHECK(pool.size() == 1);
    CHECK(pool.reveal(view(proposer.secret_key), 10)[0].declared_fee() == 88);
  }

  return bctest::summary();
}
