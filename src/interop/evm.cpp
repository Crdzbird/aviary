#include <algorithm>
#include "aviary/interop/evm.hpp"

#include <stdexcept>

namespace aviary::interop::evm {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// Strip leading zero bytes (RLP integer canonical form).
Bytes strip_leading_zeros(const Bytes& b) {
  std::size_t i = 0;
  while (i < b.size() && b[i] == 0) ++i;
  return Bytes(b.begin() + static_cast<std::ptrdiff_t>(i), b.end());
}

rlp::Item str_item(const Bytes& b) { return rlp::Item::string(b); }

const char* kHexLower = "0123456789abcdef";
}  // namespace

Address address_from_pubkey(codec::ByteView pubkey) {
  codec::ByteView xy = pubkey;
  if (pubkey.size() == 65) {
    xy = codec::ByteView(pubkey.data() + 1, 64);  // drop 0x04 prefix
  } else if (pubkey.size() != 64) {
    throw std::runtime_error("evm: need 65-byte uncompressed (or 64-byte X||Y) pubkey");
  }
  Hash h = crypto::keccak256(xy);
  Address a{};
  std::copy(h.begin() + 12, h.end(), a.begin());  // last 20 bytes
  return a;
}

Address address_from_secret(codec::ByteView secret_key) {
  Bytes pk = crypto::secp256k1_pubkey(secret_key, /*compressed=*/false);
  return address_from_pubkey(view(pk));
}

std::string to_hex_address(const Address& a) {
  std::string s = "0x";
  for (std::uint8_t b : a) {
    s.push_back(kHexLower[b >> 4]);
    s.push_back(kHexLower[b & 0xF]);
  }
  return s;
}

std::string to_checksum_address(const Address& a) {
  // Lowercase hex (no prefix), hash it, uppercase letters where the hash nibble >= 8.
  std::string lower;
  for (std::uint8_t b : a) {
    lower.push_back(kHexLower[b >> 4]);
    lower.push_back(kHexLower[b & 0xF]);
  }
  Bytes ascii(lower.begin(), lower.end());
  Hash hash = crypto::keccak256(view(ascii));

  std::string out = "0x";
  for (std::size_t i = 0; i < lower.size(); ++i) {
    char c = lower[i];
    const std::uint8_t nibble = (i % 2 == 0) ? (hash[i / 2] >> 4) : (hash[i / 2] & 0xF);
    if (c >= 'a' && c <= 'f' && nibble >= 8) c = static_cast<char>(c - 'a' + 'A');
    out.push_back(c);
  }
  return out;
}

Bytes signing_rlp(const LegacyTx& tx) {
  std::vector<rlp::Item> fields;
  fields.push_back(str_item(rlp::uint_bytes(tx.nonce)));
  fields.push_back(str_item(rlp::uint_bytes_u128(tx.gas_price)));
  fields.push_back(str_item(rlp::uint_bytes(tx.gas_limit)));
  fields.push_back(str_item(tx.to ? Bytes(tx.to->begin(), tx.to->end()) : Bytes{}));
  fields.push_back(str_item(rlp::uint_bytes_u128(tx.value)));
  fields.push_back(str_item(tx.data));
  fields.push_back(str_item(rlp::uint_bytes(tx.chain_id)));
  fields.push_back(str_item(Bytes{}));  // r = 0
  fields.push_back(str_item(Bytes{}));  // s = 0
  return rlp::encode(rlp::Item::list(std::move(fields)));
}

Hash signing_hash(const LegacyTx& tx) {
  Bytes pre = signing_rlp(tx);
  return crypto::keccak256(view(pre));
}

SignedTx sign(const LegacyTx& tx, codec::ByteView secret_key) {
  Hash h = signing_hash(tx);
  Bytes sig = crypto::secp256k1_sign(codec::ByteView(h.data(), 32), secret_key);  // 65B
  if (sig.size() != 65) throw std::runtime_error("evm: bad signature size");

  SignedTx out;
  out.r = Bytes(sig.begin(), sig.begin() + 32);
  out.s = Bytes(sig.begin() + 32, sig.begin() + 64);
  const std::uint8_t recovery_id = sig[64];
  out.v = static_cast<std::uint64_t>(recovery_id) + 35 + 2 * tx.chain_id;

  std::vector<rlp::Item> fields;
  fields.push_back(str_item(rlp::uint_bytes(tx.nonce)));
  fields.push_back(str_item(rlp::uint_bytes_u128(tx.gas_price)));
  fields.push_back(str_item(rlp::uint_bytes(tx.gas_limit)));
  fields.push_back(str_item(tx.to ? Bytes(tx.to->begin(), tx.to->end()) : Bytes{}));
  fields.push_back(str_item(rlp::uint_bytes_u128(tx.value)));
  fields.push_back(str_item(tx.data));
  fields.push_back(str_item(rlp::uint_bytes(out.v)));
  fields.push_back(str_item(strip_leading_zeros(out.r)));
  fields.push_back(str_item(strip_leading_zeros(out.s)));
  out.raw = rlp::encode(rlp::Item::list(std::move(fields)));
  out.tx_hash = crypto::keccak256(view(out.raw));
  return out;
}

