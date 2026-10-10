#include <algorithm>
#include "aviary/interop/solana.hpp"

#include <stdexcept>

#include "aviary/codec.hpp"
#include "aviary/interop/encoding.hpp"
#include "aviary/interop/json.hpp"

namespace aviary::interop::solana {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
void append(Bytes& out, const Pubkey& k) { out.insert(out.end(), k.begin(), k.end()); }

Pubkey to_pubkey(const Bytes& b) {
  if (b.size() != 32) throw std::runtime_error("solana: key not 32 bytes");
  Pubkey k{};
  std::copy(b.begin(), b.end(), k.begin());
  return k;
}
}  // namespace

std::string to_address(const Pubkey& key) {
  return encoding::base58_encode(Bytes(key.begin(), key.end()));
}

Pubkey from_address(const std::string& address) {
  return to_pubkey(encoding::base58_decode(address));
}

Pubkey system_program() { return Pubkey{}; }  // 32 zero bytes

Bytes build_transfer_message(const Pubkey& from, const Pubkey& to,
                             std::uint64_t lamports, const Pubkey& recent_blockhash) {
  Bytes msg;
  // Message header: 1 required signature, 0 readonly-signed, 1 readonly-unsigned.
  msg.push_back(1);
  msg.push_back(0);
  msg.push_back(1);

  // Account keys: [from (signer,writable), to (writable), systemProgram (readonly)].
  encoding::compact_u16_encode(3, msg);
  append(msg, from);
  append(msg, to);
  append(msg, system_program());

  // Recent blockhash.
  append(msg, recent_blockhash);

  // One instruction.
  encoding::compact_u16_encode(1, msg);
  msg.push_back(2);  // programIdIndex → systemProgram (account index 2)
  // Instruction accounts: indices [from=0, to=1].
  encoding::compact_u16_encode(2, msg);
  msg.push_back(0);
  msg.push_back(1);
  // Instruction data: u32 LE discriminator (2 = Transfer) ‖ u64 LE lamports.
  Bytes data = {2, 0, 0, 0};
  for (int i = 0; i < 8; ++i) data.push_back(static_cast<std::uint8_t>((lamports >> (8 * i)) & 0xFF));
  encoding::compact_u16_encode(static_cast<std::uint16_t>(data.size()), msg);
  msg.insert(msg.end(), data.begin(), data.end());

  return msg;
}

Bytes sign_transaction(const Bytes& message, const crypto::KeyPair& keypair) {
  Bytes sig = crypto::ed25519_sign(view(message), view(keypair.secret_key));  // 64 bytes
  Bytes tx;
  encoding::compact_u16_encode(1, tx);  // one signature
  tx.insert(tx.end(), sig.begin(), sig.end());
  tx.insert(tx.end(), message.begin(), message.end());
  return tx;
}

// ── RPC client ───────────────────────────────────────────────────────────────

std::uint64_t SolanaClient::get_balance(const Pubkey& account) {
  json::Value params = json::Value::array({json::Value::string(to_address(account))});
  json::Value result = rpc_.call("getBalance", std::move(params));
  // result = { context, value: <lamports> }
  return static_cast<std::uint64_t>(result["value"].as_number());
}

Pubkey SolanaClient::get_latest_blockhash() {
  json::Value result = rpc_.call("getLatestBlockhash", json::Value::array({}));
  // result = { context, value: { blockhash: <base58>, lastValidBlockHeight } }
  const std::string bh = result["value"]["blockhash"].as_string();
  return from_address(bh);
}

std::string SolanaClient::send_transaction(const Bytes& signed_tx) {
  json::Value cfg = json::Value::object();
  cfg.set("encoding", json::Value::string("base64"));
  json::Value params = json::Value::array(
      {json::Value::string(encoding::base64_encode(signed_tx)), std::move(cfg)});
  return rpc_.call("sendTransaction", std::move(params)).as_string();
}

}  // namespace aviary::interop::solana
