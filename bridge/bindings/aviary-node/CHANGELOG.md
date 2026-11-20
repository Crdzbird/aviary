# Changelog

## 1.1.1

The prebuilt engines are rebuilt from Aviary 1.1.1 on CipherBird 1.6.0.

- Snapshot restore verifies every archived commit certificate.
  `restoreChain`, `ChainTree.restore` and `chainSnapshotError` used to check
  only that an archived certificate below the tip named its block, so a
  snapshot whose certificate signature or signer bitmap had been altered
  restored and `archivedCertificate` then returned the forged certificate.
  Each certificate is now verified (signer list, quorum power, aggregate BLS
  signature) under the validator set in force at its height, following
  validator rotations. When that set is no longer in the snapshot (it was
  announced in a block pruned from the archive and has since rotated out),
  the certificate must equal the last commit embedded in the next archived
  header. A snapshot that fails either check is refused.
- A snapshot of a chain other than BFT that carries an archive is refused.

## 1.1.0

- The prebuilt engines are built against CipherBird 1.6.0, whose C ABI is
  now named `cipherbird_*`. `sealShare` and `sealAccountPassphrase` throw
  for a `mem` that is not a multiple of 1024 bytes; zero still selects the
  sensitive preset.
- Subchains version 2 (docs/design/SUBCHAINS.md Part 2): a parent whose
  genesis commits a dynamic policy (`encodeDynamicSubchainPolicy`,
  `encodeRegistrar`) registers, closes and re-keys children in any block
  through registrar signed operations (`opRegister`, `opClose`,
  `opSetRegistrars`, `opDigest`, `signOp`, `addOpApproval`,
  `Chain.proposeSubchain`). The registry is a Merkle tree of fixed 230 byte
  records: `Chain.registryCounts`, `childRecord`, `childProof`,
  `verifyChildProof`, `decodeChildRecord`, `pendingAnchor`,
  `Chain.subchainHopV2`, `encodeSubchainProofV2`, `verifySubchainProofV2`,
  `encodedHeaderHash`, and the `SigAlgo`, `ChildStatus`, `HopFlat` and
  `HopRecord` constants.
- Chain persistence: `Chain.tip`, `Chain.snapshot`, `restoreChain` and
  `chainSnapshotError`; a restored chain continues byte for byte. The block
  archive: `Chain.enableArchive`, `archiveRange`, `archivedBlock`,
  `archivedCertificate`, `pruneArchive`. Chain trees: `ChainTree.snapshot`
  and `ChainTree.restore`.
- `test/dynamic_subchain.test.js` replays `vectors/subchain_v2_vectors.txt`
  call for call.
- `cryptoBackendVersion()` now ends with `(CipherBird <revision>)`, the
  CipherBird revision the engine was built from.

## 1.0.0

First stable release of aviary-ledger, the Aviary ledger for Node.js over
koffi, built on the CipherBird 1.3.0 crypto engine. The public API follows semantic versioning
from this release on.

- The ledger: accounts (Ed25519 and hybrid post quantum), signed transfers,
  a mempool, BFT finality, state and inclusion proofs, snapshots and
  restore.
- Composable chains with foreign engines and proposers written in
  JavaScript, application records (`Chain.write`, `record`, `proveRecord`,
  `recordFromProof`, `writeFromStore`, `signRecordTxHybrid`), `Chain.addTx`
  for any signed transaction with queue events, subchains,
  validator rotation, light clients and chain trees.
- Networking: `NetNode` over an application owned transport, `PeerMesh`
  for in-process meshes, `SecureSession` and `NoiseSession` links, peer
  records and `AddressBook`, and the admission handshake. On record model
  networks `NetNode.record`, `recordNonce` and `proveRecord` answer on every
  node; `setTransactions` lists are cleared when the height commits, so the
  next height's list is set before `advance`.
- Custody: key stores that sign without revealing secrets, portable and
  threshold accounts. Assurance postures, sealed mempools and anonymous
  membership credentials.
- Events (`drainEvents`, `onEvent`) and Prometheus metrics on `Node`,
  `Chain` and `NetNode`.
- EVM, Bitcoin and Solana addresses and signing.
- TypeScript declarations for the whole API in `index.d.ts`.
- One self contained native library each for macOS arm64 and Linux x64
  and arm64 is bundled (6 MB package);
  the engine loads on first use or through `preload()`.
