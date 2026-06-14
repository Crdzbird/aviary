# Changelog

## 0.0.1

- Initial release of the Blockchainer Flutter plugin (`ffiPlugin`, dart:ffi).
- Full DLT surface via the high-level `Blockchainer` / `Node` API: create a
  chain, accounts, signed transfers, block production (BFT finality), mempool,
  streaming events, Prometheus metrics, light-client state proofs, and
  EVM/Bitcoin/Solana interop + EVM contract signing.
- **Background-isolate loading**: `Blockchainer.ensureInitialized()` warms the
  native libraries + crypto-backend init off the UI isolate and completes when
  ready; `Blockchainer.preload()` is the fire-and-forget variant;
  `Blockchainer.instance` is a lazy-synchronous singleton (no `await` on ledger
  calls). On Android a startup `ContentProvider` also warms the libraries off
  the main thread at launch.
- Native packaging: iOS/macOS via a dynamic `BlockchainerC.xcframework` (Swift
  Package Manager); Android via per-ABI `jniLibs`. The crypto backend is
  provided by the `cryptolib_flutter` package (depended on, not bundled);
  Blockchainer's native resolves cryptolib's C ABI at runtime.
