#include <map>
#include <string>
#include <vector>

#include "blockchainer/interop/abi.hpp"
#include "blockchainer/interop/eth_client.hpp"
#include "blockchainer/interop/evm.hpp"
#include "blockchainer/interop/json.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::interop;

namespace {
codec::ByteView view(const std::vector<std::uint8_t>& b) { return codec::ByteView(b.data(), b.size()); }
std::vector<std::uint8_t> unhex(const std::string& s) {
  std::vector<std::uint8_t> out;
  std::size_t i = (s.rfind("0x", 0) == 0) ? 2 : 0;
  for (; i + 1 < s.size(); i += 2) out.push_back(static_cast<std::uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
  return out;
}
bool contains(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }

// Mock transport: returns a canned response per method, captures requests.
class MockTransport : public RpcTransport {
 public:
  std::map<std::string, std::string> responses;
  std::vector<std::string> requests;
  std::string send(const std::string& req) override {
    requests.push_back(req);
    auto v = json::Value::parse(req);
    return responses.at(v["method"].as_string());
  }
};

std::string ok_response(const std::string& result_json) {
  return "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":" + result_json + "}";
}
}  // namespace

int main() {
  init();

  std::printf("== json round-trip ==\n");
  {
    const std::string text =
        "{\"jsonrpc\":\"2.0\",\"id\":7,\"params\":[\"0xabc\",true,null,12],\"nested\":{\"k\":\"v\"}}";
    json::Value v = json::Value::parse(text);
    CHECK(v["jsonrpc"].as_string() == "2.0");
    CHECK(v["id"].as_number() == 7);
    CHECK(v["params"].items().size() == 4);
    CHECK(v["params"].items()[1].as_bool() == true);
    CHECK(v["params"].items()[2].is_null());
    CHECK(v["nested"]["k"].as_string() == "v");
    // Re-parse of dump is structurally identical.
    CHECK(json::Value::parse(v.dump())["nested"]["k"].as_string() == "v");
  }
  CHECK_THROWS(json::Value::parse("{bad"));
  CHECK_THROWS(json::Value::parse("{}trailing"));
  // Deeply-nested input is rejected (no stack-overflow DoS from adversarial RPC).
  CHECK_THROWS(json::Value::parse(std::string(5000, '[') + std::string(5000, ']')));

  std::printf("== eth_* quantity reads ==\n");
  {
    MockTransport t;
    t.responses["eth_blockNumber"] = ok_response("\"0x10\"");
    t.responses["eth_chainId"] = ok_response("\"0x1\"");
    t.responses["eth_gasPrice"] = ok_response("\"0x3b9aca00\"");
    t.responses["eth_getBalance"] = ok_response("\"0xde0b6b3a7640000\"");
    t.responses["eth_getTransactionCount"] = ok_response("\"0x5\"");
    EthClient eth(&t);

    evm::Address a{};
    CHECK(eth.block_number() == 16);
    CHECK(eth.chain_id() == 1);
    CHECK(eth.gas_price() == static_cast<codec::u128>(1000000000ull));
    CHECK(eth.get_balance(a) == static_cast<codec::u128>(1000000000000000000ull));
    CHECK(eth.get_transaction_count(a) == 5);

    // Request shape: getBalance carries the method, address, and block tag.
    bool found = false;
    for (const auto& r : t.requests) {
      if (contains(r, "\"method\":\"eth_getBalance\"") && contains(r, "\"latest\"")) found = true;
    }
    CHECK(found);
  }

  std::printf("== eth_call decodes an ABI return ==\n");
  {
    MockTransport t;
    // balanceOf returns uint256 = 1000 (0x3e8) as a 32-byte word.
    std::string word = "0x" + std::string(61, '0') + "3e8";
    t.responses["eth_call"] = ok_response("\"" + word + "\"");
    EthClient eth(&t);
    evm::Address token{};
    auto ret = eth.eth_call(token, abi::encode_call("balanceOf(address)", {abi::encode_address(token)}));
    CHECK(abi::decode_uint(view(ret)) == 1000);
  }

  std::printf("== sign a tx and send it (full flow) ==\n");
  {
    MockTransport t;
    t.responses["eth_chainId"] = ok_response("\"0x1\"");
    t.responses["eth_getTransactionCount"] = ok_response("\"0x3\"");
    t.responses["eth_gasPrice"] = ok_response("\"0x3b9aca00\"");
    const std::string tx_hash = "0x" + std::string(64, '1');
    t.responses["eth_sendRawTransaction"] = ok_response("\"" + tx_hash + "\"");
    EthClient eth(&t);

    auto sk = unhex("ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80");
    evm::Address from = evm::address_from_secret(view(sk));

    evm::LegacyTx tx;
    tx.nonce = eth.get_transaction_count(from);
    tx.gas_price = eth.gas_price();
    tx.gas_limit = 21000;
    tx.to = from;  // self-send
    tx.value = static_cast<codec::u128>(1000);
    tx.chain_id = eth.chain_id();
    CHECK(tx.nonce == 3);

    evm::SignedTx st = evm::sign(tx, view(sk));
    crypto::Hash h = eth.send_raw_transaction(st.raw);

    // Returned the server's tx hash, and the request carried our raw tx.
    crypto::Hash expected{};
    expected.fill(0x11);
    CHECK(h == expected);
    CHECK(contains(t.requests.back(), to_hex0x(view(st.raw))));
  }

  std::printf("== JSON-RPC error surfaces as RpcError ==\n");
  {
    MockTransport t;
    t.responses["eth_blockNumber"] =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32000,\"message\":\"boom\"}}";
    EthClient eth(&t);
    CHECK_THROWS(eth.block_number());
  }

  return bctest::summary();
}
