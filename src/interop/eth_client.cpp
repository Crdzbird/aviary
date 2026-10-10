#include "aviary/interop/eth_client.hpp"

#include <stdexcept>

namespace aviary::interop {

namespace {
const char* kHex = "0123456789abcdef";

json::Value arr(std::vector<json::Value> items) {
  return json::Value::array(std::move(items));
}

std::string addr_hex(const evm::Address& a) {
  return to_hex0x(codec::ByteView(a.data(), a.size()));
}

int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  throw std::runtime_error("eth: bad hex digit");
}
}  // namespace

std::string to_quantity(codec::u128 v) {
  if (v == 0) return "0x0";
  std::string digits;
  while (v > 0) {
    digits.push_back(kHex[static_cast<int>(v & 0xF)]);
    v >>= 4;
  }
  return "0x" + std::string(digits.rbegin(), digits.rend());
}

codec::u128 parse_quantity(const std::string& hex) {
  std::size_t i = (hex.rfind("0x", 0) == 0) ? 2 : 0;
  if (i == hex.size()) throw std::runtime_error("eth: empty quantity");
  codec::u128 v = 0;
  for (; i < hex.size(); ++i) v = (v << 4) | static_cast<codec::u128>(hex_val(hex[i]));
  return v;
}

std::string to_hex0x(codec::ByteView bytes) {
  std::string s = "0x";
  for (std::uint8_t b : bytes) {
    s.push_back(kHex[b >> 4]);
    s.push_back(kHex[b & 0xF]);
  }
  return s;
}

Bytes parse_hex0x(const std::string& hex) {
  std::size_t i = (hex.rfind("0x", 0) == 0) ? 2 : 0;
  if ((hex.size() - i) % 2 != 0) throw std::runtime_error("eth: odd-length hex");
  Bytes out;
  out.reserve((hex.size() - i) / 2);
  for (; i + 1 < hex.size(); i += 2) {
    out.push_back(static_cast<std::uint8_t>((hex_val(hex[i]) << 4) | hex_val(hex[i + 1])));
  }
  return out;
}

std::uint64_t EthClient::block_number() {
  return static_cast<std::uint64_t>(parse_quantity(rpc_.call("eth_blockNumber", arr({})).as_string()));
}

std::uint64_t EthClient::chain_id() {
  return static_cast<std::uint64_t>(parse_quantity(rpc_.call("eth_chainId", arr({})).as_string()));
}

codec::u128 EthClient::gas_price() {
  return parse_quantity(rpc_.call("eth_gasPrice", arr({})).as_string());
}

codec::u128 EthClient::get_balance(const evm::Address& a, const std::string& block) {
  json::Value result = rpc_.call(
      "eth_getBalance", arr({json::Value::string(addr_hex(a)), json::Value::string(block)}));
  return parse_quantity(result.as_string());
}

std::uint64_t EthClient::get_transaction_count(const evm::Address& a, const std::string& block) {
  json::Value result = rpc_.call(
      "eth_getTransactionCount", arr({json::Value::string(addr_hex(a)), json::Value::string(block)}));
  return static_cast<std::uint64_t>(parse_quantity(result.as_string()));
}

Bytes EthClient::eth_call(const evm::Address& to, const Bytes& data, const std::string& block) {
  json::Value call_obj = json::Value::object();
  call_obj.set("to", json::Value::string(addr_hex(to)));
  call_obj.set("data", json::Value::string(to_hex0x(codec::ByteView(data.data(), data.size()))));
  json::Value result = rpc_.call("eth_call", arr({std::move(call_obj), json::Value::string(block)}));
  return parse_hex0x(result.as_string());
}

crypto::Hash EthClient::send_raw_transaction(const Bytes& raw) {
  json::Value result = rpc_.call(
      "eth_sendRawTransaction", arr({json::Value::string(to_hex0x(codec::ByteView(raw.data(), raw.size())))}));
  Bytes h = parse_hex0x(result.as_string());
  if (h.size() != 32) throw std::runtime_error("eth: tx hash not 32 bytes");
  crypto::Hash out{};
  std::copy(h.begin(), h.end(), out.begin());
  return out;
}

}  // namespace aviary::interop
