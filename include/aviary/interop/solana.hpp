#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "aviary/crypto.hpp"
#include "aviary/interop/rpc.hpp"

/// Solana interop (ARCHITECTURE §11). Ed25519 signing (cryptolib) + Base58
/// addresses + the legacy message/transaction format. Builds a System-Program
/// transfer end to end.
namespace aviary::interop::solana {

using Bytes = std::vector<std::uint8_t>;
using Pubkey = std::array<std::uint8_t, 32>;

/// Base58 address of a 32-byte public key, and back.
std::string to_address(const Pubkey& key);
Pubkey from_address(const std::string& address);

/// The System Program id (32 zero bytes → "111…1").
Pubkey system_program();

/// Build the (unsigned) message for a lamport transfer from → to, with a recent
/// blockhash. Accounts: [from(signer,writable), to(writable), systemProgram].
Bytes build_transfer_message(const Pubkey& from, const Pubkey& to,
                             std::uint64_t lamports, const Pubkey& recent_blockhash);

/// Ed25519-sign a message and assemble the wire transaction:
/// compact_u16(1) ‖ signature(64) ‖ message.
Bytes sign_transaction(const Bytes& message, const crypto::KeyPair& keypair);

/// Typed Solana JSON-RPC client.
class SolanaClient {
 public:
  explicit SolanaClient(RpcTransport* transport) : rpc_(transport) {}

  std::uint64_t get_balance(const Pubkey& account);   // getBalance → lamports
  Pubkey get_latest_blockhash();                       // getLatestBlockhash
  std::string send_transaction(const Bytes& signed_tx);  // sendTransaction → signature (base58)

 private:
  JsonRpcClient rpc_;
};

}  // namespace aviary::interop::solana
