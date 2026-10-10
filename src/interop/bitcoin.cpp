#include "aviary/interop/bitcoin.hpp"

#include <stdexcept>

#include "aviary/crypto.hpp"
#include "aviary/interop/encoding.hpp"

namespace aviary::interop::bitcoin {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// ── Bech32 (BIP173) ──────────────────────────────────────────────────────────
const char* kBech32Charset = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

std::uint32_t bech32_polymod(const std::vector<std::uint8_t>& values) {
  static const std::uint32_t gen[5] = {0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3};
  std::uint32_t chk = 1;
  for (std::uint8_t v : values) {
    std::uint32_t top = chk >> 25;
    chk = ((chk & 0x1ffffff) << 5) ^ v;
    for (int i = 0; i < 5; ++i) {
      if ((top >> i) & 1) chk ^= gen[i];
    }
  }
  return chk;
}

std::vector<std::uint8_t> bech32_hrp_expand(const std::string& hrp) {
  std::vector<std::uint8_t> out;
  for (char c : hrp) out.push_back(static_cast<std::uint8_t>(c) >> 5);
  out.push_back(0);
  for (char c : hrp) out.push_back(static_cast<std::uint8_t>(c) & 31);
  return out;
}

std::string bech32_encode(const std::string& hrp, const std::vector<std::uint8_t>& data) {
  std::vector<std::uint8_t> values = bech32_hrp_expand(hrp);
  values.insert(values.end(), data.begin(), data.end());
  values.insert(values.end(), 6, 0);
  std::uint32_t mod = bech32_polymod(values) ^ 1;
  std::vector<std::uint8_t> checksum(6);
  for (int i = 0; i < 6; ++i) checksum[i] = (mod >> (5 * (5 - i))) & 31;

  std::string out = hrp + "1";
  for (std::uint8_t d : data) out.push_back(kBech32Charset[d]);
  for (std::uint8_t d : checksum) out.push_back(kBech32Charset[d]);
  return out;
}

// Convert 8-bit bytes to 5-bit groups (pad on the high end).
std::vector<std::uint8_t> convertbits_8to5(const Bytes& in) {
  std::vector<std::uint8_t> out;
  std::uint32_t acc = 0;
  int bits = 0;
  for (std::uint8_t b : in) {
    acc = (acc << 8) | b;
    bits += 8;
    while (bits >= 5) {
      bits -= 5;
      out.push_back((acc >> bits) & 31);
    }
  }
  if (bits > 0) out.push_back((acc << (5 - bits)) & 31);
  return out;
}
}  // namespace

Bytes hash160(codec::ByteView data) {
  crypto::Hash sha = crypto::sha256(data);
  return crypto::ripemd160(codec::ByteView(sha.data(), sha.size()));
}

Bytes hash256(codec::ByteView data) {
  crypto::Hash a = crypto::sha256(data);
  crypto::Hash b = crypto::sha256(codec::ByteView(a.data(), a.size()));
  return Bytes(b.begin(), b.end());
}

std::string base58check_encode(std::uint8_t version, const Bytes& payload) {
  Bytes data;
  data.push_back(version);
  data.insert(data.end(), payload.begin(), payload.end());
  Bytes checksum = hash256(view(data));
  data.insert(data.end(), checksum.begin(), checksum.begin() + 4);
  return encoding::base58_encode(data);
}

std::pair<std::uint8_t, Bytes> base58check_decode(const std::string& s) {
  Bytes data = encoding::base58_decode(s);
  if (data.size() < 5) throw std::runtime_error("base58check: too short");
  Bytes body(data.begin(), data.end() - 4);
  Bytes checksum = hash256(view(body));
  for (int i = 0; i < 4; ++i) {
    if (checksum[i] != data[data.size() - 4 + i]) throw std::runtime_error("base58check: bad checksum");
  }
  return {body[0], Bytes(body.begin() + 1, body.end())};
}

std::string bech32_segwit_encode(const std::string& hrp, std::uint8_t witness_version,
                                 const Bytes& program) {
  std::vector<std::uint8_t> data;
  data.push_back(witness_version);
  std::vector<std::uint8_t> prog5 = convertbits_8to5(program);
  data.insert(data.end(), prog5.begin(), prog5.end());
  return bech32_encode(hrp, data);
}

std::string p2pkh_address(codec::ByteView compressed_pubkey, std::uint8_t version) {
  return base58check_encode(version, hash160(compressed_pubkey));
}

std::string p2wpkh_address(codec::ByteView compressed_pubkey, const std::string& hrp) {
  return bech32_segwit_encode(hrp, 0, hash160(compressed_pubkey));
}

std::string p2wpkh_address_from_secret(codec::ByteView secret_key, const std::string& hrp) {
  Bytes pk = crypto::secp256k1_pubkey(secret_key, /*compressed=*/true);
  return p2wpkh_address(view(pk), hrp);
}

// ── Transactions (P2WPKH / BIP143) ───────────────────────────────────────────

namespace {
void put_u32_le(Bytes& out, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}
void put_u64_le(Bytes& out, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}
void put_compact_size(Bytes& out, std::uint64_t n) {
  if (n < 0xFD) {
    out.push_back(static_cast<std::uint8_t>(n));
  } else if (n <= 0xFFFF) {
    out.push_back(0xFD);
    out.push_back(static_cast<std::uint8_t>(n & 0xFF));
    out.push_back(static_cast<std::uint8_t>((n >> 8) & 0xFF));
  } else {
    out.push_back(0xFE);
    put_u32_le(out, static_cast<std::uint32_t>(n));
  }
}
void put_var_bytes(Bytes& out, const Bytes& b) {
  put_compact_size(out, b.size());
  out.insert(out.end(), b.begin(), b.end());
}

// DER-encode a single integer (minimal big-endian, prepend 0x00 if high bit set).
void der_int(Bytes& out, const Bytes& v32) {
  std::size_t i = 0;
  while (i < v32.size() - 1 && v32[i] == 0) ++i;  // strip leading zeros (keep ≥1)
  Bytes mag(v32.begin() + static_cast<std::ptrdiff_t>(i), v32.end());
  out.push_back(0x02);
  if (mag[0] & 0x80) {
    out.push_back(static_cast<std::uint8_t>(mag.size() + 1));
    out.push_back(0x00);
  } else {
    out.push_back(static_cast<std::uint8_t>(mag.size()));
  }
  out.insert(out.end(), mag.begin(), mag.end());
}
}  // namespace

