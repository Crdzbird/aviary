// Blockchainer for Node.js — koffi FFI bindings.
//
// The native library is bundled in this package under prebuilds/<platform>-<arch>/
// and loaded automatically; consumers never provide a path:
//
//   const bc = require('blockchainer');
//   const node = bc.newNode(1n);
//   const { secret, address } = node.newAccount();
//   node.credit(address, 1_000_000n);
//
// Blockchainer's native links cryptolib's C ABI; libcryptolib_c is loaded first
// (its symbols are then process-global) so libblockchainer_c resolves them.
//
// NOTE for publishing: ship a self-contained pair of dylibs (or statically link
// cryptolib) and rewrite the install-name to @loader_path so it runs without the
// dev build tree. During local development set BLOCKCHAINER_DYLIB / CRYPTOLIB_DYLIB.

'use strict';
const path = require('node:path');
const koffi = require('koffi');

// ── Resolve the bundled native libraries for this platform/arch ──────────────
function ext() {
  return process.platform === 'darwin' ? 'dylib' : process.platform === 'win32' ? 'dll' : 'so';
}
function triple() { return `${process.platform}-${process.arch}`; }
function resolveBlockchainer() {
  return process.env.BLOCKCHAINER_DYLIB ||
    path.join(__dirname, 'prebuilds', triple(), `libblockchainer_c.${ext()}`);
}
function resolveCryptolib() {
  // Optional: load cryptolib first so its symbols are present in the process.
  return process.env.CRYPTOLIB_DYLIB ||
    path.join(__dirname, 'prebuilds', triple(), `libcryptolib_c.${ext()}`);
}

// ── Lazy native binding ──────────────────────────────────────────────────────
// The native library loads on FIRST USE (or early via preload()), never at
// require() time — importing this module is cheap and never blocks the event
// loop. All public methods are synchronous; the first to run triggers the
// (memoized) load. No `await` is ever required to call the ledger API.
let _native = null;
function ensureLoaded() {
  if (_native) return _native;

  // Load cryptolib first (best-effort) so libblockchainer_c's cryptolib_*
  // imports resolve. dlopen is process-global.
  try { koffi.load(resolveCryptolib()); } catch { /* may already be linked via rpath */ }
  const lib = koffi.load(resolveBlockchainer());

  const Buf = koffi.struct('BlockchainerBuffer', { data: 'void *', len: 'size_t' });
  koffi.struct('BlockchainerBufferResult', { buf: Buf, error: 'void *' });

  const f = (sig) => lib.func(sig);
  _native = {
    init: f('int blockchainer_init()'),
    version: f('const char *blockchainer_version()'),
    backend: f('const char *blockchainer_crypto_backend_version()'),
    hash: f('BlockchainerBufferResult blockchainer_hash(uint8_t*, size_t)'),
    verifyState: f('int blockchainer_verify_state_proof(uint8_t*, uint8_t*, size_t)'),
    verifyTx: f('int blockchainer_verify_tx_inclusion(uint8_t*, uint8_t*, size_t)'),
    // Node
    nodeNew: f('void *blockchainer_node_new(uint64_t)'),
    nodeFree: f('void blockchainer_node_free(void*)'),
    newAccount: f('BlockchainerBufferResult blockchainer_node_new_account(void*)'),
    accountAddress: f('BlockchainerBufferResult blockchainer_account_address(uint8_t*, size_t)'),
    credit: f('int blockchainer_node_credit(void*, uint8_t*, uint64_t)'),
    submit: f('BlockchainerBufferResult blockchainer_node_submit_transfer(void*, uint8_t*, size_t, uint8_t*, uint64_t, uint64_t)'),
    produce: f('int blockchainer_node_produce_block(void*, uint64_t, _Out_ uint64_t *)'),
    height: f('uint64_t blockchainer_node_height(void*)'),
    balance: f('uint64_t blockchainer_node_balance(void*, uint8_t*)'),
    nonce: f('uint64_t blockchainer_node_nonce(void*, uint8_t*)'),
    mempool: f('uint64_t blockchainer_node_mempool_size(void*)'),
    stateRoot: f('BlockchainerBufferResult blockchainer_node_state_root(void*)'),
    drainEvents: f('BlockchainerBufferResult blockchainer_node_drain_events(void*)'),
    metrics: f('BlockchainerBufferResult blockchainer_node_metrics(void*)'),
    proveAccount: f('BlockchainerBufferResult blockchainer_node_prove_account(void*, uint8_t*)'),
    // Interop
    ethAddr: f('BlockchainerBufferResult blockchainer_eth_address(uint8_t*)'),
    btcAddr: f('BlockchainerBufferResult blockchainer_btc_address(uint8_t*)'),
    solAddr: f('BlockchainerBufferResult blockchainer_sol_address(uint8_t*)'),
    erc20: f('BlockchainerBufferResult blockchainer_eth_erc20_transfer(uint8_t*, uint64_t)'),
    selector: f('BlockchainerBufferResult blockchainer_eth_selector(const char*)'),
    signLegacy: f('BlockchainerBufferResult blockchainer_eth_sign_legacy(uint64_t, uint64_t, uint64_t, uint8_t*, int, uint64_t, uint8_t*, size_t, uint64_t, uint8_t*)'),
    // Frees
    bufFree: f('void blockchainer_buffer_free(BlockchainerBuffer*)'),
    strFree: f('void blockchainer_str_free(void*)'),
  };
  return _native;
}
const fn = new Proxy({}, { get: (_t, p) => ensureLoaded()[p] });

