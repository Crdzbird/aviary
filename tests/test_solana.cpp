#include <map>
#include <string>
#include <vector>

#include "aviary/codec.hpp"
#include "aviary/interop/encoding.hpp"
#include "aviary/interop/json.hpp"
#include "aviary/interop/solana.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::interop;
using solana::Pubkey;

namespace {
codec::ByteView view(const std::vector<std::uint8_t>& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { std::vector<std::uint8_t> seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
Pubkey pk_of(const crypto::KeyPair& kp) { Pubkey p{}; std::copy(kp.public_key.begin(), kp.public_key.end(), p.begin()); return p; }
bool contains(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

class MockTransport : public RpcTransport {
 public:
  std::map<std::string, std::string> responses;
  std::vector<std::string> requests;
  std::string send(const std::string& req) override {
    requests.push_back(req);
    return responses.at(json::Value::parse(req)["method"].as_string());
  }
};
std::string ok(const std::string& result) { return "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":" + result + "}"; }
}  // namespace

int main() {
  init();

  std::printf("== addresses ==\n");
  CHECK(solana::to_address(solana::system_program()) == std::string(32, '1'));
  {
    auto kp = ed(1);
    Pubkey p = pk_of(kp);
    CHECK(solana::from_address(solana::to_address(p)) == p);
  }

  std::printf("== build + sign a transfer; signature verifies over the message ==\n");
  Pubkey blockhash{};
  blockhash.fill(0x07);
  auto alice = ed(1);
  auto bob = ed(2);
  std::vector<std::uint8_t> signed_tx;
  {
    Pubkey from = pk_of(alice), to = pk_of(bob);
    auto msg = solana::build_transfer_message(from, to, 1000000, blockhash);
    // Message starts with the header [1,0,1] and lists 3 accounts.
    CHECK(msg[0] == 1 && msg[1] == 0 && msg[2] == 1);

    signed_tx = solana::sign_transaction(msg, alice);
    CHECK(signed_tx[0] == 1);  // one signature (compact-u16)

    std::vector<std::uint8_t> sig(signed_tx.begin() + 1, signed_tx.begin() + 65);
    std::vector<std::uint8_t> recovered_msg(signed_tx.begin() + 65, signed_tx.end());
    CHECK(recovered_msg == msg);
    CHECK(crypto::ed25519_verify(view(recovered_msg), view(sig), view(alice.public_key)));
    // A different key does not verify.
    CHECK(!crypto::ed25519_verify(view(recovered_msg), view(sig), view(bob.public_key)));
  }

  std::printf("== SolanaClient over a mock transport ==\n");
  {
    MockTransport t;
    t.responses["getBalance"] = ok("{\"context\":{\"slot\":1},\"value\":1000000000}");
    Pubkey bh{};
    bh.fill(0x07);
    t.responses["getLatestBlockhash"] =
        ok("{\"context\":{\"slot\":1},\"value\":{\"blockhash\":\"" + solana::to_address(bh) +
           "\",\"lastValidBlockHeight\":123}}");
    t.responses["sendTransaction"] = ok("\"5sigBase58Signature\"");
    solana::SolanaClient sol(&t);

    CHECK(sol.get_balance(pk_of(alice)) == 1000000000ull);
    CHECK(sol.get_latest_blockhash() == bh);

    std::string sig = sol.send_transaction(signed_tx);
    CHECK(sig == "5sigBase58Signature");
    // The request carried the base64-encoded transaction.
    CHECK(contains(t.requests.back(), encoding::base64_encode(signed_tx)));
    CHECK(contains(t.requests.back(), "\"encoding\":\"base64\""));
  }

  return bctest::summary();
}
