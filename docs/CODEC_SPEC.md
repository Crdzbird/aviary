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
| `0x10` | Dynamic registry leaf: `H(0x10 || canonical(ChildRecord))` |
| `0x11` | Dynamic registry commitment (`subchain_root` of a version 4 header) |
| `0x12` | Registry operation leaf under `ops_root`: `H(0x12 || canonical(SubchainOp))` |
| `0x13` | Registry operation authorisation digest (what a registrar signs) |
| `0x14` | Registrar set commitment (`authority_hash`) |

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

### Subchain layouts, version 2 (header version 4; docs/design/SUBCHAINS.md Part 2)

A parent whose genesis `subchain_policy` is a dynamic policy produces version 4
headers. Versions 1 to 3 are unchanged: `ops_root` and the operation list are
written and read only from version 4 on.

- `BlockHeader` v4 = v3 layout || `ops_root[32]`, before `consensus_data`.
- `Block` v4 = `bytes(header) || list(bytes(tx)) || list(bytes(anchor)) || list(bytes(op))`.
- `ops_root` = Merkle root over `H(0x12 || canonical(op))` in block order; the
  zero hash for no operations.
- `DynamicSubchainPolicy` = `00 00 00 || u8 format(=2) || u8 max_depth ||
  u32 max_children || u16 max_anchors_per_block || u16 max_headers_per_anchor ||
  u16 max_ops_per_block || RegistrarSet || list(bytes(ChildRegistration))`.
  It occupies the same genesis field as the version 1 `SubchainPolicy`. The
  three zero bytes can never begin a valid version 1 policy (they would read as
  `max_depth = 0`, which is rejected), so the two formats cannot be confused.
- `RegistrarSet` = `u8 threshold || list(u8 sig_algo || bytes(public_key))`;
  `sig_algo` is 1 (Ed25519), 2 (hybrid Ed25519 + ML-DSA-65) or 4 (SLH-DSA).
  `authority_hash` = `H(0x14 || canonical(RegistrarSet))`.
- `SubchainOp` = `u8 kind || u32 authority_epoch || u64 valid_until ||
  bytes(payload) || list(u16 registrar_index || bytes(signature))`, approvals
  ascending and unique by index. `kind` 1 register (payload = `ChildRegistration`),
  2 close (payload = `u64 chain_id || u64 final_height || final_tip_hash[32]`),
  3 set registrars (payload = `RegistrarSet`).
- Authorisation digest = `H(0x13 || u64 parent_chain_id || parent_genesis_hash[32] ||
  u8 kind || u32 authority_epoch || u64 valid_until || bytes(payload))`. Each
  approval signs these 32 bytes with its registrar's scheme.
- `ChildRecord` (230 bytes, fixed) = `u32 index || u64 chain_id || u8 status ||
  u8 child_max_depth || genesis_header_hash[32] || u64 registered_height ||
  u64 closed_height || u64 tip_height || tip_hash[32] || tip_state_root[32] ||
  tip_subchain_root[32] || vset_hash[32] || tip_vset_hash[32]`. `status` is 1
  (open) or 2 (closed); `closed_height` is zero exactly when the record is open.

  | Offset | Field | Offset | Field |
  |---|---|---|---|
  | 0 | index u32 | 62 | tip_height u64 |
  | 4 | chain_id u64 | 70 | tip_hash |
  | 12 | status u8 | 102 | tip_state_root |
  | 13 | child_max_depth u8 | 134 | tip_subchain_root |
  | 14 | genesis_header_hash | 166 | vset_hash |
  | 46 | registered_height u64 | 198 | tip_vset_hash |
  | 54 | closed_height u64 | 230 | end |

- `subchain_root` (v4) = `H(0x11 || u32 record_count || u32 open_count ||
  u32 authority_epoch || authority_hash[32] || children_root[32])`, where
  `children_root` is the Merkle root (the tree below) over
  `H(0x10 || canonical(ChildRecord_i))` in index order.
- `ChildProof` = `bytes(ChildRecord) || u32 record_count || u32 open_count ||
  u32 authority_epoch || authority_hash[32] || list(sibling[32])`, siblings
  bottom up. The record's own `index` is the leaf position.
- `SubchainProofV2` = `list(u8 kind || bytes(hop)) || bytes(StateProof)`; hop
  kind 1 is the version 1 hop above, kind 2 is a `ChildProof`.
- Ledger snapshot of a dynamic parent appends
  `bytes(u32 authority_epoch || RegistrarSet || list(u64 chain_id || u8 status ||
  u8 child_max_depth || genesis_header_hash[32] || u64 registered_height ||
  u64 closed_height || bytes(tip_header) || sets))`, where `sets` is
  `bytes(vset) || bytes(tip_vset)` for an open child and
  `vset_hash[32] || tip_vset_hash[32]` for a closed one. Records are in index
  order.

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

## Chain snapshot

What `aviary_chain_snapshot` writes and `aviary_chain_restore` reads. Strict:
a wrong magic, an unknown version, a trailing byte, or any section that is not
byte for byte what the restored chain would write again, is refused.

- `ChainSnapshot` = `"AVCS" || u8 version(=1) || genesis_hash[32] ||
  bytes(ledger) || list(bytes(Receipt)) || bytes(engine) || bool has_archive ||
  [bytes(archive)]`.
- `ledger` = the ledger snapshot: `bytes(tip_header) || bytes(state)`, then
  for a version 1 parent `bytes(child tips)` and for a dynamic parent the
  registry section above.
- The receipts are those of the tip block; they must hash to its
  `receipt_root`.
- `engine`, by consensus engine: BFT = `bytes(vset) || bytes(certifying_vset) ||
  bool has_cert || [bytes(CommitCertificate)] || bool has_scheduled ||
  [bytes(scheduled_vset)] || list(u32 equivocator)` (ascending, unique); proof
  of work = the 32 byte current target; every other built in engine = empty.
- `archive` = the block archive: `u32 count || (u64 height || bytes(Block) ||
  bytes(CommitCertificate))*`, heights ascending and contiguous, ending at the
  tip.
- A snapshot holds no secret key.

Chain tree snapshot = `"AVTS" || u8 version(=1) || list(u64 chain_id ||
bytes(ChainSnapshot))` in pre-order (parents before children).

## Record model (state model 2)

Transaction body: `u64 nonce || fixed32 key || u8 op || bytes value`, with
op 0 put, 1 transfer (value is the new 32 byte owner address), 2 delete
(value empty). The value is at most 65536 bytes.

State leaves, in this order: for every record in key order
`0x01 || fixed32 key || fixed32 owner || u64 version || bytes value`; then
for every author in address order `0x02 || fixed32 address || u64 nonce`.
The state root is the Merkle root over `hash_tagged(kLeaf, leaf)` of those
leaves, the same construction as the account model. A record proof is a
`StateProof` whose leaf preimage is the record leaf.

Snapshot: `u32 count || (key || owner || u64 version || bytes value)* ||
u32 count || (address || u64 nonce)*`.

