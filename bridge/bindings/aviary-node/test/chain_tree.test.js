// ChainTree over the npm binding (mirrors the Go test).
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }
const val = (seed, power = 1) => { const v = bc.newBLSValidator(Buffer.alloc(32, seed)); return { publicKey: v.public, secret: v.secret, power }; };
const alice = Buffer.alloc(32, 0xa1);
const pos = new bc.Blueprint('pos', 'account', 'round-robin');
const root = { chainId: 1, blueprint: pos, validators: [val(0x01)], children: [
  { chainId: 42, validators: [val(0x10), val(0x11), val(0x12)], maxHeadersPerAnchor: 4, children: [
    { chainId: 420, validators: [val(0x20), val(0x21)], accounts: [{ address: alice, balance: 1000 }] }] },
  { chainId: 43, validators: [val(0x30)] }] };
throws(() => new bc.ChainTree({ ...root, children: [root.children[0], { chainId: 42, validators: [val(0x30)] }] }), 'duplicate id refused');
throws(() => new bc.ChainTree({ ...root, children: [{ chainId: 42, blueprint: pos, validators: [val(0x10)] }] }), 'PoS child refused');
throws(() => new bc.ChainTree({ ...root, blueprint: new bc.Blueprint('no-such-engine', 'account') }), 'unknown engine refused');
const tree = new bc.ChainTree(root);
assert(JSON.stringify(tree.chainIds()) === '[1,42,420,43]', 'pre-order ids');
const finalized = {}; tree.onFinalized((id) => { finalized[id] = (finalized[id] || 0) + 1; });
tree.produce(420, 2); tree.produce(420, 3);
assert(tree.pendingAnchors(42) === 1 && tree.pendingAnchors(1) === 0, 'pending anchors');
tree.produce(42, 2); tree.produce(43, 2);
const rootBlk = tree.produce(1, 2);
assert(tree.height(1) === 1 && tree.height(42) === 1 && tree.height(420) === 2 && tree.pendingAnchors(1) === 0, 'heights');
assert(finalized[420] === 2 && finalized[42] === 1 && finalized[1] === 1 && finalized[43] === 1, 'finalized hook from JS');
throws(() => tree.produce(7, 3), 'unknown chain');
const proof = tree.proveAccount(420, alice);
assert(tree.verify(proof) && bc.verifySubchainProof(bc.headerSubchainRoot(bc.blockHeader(rootBlk)), proof), 'grandchild proven from the root');
const bad = Buffer.from(proof); bad[bad.length - 1] ^= 1; assert(!tree.verify(bad), 'tampered proof fails');
assert(bc.genesisHeader(tree.genesis(42)).length > 0 && tree.tip(420).length > 0, 'genesis/tip');
let calls = 0; tree.txSource(() => { calls++; return []; }); tree.produce(420, 4); assert(calls === 1, 'tx source from JS');
tree.free();
console.log('chain-tree test: OK (root <- child <- grandchild built together, produced upward, proven from the root, hooks from JS)');