// ── Helpers ──────────────────────────────────────────────────────────────────
const u8 = (x) => Buffer.isBuffer(x) ? x : Buffer.from(x);
const bytes = (cb) => (!cb.data || Number(cb.len) === 0)
  ? Buffer.alloc(0) : Buffer.from(koffi.decode(cb.data, 'uint8_t', Number(cb.len)));
function consume(r, op) {
  if ((!r.buf.data || Number(r.buf.len) === 0) && r.error) {
    const m = koffi.decode(r.error, 'char *'); fn.strFree(r.error); throw new Error(m);
  }
  if (!r.buf.data) throw new Error(`${op || 'call'} failed`);
  const out = bytes(r.buf); fn.bufFree(r.buf); return out;
}

// Internal: synchronously load + initialize the native lib. Used by the worker
// in preload() and reachable from the main thread for the lazy fallback.
function _warm() {
  const a = ensureLoaded();
  if (a.init() !== 0) throw new Error('blockchainer init failed');
  a.version();
  return true;
}

// Optionally warm the native binding OFF the main thread.
//
// Spawns a short-lived worker_thread that loads the shared libraries and runs
// the one-time crypto-backend init. dlopen mapping + backend init are
// process-global, so this warms the path for the main thread's later lazy load.
// Returns a Promise<boolean> you may ignore (fire-and-forget) or await; the
// ledger API never requires awaiting it. Falls back to a same-thread warm.
function preload() {
  return new Promise((resolve) => {
    let settled = false;
    const done = (v) => { if (!settled) { settled = true; resolve(v); } };
    let Worker;
    try { ({ Worker } = require('node:worker_threads')); }
    catch { try { _warm(); } catch { /* ignore */ } return done(true); }
    try {
      const code = `
        const { workerData, parentPort } = require('node:worker_threads');
        try { require(workerData.modulePath)._warm(); parentPort.postMessage({ ok: true }); }
        catch (e) { parentPort.postMessage({ ok: false, error: String(e) }); }`;
      const w = new Worker(code, { eval: true, workerData: { modulePath: __filename } });
      w.once('message', (m) => { w.terminate(); done(!!(m && m.ok)); });
      w.once('error', () => done(false));
      w.once('exit', (c) => done(c === 0));
    } catch { done(false); }
  });
}

