#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/interop/evm.hpp"
#include "blockchainer/interop/rpc.hpp"

/// Typed EVM JSON-RPC client (ARCHITECTURE §11). Method names and semantics are
/// the stable `eth_*` JSON-RPC surface; confirm against the target node's docs
/// at integration. Built on the pluggable [RpcTransport].
namespace blockchainer::interop {

using Bytes = std::vector<std::uint8_t>;

// ── Hex helpers (Ethereum "quantity" / "data" encodings) ────────────────────

/// Minimal hex quantity, "0x"-prefixed (zero → "0x0").
std::string to_quantity(codec::u128 v);
/// Parse a "0x"-prefixed hex quantity.
codec::u128 parse_quantity(const std::string& hex);
/// "0x" + lowercase hex of bytes.
std::string to_hex0x(codec::ByteView bytes);
/// Parse "0x" + hex into bytes (even length required).
Bytes parse_hex0x(const std::string& hex);

class EthClient {
 public:
  explicit EthClient(RpcTransport* transport) : rpc_(transport) {}

  std::uint64_t block_number();                                  // eth_blockNumber
  std::uint64_t chain_id();                                      // eth_chainId
  codec::u128 gas_price();                                       // eth_gasPrice
  codec::u128 get_balance(const evm::Address& a, const std::string& block = "latest");
  std::uint64_t get_transaction_count(const evm::Address& a, const std::string& block = "pending");
  Bytes eth_call(const evm::Address& to, const Bytes& data, const std::string& block = "latest");
  crypto::Hash send_raw_transaction(const Bytes& raw);           // eth_sendRawTransaction

 private:
  JsonRpcClient rpc_;
};

}  // namespace blockchainer::interop
