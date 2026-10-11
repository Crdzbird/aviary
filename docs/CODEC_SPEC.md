# Aviary Canonical Codec - Specification (DRAFT)

> The byte-exact encoding rules every hashed object obeys. This is the
> determinism contract (ARCHITECTURE section 2, section 4): one value has **exactly one** valid
> encoding; the encoder only ever emits it, and the decoder **rejects** anything
> else rather than normalizing it (normalization would let two byte strings share
> a hash -> malleability).
>
> Status: **DRAFT** for M1. To be implemented in C++ (`src/codec/`) and exercised
> by `vectors/` golden vectors replayed through every binding.

## Global rules

- **Endianness:** big-endian (network order) for all integers.
- **No floats. No locale. No map/iteration-order dependence.** Sets that must be
  hashed are encoded as lists **sorted by canonical key bytes**, ascending, with
  duplicates rejected.
- **Struct fields** are encoded in a fixed, declared schema order - never by
  reflection.
- **Decoding is total and strict:** every read is bounds-checked; trailing bytes
  after a top-level decode are an error; any non-canonical form is an error.

## Primitive encodings

| Type | Encoding |
|------|----------|
| `u8`,`u16`,`u32`,`u64` | fixed 1/2/4/8 bytes, big-endian, unsigned. `u64` values are `< 2^63` (writer rejects negative; reader rejects the high-bit-set range). |
| `bool` | one byte: `0x00` (false) / `0x01` (true). Any other byte -> error. |
| `u128` (value/amount) | fixed **16 bytes**, big-endian. Range `[0, 2^128)`. Used for balances/amounts; value math is checked - overflow is a fork. |
| `bytes` (variable) | `u32` length prefix + raw bytes. Max length bounded (allocation-DoS guard) and checked against bytes remaining. |
| `fixed_bytes(n)` | exactly `n` raw bytes, **no** length prefix (hashes, public keys - size fixed by schema). |
| `biguint` | minimal-length big-endian, length-prefixed as `bytes`. **No leading zero byte.** Zero encodes as an empty byte string (`u32` length `0`). Reader rejects a leading zero byte (non-canonical). |
| `list<T>` | `u32` count prefix + each element encoded in order. Count checked against bytes remaining. Order is part of the canonical form (sort sets before encoding). |

> A reference implementation of exactly these rules (with round-trip,
> canonical-rejection, and golden-vector tests) was prototyped in Dart during
> design; M1 ports it to C++ as the single source of truth.

## Hashing & domain separation

Primary hash: **BLAKE3-256** (`aviary::crypto::blake3_256`). Every hashed
structure is prefixed with a 1-byte **domain tag** so a leaf can never collide
with an internal node or a header:

| Tag | Meaning |
|-----|---------|
| `0x00` | Merkle leaf:    `H(0x00 || canonical(x))` |
| `0x01` | Merkle node:    `H(0x01 || left || right)` |
| `0x02` | Block header:   `H(0x02 || canonical(header))` |
| `0x03` | Transaction id: `H(0x03 || canonical(tx))` |
| `0x0C` | Subchain anchor leaf: `H(0x0C || canonical(anchor))` |
| `0x0D` | Subchain registry commitment (`subchain_root`) |
| `0x0E` | Subchain policy (parent genesis) |
| `0x0F` | Parent binding (child genesis) |

### Subchain layouts (header version 3; docs/design/SUBCHAINS.md)

- `BlockHeader` v3 = v2 layout || `anchor_root[32]` || `subchain_root[32]`, before
  `consensus_data`. v1/v2 encodings are unchanged.
- `Block` v3 = `bytes(header) || list(bytes(tx)) || list(bytes(anchor))`; the anchor
  list is present only when the header version is >= 3.
- `CertifiedHeader` = `bytes(header) || bytes(certificate)` (certificate = canonical
  `CommitCertificate`).
- `Anchor` = `u64 child_chain_id || list(bytes(CertifiedHeader))` (consecutive heights).
- `ChildRegistration` = `u64 chain_id || bytes(genesis_header) || list(bytes(bls_pubkey) || u64 power) || u8 child_max_depth`
  (identity = the child's genesis HEADER hash - its genesis hash would be circular with the child's ParentBinding).
- `subchain_root` = `H(0x0D || u32 count || per child ascending: u64 chain_id || genesis_header_hash[32] || tip_hash[32] || u64 tip_height || tip_state_root[32] || vset_hash[32] || tip_vset_hash[32])` (the child's validator set in force next, and the set that certified the tip).
- `SubchainProof` = `list(bytes(hop)) || bytes(StateProof)`, hop = `list(u64 chain_id || genesis_header_hash[32] || bytes(tip_header) || vset_hash[32] || tip_vset_hash[32]) || u64 child_chain_id`.
- `SubchainPolicy` = `u16 max_children || u8 max_depth || u16 max_anchors_per_block || u16 max_headers_per_anchor || list(bytes(ChildRegistration))`.
- `ParentBinding` = `u64 parent_chain_id || parent_genesis_hash[32]`.
- `GenesisSpec` gains `bytes(subchain_policy) || bytes(parent_binding)` appended last
  (empty when unused).

(Tags are provisional; finalized when the block/tx model lands in M1.)

## Merkle commitment

- Binary Merkle tree over `H_leaf(tx)` in block order.
- **Odd node:** carry the lone node up unchanged (do **not** duplicate-last) - this
  avoids the CVE-2012-2459 duplicate-node malleability class.
- Inclusion proof = `(index, sibling_hashes[])`; verification recomputes the root.

## Open items (resolve during M1)

1. `u128` vs arbitrary-precision for value fields (ARCHITECTURE section 17.1).
2. State-trie key/proof format (section 17.2).
3. Final domain-tag table and any additional roots (receipts).