// ── A live in-memory chain node (ledger + BFT + mempool + observability) ──────
class Node {
  constructor(chainId) {
    this._h = fn.nodeNew(BigInt(chainId));
    if (!this._h) throw new Error('node creation failed');
  }
  free() { if (this._h) { fn.nodeFree(this._h); this._h = null; } }

  // Fresh Ed25519 account: 64-byte secret (public = secret[32:64]), 32-byte addr.
  newAccount() {
    const secret = consume(fn.newAccount(this._h), 'new_account');
    const publicKey = secret.subarray(32, 64);
    const address = Node.accountAddress(publicKey);
    return { secret, publicKey, address };
  }
  static accountAddress(publicKey) {
    return consume(ensureLoaded().accountAddress(u8(publicKey), u8(publicKey).length), 'account_address');
  }

  credit(address, amount) {
    if (fn.credit(this._h, u8(address), BigInt(amount)) !== 0) throw new Error('credit failed');
  }
  submitTransfer(secret, toAddress, amount, nonce) {
    return consume(fn.submit(this._h, u8(secret), u8(secret).length, u8(toAddress), BigInt(amount), BigInt(nonce)), 'submit_transfer');
  }
  produceBlock(timestamp) {
    const out = [0n];
    if (fn.produce(this._h, BigInt(timestamp), out) !== 0) throw new Error('produce_block failed');
    return Number(out[0]);
  }
  height() { return Number(fn.height(this._h)); }
  mempoolSize() { return Number(fn.mempool(this._h)); }
  balance(address) { return Number(fn.balance(this._h, u8(address))); }
  nonce(address) { return Number(fn.nonce(this._h, u8(address))); }
  stateRoot() { return consume(fn.stateRoot(this._h), 'state_root'); }
  drainEvents() { return consume(fn.drainEvents(this._h), 'drain_events').toString('utf8'); }
  metrics() { return consume(fn.metrics(this._h), 'metrics').toString('utf8'); }
  proveAccount(address) { return consume(fn.proveAccount(this._h, u8(address)), 'prove_account'); }
}

// ── Public API ───────────────────────────────────────────────────────────────
module.exports = {
  preload,
  _warm,
  init() { if (fn.init() !== 0) throw new Error('blockchainer init failed'); },
  version: () => fn.version(),
  cryptoBackendVersion: () => fn.backend(),

  hash: (msg) => consume(fn.hash(u8(msg), u8(msg).length), 'hash'),
  verifyStateProof: (root, proof) => fn.verifyState(u8(root), u8(proof), u8(proof).length) === 1,
  verifyTxInclusion: (root, proof) => fn.verifyTx(u8(root), u8(proof), u8(proof).length) === 1,

  newNode: (chainId) => new Node(chainId),
  Node,
  accountAddress: Node.accountAddress,

  ethAddress: (secret32) => consume(fn.ethAddr(u8(secret32)), 'eth_address').toString('utf8'),
  btcAddress: (secret32) => consume(fn.btcAddr(u8(secret32)), 'btc_address').toString('utf8'),
  solAddress: (ed64) => consume(fn.solAddr(u8(ed64)), 'sol_address').toString('utf8'),
  erc20TransferCalldata: (to20, amount) => consume(fn.erc20(u8(to20), BigInt(amount)), 'erc20'),
  functionSelector: (signature) => consume(fn.selector(signature), 'selector'),
  signLegacyTx({ nonce = 0, gasPrice = 0, gasLimit = 0, to = null, value = 0, data = null, chainId = 1, secret }) {
    const t = to ? u8(to) : null;
    const d = data ? u8(data) : null;
    return consume(fn.signLegacy(BigInt(nonce), BigInt(gasPrice), BigInt(gasLimit),
      t, t ? 1 : 0, BigInt(value), d, d ? d.length : 0, BigInt(chainId), u8(secret)), 'sign_legacy');
  },
};
