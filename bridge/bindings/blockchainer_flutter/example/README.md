# blockchainer_flutter example

A minimal app demonstrating the recommended library usage:

1. **Background init** — `await Blockchainer.ensureInitialized()` in `main()`
   loads the native library + crypto backend on a background isolate, so the UI
   thread never stalls.
2. **Synchronous ledger** — after init, the API needs no `await`: create a chain,
   fund accounts, transfer, and produce blocks directly.

```bash
flutter run            # any connected device / simulator / desktop
```

For a full-featured dashboard (streaming events, metrics, light-client proofs,
multi-chain interop), see `examples/blockchainer_demo` in the repository.
