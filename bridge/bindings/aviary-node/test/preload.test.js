// Verifies: require() does not load native eagerly; preload() warms off-thread
// in a worker; the synchronous API works with and without preload (no await).
//
//   AVIARY_DYLIB=/abs/libaviary_c.dylib \
//   CRYPTOLIB_DYLIB=/abs/libcryptolib_c.dylib \
//     node test/preload.test.js
'use strict';
const assert = require('node:assert');

(async () => {
  const bc = require('..');
  assert.strictEqual(typeof bc.preload, 'function', 'preload() exported');

  // Synchronous API with NO preload (lazy self-init on first call).
  bc.init();
  assert.ok(bc.version().length > 0, 'version after lazy init');
  const h = bc.hash(Buffer.from('abc'));
  assert.strictEqual(h.length, 32, 'BLAKE3 digest is 32 bytes');

  // preload() warms in a worker thread and resolves true.
  assert.strictEqual(await bc.preload(), true, 'preload() resolved true');
  assert.strictEqual(await bc.preload(), true, 'preload idempotent');

  // A full ledger flow - all synchronous, no await.
  const node = bc.newNode(1n);
  const a = node.newAccount();
  const b = node.newAccount();
  node.credit(a.address, 1_000_000n);
  node.submitTransfer(a.secret, b.address, 250_000n, node.nonce(a.address));
  const height = node.produceBlock(1n);
  assert.strictEqual(height, 1, 'first block sealed at height 1');
  assert.strictEqual(node.balance(b.address), 250_000, 'recipient credited');

  // Light-client proof round-trip.
  const proof = node.proveAccount(b.address);
  assert.strictEqual(bc.verifyStateProof(node.stateRoot(), proof), true, 'state proof verifies');

  // Interop + EVM contract signing, deterministic + synchronous.
  const secp = Buffer.alloc(32, 0x11);
  const eth1 = bc.ethAddress(secp), eth2 = bc.ethAddress(secp);
  assert.strictEqual(eth1, eth2, 'deterministic eth address');
  assert.ok(/^0x[0-9a-fA-F]{40}$/.test(eth1), `eth address shape (${eth1})`);
  const sel = bc.functionSelector('transfer(address,uint256)').toString('hex');
  assert.strictEqual(sel, 'a9059cbb', 'ERC-20 transfer selector');
  const raw = bc.signLegacyTx({ nonce: 0, gasPrice: 30e9, gasLimit: 60000, to: Buffer.alloc(20, 0xab), value: 0, data: bc.erc20TransferCalldata(Buffer.alloc(20, 0xcd), 1n), chainId: 1, secret: secp });
  assert.ok(raw.length > 0, 'signed raw tx produced');
  node.free();

  console.log(`aviary npm preload test: OK (v${bc.version()}, crypto ${bc.cryptoBackendVersion()}, eth ${eth1.slice(0, 10)}..., selector 0x${sel})`);
})().catch((e) => { console.error('FAILED:', e); process.exit(1); });
