# aviary (Node.js)

Aviary for Node.js via [koffi](https://koffi.dev) FFI — a full DLT (ledger,
BFT consensus, mempool, observability, light-client verification) plus
EVM/Bitcoin/Solana interop.

```js
const bc = require('aviary');

bc.preload();                       // optional: warm the native lib off-thread
                                    // (worker_thread). The API never needs await.

const node = bc.newNode(1n);
const a = node.newAccount();
const b = node.newAccount();
node.credit(a.address, 1_000_000n);
node.submitTransfer(a.secret, b.address, 250_000n, node.nonce(a.address));
node.produceBlock(1n);              // → 1  (finalized)
node.balance(b.address);           // → 250000

// Light-client proof
bc.verifyStateProof(node.stateRoot(), node.proveAccount(b.address)); // true

// EVM smart contract
bc.functionSelector('transfer(address,uint256)').toString('hex');    // a9059cbb
bc.signLegacyTx({ to, value: 0n, data: bc.erc20TransferCalldata(to, 1n), secret });
```

## Preload (off the main thread, no await)

`preload()` spawns a short-lived `worker_thread` that loads the shared libraries
and runs the one-time crypto-backend init. Because `dlopen` mapping + backend
init are process-global, this warms the path for the main thread's later lazy
load — the first synchronous call then pays no load cost. It is **optional and
fire-and-forget**: the synchronous API works identically whether or not you call
it, and never requires `await`.

## Native library

The native pair (`libaviary_c` + `libcryptolib_c`) ships under
`prebuilds/<platform>-<arch>/`, made relocatable so they load with no setup.
Rebuild + restage with `scripts/node/stage_prebuild.sh`. For development you can
point at any build with `AVIARY_DYLIB` / `CRYPTOLIB_DYLIB`.
