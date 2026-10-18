# Changelog

## 0.1.0

First release of aviary-ledger, the Aviary ledger for Node.js over koffi,
built on the CipherBird 1.1.1 crypto engine.

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
  records and `AddressBook`, and the admission handshake.
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
