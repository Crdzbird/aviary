#include "aviary/custody_signing.hpp"

#include <stdexcept>

#include "aviary/account_state.hpp"

namespace aviary::custody {

void wipe(Bytes& bytes) {
  volatile std::uint8_t* p = bytes.data();
  for (std::size_t i = 0; i < bytes.size(); ++i) p[i] = 0;
}

Transaction sign_transfer(const KeyStore& store, const Hash& from, const Hash& to,
                          codec::u128 amount, std::uint64_t nonce, codec::u128 fee,
                          std::uint64_t chain_id) {
  crypto::KeyPair kp;
  kp.public_key = store.public_key(from);
  if (kp.public_key.empty()) throw std::runtime_error("keystore: unknown account");
  kp.secret_key = store.reveal_secret(from);
  const bool hybrid = kp.public_key.size() != 32;
  Transaction tx;
  tx.chain_id = chain_id;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amount});
  tx.version = (hybrid || fee != 0) ? 2 : 1;
  tx.sig_algo = static_cast<std::uint8_t>(hybrid ? SigAlgo::kHybridEd25519MlDsa : SigAlgo::kEd25519);
  tx.fee = fee;
  try {
    sign_transaction(tx, kp);
  } catch (...) {
    wipe(kp.secret_key);
    throw;
  }
  wipe(kp.secret_key);
  return tx;
}

}  // namespace aviary::custody
