#pragma once
#include <cstdint>

#include "aviary/codec.hpp"
#include "aviary/keyring.hpp"
#include "aviary/types.hpp"

/// Signing straight from a KeyStore: the sealed secret is revealed inside the
/// core, used once, and wiped, so it never crosses an API boundary. The
/// scheme follows the stored public key: 32 bytes is Ed25519, anything longer
/// is the hybrid Ed25519 + ML-DSA-65 key. A zero fee produces the version 1
/// envelope (byte identical to a plain transfer); a fee selects version 2.
namespace aviary::custody {

/// A signed transfer of [amount] from the stored account at [from] to [to].
/// Throws if the store is locked or [from] is unknown.
Transaction sign_transfer(const KeyStore& store, const Hash& from, const Hash& to,
                          codec::u128 amount, std::uint64_t nonce, codec::u128 fee,
                          std::uint64_t chain_id);

/// A signed record write (record state model) by the stored account at
/// [from]: put, transfer or delete of [key] with [value]. The scheme follows
/// the stored key as for transfers; a hybrid key yields a version 2 envelope.
/// Throws if the store is locked or [from] is unknown.
Transaction sign_record(const KeyStore& store, const Hash& from, std::uint64_t chain_id,
                        std::uint64_t nonce, const Hash& key, std::uint8_t op,
                        const Bytes& value);
/// Overwrites [bytes] with zeros in a way the optimizer keeps.
void wipe(Bytes& bytes);

}  // namespace aviary::custody
