#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/version.hpp"
#include "aviary/wire.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::wire;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
}  // namespace

int main() {
  init();

  std::printf("== Hello round-trip ==\n");
  {
    Message m = make(Hello{1, 7, 42});
    Message back = Message::decode_all(view(m.encode()));
    Hello h = as_hello(back);
    CHECK(h.version == 1 && h.chain_id == 7 && h.tip_height == 42);
  }

  std::printf("== TxMsg round-trip ==\n");
  {
    auto a = ed(1);
    Transaction tx; tx.version = 1; tx.chain_id = 7;
    tx.body = AccountStateMachine::encode_transfer(
        {0, crypto::address_of(view(ed(2).public_key)), 100});
    sign_transaction(tx, a);
    Message m = make(TxMsg{tx});
    TxMsg back = as_tx(Message::decode_all(view(m.encode())));
    CHECK(back.tx.encode() == tx.encode());
    CHECK(back.tx.txid() == tx.txid());
  }

  std::printf("== BlockMsg round-trip ==\n");
  {
    Block blk;
    blk.header.version = 1; blk.header.chain_id = 7; blk.header.height = 1;
    consensus::CommitCertificate cert;
    cert.height = 1; cert.round = 0;
    cert.block_id = blk.header.hash();
    cert.signers = {0, 1, 2};
    cert.agg_signature = Bytes(96, 0xAB);
    Message m = make(BlockMsg{blk, cert});
    BlockMsg back = as_block(Message::decode_all(view(m.encode())));
    CHECK(back.block.encode() == blk.encode());
    CHECK(back.commit.encode() == cert.encode());
  }

  std::printf("== GetBlocks round-trip ==\n");
  {
    Message m = make(GetBlocks{5, 16});
    GetBlocks g = as_get_blocks(Message::decode_all(view(m.encode())));
    CHECK(g.from_height == 5 && g.count == 16);
  }

  std::printf("== canonical rejection ==\n");
  {
    // Unknown message type byte.
    Bytes bad = make(Hello{1, 7, 0}).encode();
    bad[0] = 0x7F;  // not a valid MessageType
    CHECK_THROWS(Message::decode_all(view(bad)));

    // Trailing bytes.
    Bytes tr = make(GetBlocks{1, 1}).encode();
    tr.push_back(0x00);
    CHECK_THROWS(Message::decode_all(view(tr)));

    // Wrong accessor for the message type.
    Message hello = make(Hello{1, 7, 0});
    CHECK_THROWS(as_tx(hello));
  }

  return bctest::summary();
}
