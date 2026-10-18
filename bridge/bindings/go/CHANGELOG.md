# Changelog

## 0.1.1

- Built against CipherBird 1.1.1 (engine hardening, libsodium 1.0.22,
  OpenSSL 3.5.9); the CipherBird Go module pin moves to that commit.
- Application records: `Chain.Write`, `Record`, `ProveRecord`,
  `RecordNonce`, `AddTx`, `PendingCount`, `ChainID`, `StateRoot`;
  `SignRecordTx`, `AddressOf`, `RecordFromProof`.
- `SetEventHook`, `DrainEvents` and `Metrics` on `Chain` and `NetNode`.

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