Bytes p2wpkh_script_code(const Bytes& pubkey_hash) {
  Bytes s = {0x76, 0xa9, 0x14};  // OP_DUP OP_HASH160 PUSH20
  s.insert(s.end(), pubkey_hash.begin(), pubkey_hash.end());
  s.push_back(0x88);  // OP_EQUALVERIFY
  s.push_back(0xac);  // OP_CHECKSIG
  return s;
}

Bytes p2wpkh_script_pubkey(const Bytes& pubkey_hash) {
  Bytes s = {0x00, 0x14};  // OP_0 PUSH20
  s.insert(s.end(), pubkey_hash.begin(), pubkey_hash.end());
  return s;
}

Bytes bip143_sighash(const Tx& tx, std::size_t index, const Bytes& script_code,
                     std::uint64_t amount, std::uint32_t sighash_type) {
  // hashPrevouts = sha256d( ‖ outpoint(txid ‖ vout LE) ).
  Bytes prevouts;
  for (const auto& in : tx.inputs) {
    prevouts.insert(prevouts.end(), in.prevout.txid.begin(), in.prevout.txid.end());
    put_u32_le(prevouts, in.prevout.vout);
  }
  Bytes hash_prevouts = hash256(view(prevouts));

  // hashSequence = sha256d( ‖ nSequence LE ).
  Bytes sequences;
  for (const auto& in : tx.inputs) put_u32_le(sequences, in.sequence);
  Bytes hash_sequence = hash256(view(sequences));

  // hashOutputs = sha256d( ‖ value LE ‖ var(scriptPubKey) ).
  Bytes outputs;
  for (const auto& out : tx.outputs) {
    put_u64_le(outputs, out.value);
    put_var_bytes(outputs, out.script_pubkey);
  }
  Bytes hash_outputs = hash256(view(outputs));

  const TxIn& in = tx.inputs.at(index);
  Bytes pre;
  put_u32_le(pre, tx.version);
  pre.insert(pre.end(), hash_prevouts.begin(), hash_prevouts.end());
  pre.insert(pre.end(), hash_sequence.begin(), hash_sequence.end());
  pre.insert(pre.end(), in.prevout.txid.begin(), in.prevout.txid.end());
  put_u32_le(pre, in.prevout.vout);
  put_var_bytes(pre, script_code);
  put_u64_le(pre, amount);
  put_u32_le(pre, in.sequence);
  pre.insert(pre.end(), hash_outputs.begin(), hash_outputs.end());
  put_u32_le(pre, tx.locktime);
  put_u32_le(pre, sighash_type);

  return hash256(view(pre));
}

Bytes der_encode_signature(const Bytes& rs64) {
  if (rs64.size() != 64) throw std::runtime_error("bitcoin: signature must be 64 bytes");
  Bytes r(rs64.begin(), rs64.begin() + 32);
  Bytes s(rs64.begin() + 32, rs64.end());
  Bytes body;
  der_int(body, r);
  der_int(body, s);
  Bytes out;
  out.push_back(0x30);
  out.push_back(static_cast<std::uint8_t>(body.size()));
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

Bytes sign_p2wpkh_input(const Tx& tx, std::size_t index, std::uint64_t amount,
                        codec::ByteView secret_key, const Bytes& pubkey_hash,
                        std::uint32_t sighash_type) {
  Bytes script_code = p2wpkh_script_code(pubkey_hash);
  Bytes sighash = bip143_sighash(tx, index, script_code, amount, sighash_type);
  Bytes sig65 = crypto::secp256k1_sign(view(sighash), secret_key);  // r‖s‖recid, low-S
  Bytes der = der_encode_signature(Bytes(sig65.begin(), sig65.begin() + 64));
  der.push_back(static_cast<std::uint8_t>(sighash_type));  // append sighash-type byte
  return der;
}

Bytes serialize_segwit(const Tx& tx, const std::vector<std::pair<Bytes, Bytes>>& witnesses) {
  Bytes out;
  put_u32_le(out, tx.version);
  out.push_back(0x00);  // SegWit marker
  out.push_back(0x01);  // SegWit flag
  put_compact_size(out, tx.inputs.size());
  for (const auto& in : tx.inputs) {
    out.insert(out.end(), in.prevout.txid.begin(), in.prevout.txid.end());
    put_u32_le(out, in.prevout.vout);
    put_compact_size(out, 0);  // empty scriptSig for native segwit
    put_u32_le(out, in.sequence);
  }
  put_compact_size(out, tx.outputs.size());
  for (const auto& o : tx.outputs) {
    put_u64_le(out, o.value);
    put_var_bytes(out, o.script_pubkey);
  }
  // Witness: per input, a stack of [signature, pubkey].
  for (const auto& w : witnesses) {
    put_compact_size(out, 2);
    put_var_bytes(out, w.first);
    put_var_bytes(out, w.second);
  }
  put_u32_le(out, tx.locktime);
  return out;
}

}  // namespace aviary::interop::bitcoin
