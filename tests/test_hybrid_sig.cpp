// Q1: hybrid post-quantum transaction signatures (Ed25519 + ML-DSA-65).
// Verifies the security properties: both parts must pass, the scheme is
// committed to (no downgrade), unknown schemes are rejected, and chain params
// can require PQ. Version-1 (legacy Ed25519) transactions are unaffected.
#include "blockchainer/account_state.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// Build a signed transfer under a chosen scheme.
Transaction make_transfer(const crypto::KeyPair& kp, SigAlgo algo, std::uint64_t nonce) {
  Transaction tx;
  tx.version = (algo == SigAlgo::kEd25519) ? 1 : 2;
  tx.sig_algo = static_cast<std::uint8_t>(algo);
  tx.chain_id = 7;
  Hash to{};
  to[0] = 0xBB;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, 100});
  sign_transaction(tx, kp);
  return tx;
}
}  // namespace

int main() {
  init();

  std::printf("== hybrid keygen shapes ==\n");
  crypto::KeyPair hk = crypto::hybrid_sig_keygen();
  // ed25519 pub(32) + ML-DSA-65 pub(1952); ed25519 sec includes seed+pub.
  CHECK(hk.public_key.size() > 1900);
  CHECK(hk.secret_key.size() > 1900);

  std::printf("== hybrid tx round-trips (sign -> verify) ==\n");
  Transaction htx = make_transfer(hk, SigAlgo::kHybridEd25519MlDsa, 0);
  CHECK(htx.version == 2);
  CHECK(htx.algo() == SigAlgo::kHybridEd25519MlDsa);
  CHECK(htx.signature.size() > 3000);  // ed_sig(64) + ML-DSA-65 sig(~3309)
  CHECK(verify_transaction_signature(htx));

  std::printf("== encode/decode preserves the extended envelope ==\n");
  Transaction rt = Transaction::decode_all(view(htx.encode()));
  CHECK(rt.version == 2 && rt.sig_algo == htx.sig_algo);
  CHECK(rt.txid() == htx.txid());
  CHECK(verify_transaction_signature(rt));

  std::printf("== anti-downgrade: stripping the ML-DSA part fails ==\n");
  {
    Transaction down = htx;
    down.signature.resize(64);  // keep only the Ed25519 signature
    CHECK(!verify_transaction_signature(down));
    // Also claiming it's plain Ed25519 must fail: the hybrid public key is not a
    // 32-byte Ed25519 key, and signing_bytes committed to sig_algo=2.
    down.version = 1;
    CHECK(!verify_transaction_signature(down));
  }

  std::printf("== algo is committed: flipping sig_algo invalidates ==\n");
  {
    Transaction tampered = htx;
    tampered.sig_algo = static_cast<std::uint8_t>(SigAlgo::kEd25519);
    CHECK(!verify_transaction_signature(tampered));
  }

  std::printf("== unknown scheme is rejected ==\n");
  {
    Transaction weird = htx;
    weird.sig_algo = 99;
    CHECK(!verify_transaction_signature(weird));
  }

  std::printf("== wrong key fails ==\n");
  {
    crypto::KeyPair other = crypto::hybrid_sig_keygen();
    Transaction t = htx;
    t.author = other.public_key;
    CHECK(!verify_transaction_signature(t));
  }

  std::printf("== legacy Ed25519 (v1) still verifies ==\n");
  {
    crypto::KeyPair ek = crypto::ed25519_keygen();
    Transaction etx = make_transfer(ek, SigAlgo::kEd25519, 0);
    CHECK(etx.version == 1);
    CHECK(etx.signature.size() == 64);
    CHECK(verify_transaction_signature(etx));
  }

  std::printf("== chain params gate the accepted scheme ==\n");
  {
    ChainSpec pq_only{7, StateModel::kAccount, {static_cast<std::uint8_t>(SigAlgo::kHybridEd25519MlDsa)}};
    CHECK(pq_only.accepts(SigAlgo::kHybridEd25519MlDsa));
    CHECK(!pq_only.accepts(SigAlgo::kEd25519));

    ChainSpec classic{7, StateModel::kAccount, {static_cast<std::uint8_t>(SigAlgo::kEd25519)}};
    CHECK(!classic.accepts(SigAlgo::kHybridEd25519MlDsa));

    ChainSpec any{7, StateModel::kAccount, {}};
    CHECK(any.accepts(SigAlgo::kEd25519));
    CHECK(any.accepts(SigAlgo::kHybridEd25519MlDsa));
  }

  return bctest::summary();
}