Address recover_sender(const LegacyTx& tx, const SignedTx& signed_tx) {
  // recovery_id = v - 35 - 2*chain_id (EIP-155).
  const std::uint64_t rec = signed_tx.v - 35 - 2 * tx.chain_id;
  Bytes sig65;
  sig65.insert(sig65.end(), signed_tx.r.begin(), signed_tx.r.end());
  sig65.insert(sig65.end(), signed_tx.s.begin(), signed_tx.s.end());
  sig65.push_back(static_cast<std::uint8_t>(rec));

  Hash h = signing_hash(tx);
  Bytes pk = crypto::secp256k1_recover(codec::ByteView(h.data(), 32), view(sig65));
  return address_from_pubkey(view(pk));
}

// -- EIP-1559 (type-2) -----------------------------------------------------------

namespace {
constexpr std::uint8_t kEip1559Type = 0x02;

rlp::Item access_list_item(const std::vector<AccessListEntry>& list) {
  std::vector<rlp::Item> entries;
  for (const auto& e : list) {
    std::vector<rlp::Item> keys;
    for (const auto& k : e.storage_keys) keys.push_back(str_item(Bytes(k.begin(), k.end())));
    std::vector<rlp::Item> pair;
    pair.push_back(str_item(Bytes(e.address.begin(), e.address.end())));
    pair.push_back(rlp::Item::list(std::move(keys)));
    entries.push_back(rlp::Item::list(std::move(pair)));
  }
  return rlp::Item::list(std::move(entries));
}

// The nine payload fields common to the signing preimage and the signed tx.
std::vector<rlp::Item> payload_fields(const Eip1559Tx& tx) {
  std::vector<rlp::Item> f;
  f.push_back(str_item(rlp::uint_bytes(tx.chain_id)));
  f.push_back(str_item(rlp::uint_bytes(tx.nonce)));
  f.push_back(str_item(rlp::uint_bytes_u128(tx.max_priority_fee_per_gas)));
  f.push_back(str_item(rlp::uint_bytes_u128(tx.max_fee_per_gas)));
  f.push_back(str_item(rlp::uint_bytes(tx.gas_limit)));
  f.push_back(str_item(tx.to ? Bytes(tx.to->begin(), tx.to->end()) : Bytes{}));
  f.push_back(str_item(rlp::uint_bytes_u128(tx.value)));
  f.push_back(str_item(tx.data));
  f.push_back(access_list_item(tx.access_list));
  return f;
}
}  // namespace

Bytes signing_bytes(const Eip1559Tx& tx) {
  Bytes rlp_payload = rlp::encode(rlp::Item::list(payload_fields(tx)));
  Bytes out;
  out.reserve(rlp_payload.size() + 1);
  out.push_back(kEip1559Type);  // EIP-2718 type prefix
  out.insert(out.end(), rlp_payload.begin(), rlp_payload.end());
  return out;
}

Hash signing_hash(const Eip1559Tx& tx) {
  Bytes pre = signing_bytes(tx);
  return crypto::keccak256(view(pre));
}

SignedEip1559 sign(const Eip1559Tx& tx, codec::ByteView secret_key) {
  Hash h = signing_hash(tx);
  Bytes sig = crypto::secp256k1_sign(codec::ByteView(h.data(), 32), secret_key);  // 65B
  if (sig.size() != 65) throw std::runtime_error("evm: bad signature size");

  SignedEip1559 out;
  out.r = Bytes(sig.begin(), sig.begin() + 32);
  out.s = Bytes(sig.begin() + 32, sig.begin() + 64);
  out.y_parity = sig[64];  // 0 or 1 for type-2

  std::vector<rlp::Item> fields = payload_fields(tx);
  fields.push_back(str_item(rlp::uint_bytes(out.y_parity)));  // 0 -> empty, 1 -> 0x01
  fields.push_back(str_item(strip_leading_zeros(out.r)));
  fields.push_back(str_item(strip_leading_zeros(out.s)));

  Bytes rlp_signed = rlp::encode(rlp::Item::list(std::move(fields)));
  out.raw.reserve(rlp_signed.size() + 1);
  out.raw.push_back(kEip1559Type);
  out.raw.insert(out.raw.end(), rlp_signed.begin(), rlp_signed.end());
  out.tx_hash = crypto::keccak256(view(out.raw));
  return out;
}

Address recover_sender(const Eip1559Tx& tx, const SignedEip1559& signed_tx) {
  Bytes sig65;
  sig65.insert(sig65.end(), signed_tx.r.begin(), signed_tx.r.end());
  sig65.insert(sig65.end(), signed_tx.s.begin(), signed_tx.s.end());
  sig65.push_back(signed_tx.y_parity);
  Hash h = signing_hash(tx);
  Bytes pk = crypto::secp256k1_recover(codec::ByteView(h.data(), 32), view(sig65));
  return address_from_pubkey(view(pk));
}

}  // namespace aviary::interop::evm
