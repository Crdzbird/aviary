# Changelog

## 0.0.1

- Initial Go binding for the Aviary C ABI (cgo). **Full ABI coverage** —
  every `aviary_*` C function is wrapped.
  - Canonical bytes: `Hash`, `Txid`, `HeaderHash`.
  - Verification: `VerifyStateProof`, `VerifyTxInclusion`, `VerifyCommit`.
  - Node (ledger + BFT + mempool + observability): `NewNode`, `NewAccount`,
    `Credit`, `SubmitTransfer`, `ProduceBlock`, `Height`, `MempoolSize`,
    `Balance`, `Nonce`, `StateRoot`, `DrainEvents`, `Metrics`, `ProveAccount`,
    `Free`; plus `AccountAddress`.
  - Interop: `EthAddress`, `BtcAddress`, `SolAddress`, `EthErc20TransferCalldata`,
    `EthSelector`, `EthSignLegacy`.
- Lazy + background init: the API self-initializes on first use (`sync.Once`);
  `Preload()` warms it on a goroutine; `Init()` / `Initialized()` for explicit
  control.
- Crypto backend via the depended-on cryptolib Go binding (full crypto surface
  available alongside the ledger). `CryptoBackendVersion()` reports the linked
  backend version, proving the link.
- Linking: `cgo_local.go` (default), `cgo_pkgconfig.go` (`-tags
  aviary_pkgconfig`), `cgo_static.go` (`-tags aviary_static`, fully
  self-contained), and a `!cgo` build guard.
