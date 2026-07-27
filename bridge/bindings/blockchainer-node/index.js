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
    submitFee: f('BlockchainerBufferResult blockchainer_node_submit_transfer_fee(void*, uint8_t*, size_t, uint8_t*, uint64_t, uint64_t, uint64_t)'),
    ksNew: f('void *blockchainer_keystore_new(const char*)'),
    ksAdd: f('int blockchainer_keystore_add_account(void*, uint8_t*, size_t, uint8_t*, size_t, uint8_t*)'),
    ksReveal: f('BlockchainerBufferResult blockchainer_keystore_reveal(void*, uint8_t*)'),
    ksSerialise: f('BlockchainerBufferResult blockchainer_keystore_serialise(void*)'),
    ksLoad: f('void *blockchainer_keystore_load(uint8_t*, size_t)'),
    ksUnlock: f('int blockchainer_keystore_unlock(void*, const char*)'),
    ksUnlocked: f('int blockchainer_keystore_unlocked(void*)'),
    ksFree: f('void blockchainer_keystore_free(void*)'),
    kemKeygen: f('BlockchainerBufferResult blockchainer_hybrid_kem_keygen()'),
    pSealPass: f('BlockchainerBufferResult blockchainer_portable_seal_passphrase(uint8_t*, size_t, uint8_t*, size_t, const char*, uint64_t, size_t)'),
    pOpenPass: f('BlockchainerBufferResult blockchainer_portable_open_passphrase(uint8_t*, size_t, const char*)'),
    pSealTo: f('BlockchainerBufferResult blockchainer_portable_seal_to(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    pOpenTo: f('BlockchainerBufferResult blockchainer_portable_open_to(uint8_t*, size_t, uint8_t*, size_t)'),
    pSealSigned: f('BlockchainerBufferResult blockchainer_portable_seal_signed_to(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    pOpenSigned: f('BlockchainerBufferResult blockchainer_portable_open_signed_from(uint8_t*, size_t, uint8_t*, size_t, uint8_t*, size_t)'),
    newAccountHybrid: f('BlockchainerBufferResult blockchainer_node_new_account_hybrid(void*)'),
    submitHybrid: f('BlockchainerBufferResult blockchainer_node_submit_transfer_hybrid(void*, uint8_t*, size_t, uint8_t*, size_t, uint8_t*, uint64_t, uint64_t)'),
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
// Unpack the C ABI account blob [pub_len:4 BE][public][secret].
const splitBlob = (blob) => {
  const pl = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
  return { public: blob.subarray(4, 4 + pl), secret: blob.subarray(4 + pl) };
};
const bytes = (cb) => (!cb.data || Number(cb.len) === 0)
  ? Buffer.alloc(0) : Buffer.from(koffi.decode(cb.data, 'uint8_t', Number(cb.len)));
function consume(r, op) {
  if ((!r.buf.data || Number(r.buf.len) === 0) && r.error) {
    // Read the NUL-terminated C string the error pointer holds (koffi: length -1
    // decodes to the terminator). Decoding it as 'char *' instead reads a pointer
    // AT the address — a double-indirection that segfaults on the error path.
    const m = koffi.decode(r.error, 'char', -1); fn.strFree(r.error); throw new Error(m);
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

  // Valid until the first block is produced (reads do NOT seal genesis);
  // afterwards throws — never a silent no-op.
  credit(address, amount) {
    const rc = fn.credit(this._h, u8(address), BigInt(amount));
    if (rc === 2) throw new Error('genesis is sealed (a block was produced); fund accounts with transfers');
    if (rc !== 0) throw new Error(`credit failed (status ${rc})`);
  }
  submitTransfer(secret, toAddress, amount, nonce) {
    return consume(fn.submit(this._h, u8(secret), u8(secret).length, u8(toAddress), BigInt(amount), BigInt(nonce)), 'submit_transfer');
  }
  // Ed25519 transfer carrying a priority-fee tip (version-2 envelope) for fee-market priority.
  submitTransferFee(secret, toAddress, amount, nonce, fee) {
    return consume(fn.submitFee(this._h, u8(secret), u8(secret).length, u8(toAddress), BigInt(amount), BigInt(nonce), BigInt(fee)), 'submit_transfer_fee');
  }
  // Hybrid (Ed25519 + ML-DSA-65) post-quantum account. Returns {secret, publicKey, address}.
  newAccountHybrid() {
    const blob = consume(fn.newAccountHybrid(this._h), 'new_account_hybrid');
    // [pub_len: 4 bytes big-endian][public][secret]
    const pl = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
    const publicKey = blob.subarray(4, 4 + pl);
    const secret = blob.subarray(4 + pl);
    return { secret, publicKey, address: Node.accountAddress(publicKey) };
  }
  submitTransferHybrid(secret, publicKey, toAddress, amount, nonce) {
    return consume(fn.submitHybrid(this._h, u8(secret), u8(secret).length,
      u8(publicKey), u8(publicKey).length, u8(toAddress), BigInt(amount), BigInt(nonce)), 'submit_transfer_hybrid');
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

// A custodial store that seals account signing secrets at rest.
class KeyStore {
  constructor(passphrase) {
    this._h = fn.ksNew(passphrase);
    if (!this._h) throw new Error('keystore create failed');
  }
  static _wrap(h) { const k = Object.create(KeyStore.prototype); k._h = h; return k; }
  static load(blob) {
    const b = u8(blob);
    const h = fn.ksLoad(b, b.length);
    if (!h) throw new Error('keystore load failed');
    return KeyStore._wrap(h);  // LOCKED — call unlock()
  }
  free() { if (this._h) { fn.ksFree(this._h); this._h = null; } }
  addAccount(publicKey, secret) {
    const pk = u8(publicKey), sk = u8(secret), out = Buffer.alloc(32);
    if (fn.ksAdd(this._h, pk, pk.length, sk, sk.length, out) !== 0) {
      throw new Error('keystore add account failed (locked?)');
    }
    return out;
  }
  reveal(address) { return consume(fn.ksReveal(this._h, u8(address)), 'keystore_reveal'); }
  serialise() { return consume(fn.ksSerialise(this._h), 'keystore_serialise'); }
  unlock(passphrase) { if (fn.ksUnlock(this._h, passphrase) !== 0) throw new Error('keystore unlock failed'); }
  get unlocked() { return fn.ksUnlocked(this._h) === 1; }
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

  newKeyStore: (passphrase) => new KeyStore(passphrase),
  loadKeyStore: (blob) => KeyStore.load(blob),
  KeyStore,

  // Portable account export (tiered encrypted backup). Returns/takes accounts as
  // { public, secret } Buffers; envelopes are Buffers.
  hybridKemKeypair: () => splitBlob(consume(fn.kemKeygen(), 'hybrid_kem_keygen')),
  sealAccountPassphrase: (publicKey, secret, passphrase, ops = 0, mem = 0) =>
    consume(fn.pSealPass(u8(publicKey), u8(publicKey).length, u8(secret), u8(secret).length,
      passphrase, BigInt(ops), mem), 'portable_seal_passphrase'),
  openAccountPassphrase: (envelope, passphrase) =>
    splitBlob(consume(fn.pOpenPass(u8(envelope), u8(envelope).length, passphrase), 'portable_open_passphrase')),
  sealAccountTo: (publicKey, secret, recipientKemPublic) =>
    consume(fn.pSealTo(u8(publicKey), u8(publicKey).length, u8(secret), u8(secret).length,
      u8(recipientKemPublic), u8(recipientKemPublic).length), 'portable_seal_to'),
  openAccountTo: (envelope, recipientKemSecret) =>
    splitBlob(consume(fn.pOpenTo(u8(envelope), u8(envelope).length,
      u8(recipientKemSecret), u8(recipientKemSecret).length), 'portable_open_to')),
  sealAccountSignedTo: (publicKey, secret, recipientKemPublic, senderSigSecret) =>
    consume(fn.pSealSigned(u8(publicKey), u8(publicKey).length, u8(secret), u8(secret).length,
      u8(recipientKemPublic), u8(recipientKemPublic).length,
      u8(senderSigSecret), u8(senderSigSecret).length), 'portable_seal_signed_to'),
  openAccountSignedFrom: (envelope, recipientKemSecret, senderSigPublic) =>
    splitBlob(consume(fn.pOpenSigned(u8(envelope), u8(envelope).length,
      u8(recipientKemSecret), u8(recipientKemSecret).length,
      u8(senderSigPublic), u8(senderSigPublic).length), 'portable_open_signed_from')),

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
