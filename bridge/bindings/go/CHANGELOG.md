# Changelog

## 1.1.0

Needs libaviary_c built from the same source.

- Built on CipherBird 1.6.0: the binding imports CipherBird's Go package
  from `bridge/bindings/go/cipherbird` and links `libcipherbird_c`.
- `PortableAccount.SealPassphrase` and `ShareCard.Seal` return an error for
  a `mem` that is not a multiple of 1024 bytes; zero still selects the
  sensitive preset.
- Subchains version 2: `DynamicSubchainPolicy`, `Registrar`, `OpRegister`,
  `OpClose`, `OpSetRegistrars`, `OpDigest`, `SignOp`, `AddOpApproval`,
  `Chain.ProposeSubchain`, `Chain.RegistryCounts`, `Chain.ChildRecord`,
  `Chain.ChildProof`, `VerifyChildProof`, `PendingAnchor`,
  `Chain.SubchainHopV2`, `EncodeSubchainProofV2`, `VerifySubchainProofV2`
  and `EncodedHeaderHash`.
- Persistence: `Chain.Snapshot`, `RestoreChain`, `ChainSnapshotError`,
  `Chain.Tip`, the opt in archive (`Chain.EnableArchive`, `ArchiveRange`,
  `ArchivedBlock`, `ArchivedCertificate`, `PruneArchive`),
  `ChainTree.Snapshot` and `RestoreChainTree`. `RestoreChain` verifies
  every archived commit certificate under the validator set in force at its
  height, across validator rotations, and refuses a snapshot carrying a
  forged one.
- `CryptoBackendVersion()` now ends with `(CipherBird <revision>)`, the
  CipherBird revision the engine was built from.

## 1.0.0

First stable release. The public API follows semantic versioning from this
release on.

- A faster engine: each transaction's signature is verified once on its
  way into a block instead of three times, and consensus no longer repeats
  vote and block checks within a height.
- Built against CipherBird 1.3.0 (libsodium 1.0.22, OpenSSL 3.5.9, public
  outputs such as digests, ciphertexts and signatures in plain memory
  instead of page-locked buffers); the CipherBird Go module pin moves to
  that commit.
- Application records: `Chain.Write`, `Record`, `ProveRecord`,
  `RecordNonce`, `AddTx`, `PendingCount`, `ChainID`, `StateRoot`;
  `SignRecordTx`, `AddressOf`, `RecordFromProof`.
- `SetEventHook`, `DrainEvents` and `Metrics` on `Chain` and `NetNode`.
- Records signed inside the engine from a key store (`KeyStore.SignRecord`,
  `Chain.WriteFromStore`) and by hybrid accounts (`SignRecordTxHybrid`);
  the chain queue reports `tx_admitted` and `tx_removed` events.
- `PeerMesh`, a deterministic in-memory mesh of `NetNode`s.
- Peer records and `AddressBook`, `NoiseSession`, and the admission
  handshake (`NewAdmissionChallenge`, `CredentialPolicy.AdmissionAnswer`,
  `CredentialGate.AdmissionGranted`).
- `NetNode.Record`, `RecordNonce` and `ProveRecord` on record model
  networks. `SetTransactions` lists are cleared when the height commits, so
  the next height's list is set before `Advance` and reaches its round 0
  proposal.

## 0.1.0

- Module path `github.com/Crdzbird/aviary/bridge/bindings/go`, depending on
  the CipherBird Go module.
- Event hook, snapshot and restore, key store backed signing, `NetNode` and
  `SecureSession` wrappers.

## 0.0.1

- Initial Go binding for the Aviary C ABI over cgo, covering every
  `aviary_*` function at the time: canonical bytes (`Hash`, `Txid`,
  `HeaderHash`), verification (`VerifyStateProof`, `VerifyTxInclusion`,
  `VerifyCommit`), the node, and EVM, Bitcoin and Solana interop.
- Lazy initialization on first use, `Preload()` on a goroutine, and
  `Init()` and `Initialized()` for explicit control.
- Linking through `cgo_local.go` (default), `cgo_pkgconfig.go`
  (`-tags aviary_pkgconfig`) or `cgo_static.go` (`-tags aviary_static`).
