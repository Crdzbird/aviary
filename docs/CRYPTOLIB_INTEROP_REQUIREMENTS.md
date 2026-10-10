# cryptolib primitives required for M6 interoperability

> What aviary needs added to **cryptolib's C ABI** (`bridge/cryptolib_c.h`
> + impl) to support outbound EVM and Bitcoin interop. Solana needs nothing new
> (Ed25519 + SHA-256 already present). Follows cryptolib's existing conventions:
> `CryptoBuffer`/`CryptoBufferResult` (malloc'd, caller frees via
> `cryptolib_buffer_free`/`cryptolib_str_free`), `CryptoKeyPair`, verify returns
> `int` 1/0, inputs are borrowed const pointers.
>
> **Recommended upstream:** vendor **libsecp256k1** (bitcoin-core) for all EC
> operations — audited, constant-time, includes RFC6979 deterministic nonces,
> low-S normalization, and the public-key recovery module. Keccac-256 and
> RIPEMD-160 are small standalone additions.

---

## 1. secp256k1 ECDSA — EVM + Bitcoin signing

ECDSA over secp256k1 signs a **32-byte message digest** (not the message). For
Ethereum the signature must be **recoverable** (carry a recovery id `v`) and
**low-S normalized** (EIP-2); Bitcoin also requires low-S (BIP-62).

```c
/** secp256k1 keypair. sk = 32 bytes; pk = 65 bytes uncompressed (0x04 ‖ X ‖ Y). */
CRYPTO_API CryptoKeyPair cryptolib_secp256k1_keygen(void);

/** Derive the public key from a 32-byte secret key.
 *  compressed: 1 → 33 bytes (0x02/0x03 ‖ X), 0 → 65 bytes (0x04 ‖ X ‖ Y). */
CRYPTO_API CryptoBufferResult cryptolib_secp256k1_pubkey(
    const uint8_t* secret_key, size_t sk_len, int compressed);

/** Sign a 32-byte digest. RFC6979 deterministic nonce, low-S normalized.
 *  Returns 65 bytes: r(32) ‖ s(32) ‖ recovery_id(1, value 0..3). */
CRYPTO_API CryptoBufferResult cryptolib_secp256k1_sign(
    const uint8_t* digest32, const uint8_t* secret_key, size_t sk_len);

/** Verify. sig is 64 bytes (r ‖ s); pk is 33 or 65 bytes. Returns 1 if valid. */
CRYPTO_API int cryptolib_secp256k1_verify(
    const uint8_t* digest32,
    const uint8_t* sig, size_t sig_len,
    const uint8_t* public_key, size_t pk_len);

/** Recover the public key (65-byte uncompressed) from a 32-byte digest and a
 *  65-byte recoverable signature (r ‖ s ‖ recovery_id). This is Ethereum's
 *  ecrecover and is how a sender address is derived from a signed tx. */
CRYPTO_API CryptoBufferResult cryptolib_secp256k1_recover(
    const uint8_t* digest32, const uint8_t* sig65);
```

Notes:
- **Digest signing**: callers pass an already-computed 32-byte hash (Ethereum:
  `keccak256(rlp(tx))`; Bitcoin: `sha256(sha256(preimage))`).
- **Low-S is mandatory.** Without it, signatures are malleable and Ethereum
  rejects them (EIP-2).
- **Recovery id** for Ethereum: the tx `v` is derived from it (legacy:
  `v = recovery_id + 27`, or `recovery_id + 35 + 2*chain_id` for EIP-155).

KAT (high confidence — the secp256k1 generator point):
- `secret_key = 0x00…01` → uncompressed pubkey
  `04 79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798
      483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8`
- For ECDSA sign/recover, validate against **libsecp256k1's own test suite** and
  **RFC 6979 §A.2.5** (secp256k1, SHA-256) rather than a hand-picked vector.

## 2. Keccak-256 — Ethereum

Ethereum uses **original Keccak** (padding byte `0x01`), **not** NIST SHA3-256
(padding `0x06`). They produce different outputs — this distinction is critical.

```c
/** Keccak-256 (original padding). 32-byte output. NOT NIST SHA3-256. */
CRYPTO_API CryptoBufferResult cryptolib_keccak256(const uint8_t* msg, size_t msg_len);
```

KAT (verifiable, widely published):
- `keccak256("")`    = `c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470`
- `keccak256("abc")` = `4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45`

Used for: tx hashing, contract address derivation, ABI 4-byte selectors,
event topic hashing, and EIP-55 address checksums.

## 3. RIPEMD-160 — Bitcoin

```c
/** RIPEMD-160. 20-byte output. */
CRYPTO_API CryptoBufferResult cryptolib_ripemd160(const uint8_t* msg, size_t msg_len);
```

KAT:
- `ripemd160("")`    = `9c1185a5c5e9fc54612808977ee8f548b2258d31`
- `ripemd160("abc")` = `8eb208f7e05d987a9b044a8e98c6b087f15a0bfc`

Bitcoin `HASH160(x) = ripemd160(sha256(x))` composes from this + the existing
`cryptolib_sha256`. Bitcoin `HASH256(x) = sha256(sha256(x))` already composable.

---

## What aviary derives from these (no further cryptolib help needed)

- **Ethereum address** = last 20 bytes of `keccak256(uncompressed_pubkey[1:])`
  (drop the `0x04` prefix). EIP-55 checksum via keccak256.
- **Bitcoin P2PKH/P2WPKH** = `HASH160(compressed_pubkey)` + Base58Check / Bech32
  (aviary implements Base58/Bech32).
- **RLP**, **ABI encoding**, **JSON-RPC** — all in aviary.

## Binding note

Expose the same functions in cryptolib's **Dart** binding too if you want
client-side EVM/BTC crypto from Dart; aviary's core links the **C ABI**
directly, so the C functions above are the only hard requirement.

## Priority

`keccak256` + `secp256k1` (sign/pubkey/recover) unblock **EVM** — the highest-value
target. `ripemd160` unblocks **Bitcoin** addresses. Solana needs none of this.
