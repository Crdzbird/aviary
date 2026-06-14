#include <cstdint>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
Bytes str_bytes(const char* s) {
  Bytes b;
  for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s));
  return b;
}
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
}  // namespace

int main() {
  init();

  auto kp = crypto::ed25519_keygen();
  CHECK(kp.public_key.size() == 32);
  CHECK(kp.secret_key.size() == 64);

  std::printf("== transaction sign/verify + round-trip ==\n");
  Transaction tx;
  tx.version = 1;
  tx.chain_id = 7;
  tx.body = str_bytes("transfer:alice->bob:100");
  sign_transaction(tx, kp);
  CHECK(verify_transaction_signature(tx));

  Bytes enc = tx.encode();
  Transaction back = Transaction::decode_all(view(enc));
  CHECK(back.encode() == enc);                     // round-trip
  CHECK(back.txid() == tx.txid());                 // txid stable across decode
  CHECK(verify_transaction_signature(back));

  // Tamper: flip a body byte -> signature must fail.
  Transaction tampered = tx;
  tampered.body[0] ^= 0x01;
  CHECK(!verify_transaction_signature(tampered));
  CHECK(tampered.txid() != tx.txid());

  // Wrong signer: re-sign with a different key changes author + sig.
  auto kp2 = crypto::ed25519_keygen();
  Transaction tx2 = tx;
  sign_transaction(tx2, kp2);
  CHECK(verify_transaction_signature(tx2));
  CHECK(tx2.author != tx.author);

  std::printf("== non-canonical rejection ==\n");
  Bytes trailing = enc;
  trailing.push_back(0xFF);
  CHECK_THROWS(Transaction::decode_all(view(trailing)));

  std::printf("== block header round-trip + hash ==\n");
  BlockHeader h;
  h.chain_id = 7;
  h.height = 42;
  h.timestamp = 1000;
  h.tx_root = compute_tx_root({tx, tx2});
  h.state_root = crypto::zero_hash();
  Bytes henc = h.encode();
  BlockHeader hback = BlockHeader::decode_all(view(henc));
  CHECK(hback.encode() == henc);
  CHECK(hback.hash() == h.hash());

  std::printf("== golden vectors (cross-language, see vectors/block_vectors.txt) ==\n");
  {
    Transaction g;
    g.version = 1;
    g.chain_id = 7;
    g.body = Bytes{'h', 'e', 'l', 'l', 'o'};
    g.author = Bytes(32, 0x11);
    g.signature = Bytes(64, 0x22);
    CHECK(crypto::to_hex(g.txid()) ==
          "be99a206b50a87b74e8359ab3a2895ed2f082169089b7ff9ec416d9df18d36dd");

    BlockHeader gh;
    gh.version = 1;
    gh.chain_id = 7;
    gh.height = 42;
    gh.parent_hash = crypto::zero_hash();
    gh.timestamp = 1000;
    gh.tx_root.fill(0xAA);
    gh.state_root.fill(0xBB);
    CHECK(crypto::to_hex(gh.hash()) ==
          "0ad7e2e233a9ebf827717d33b72a685eb993d8bac70b3cee517ba8023dd79793");
  }

  std::printf("== block round-trip + tx_root determinism ==\n");
  Block blk;
  blk.header = h;
  blk.transactions = {tx, tx2};
  Bytes benc = blk.encode();
  Block bback = Block::decode_all(view(benc));
  CHECK(bback.encode() == benc);
  CHECK(bback.transactions.size() == 2);
  CHECK(compute_tx_root(bback.transactions) == h.tx_root);

  // tx order matters: swapping changes the root.
  CHECK(compute_tx_root({tx, tx2}) != compute_tx_root({tx2, tx}));

  return bctest::summary();
}
