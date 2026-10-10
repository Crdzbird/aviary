// Key store backed signing: the secret never leaves the engine.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }
const n = bc.newNode(0x7702);
const a = n.newAccount(), b = n.newAccount();
const ks = bc.newKeyStore('hunter2');
const stored = ks.addAccount(a.publicKey, a.secret);
assert(stored.equals(a.address) && ks.has(a.address) && !ks.has(b.address), 'inventory');
assert(ks.publicKey(a.address).equals(a.publicKey) && ks.addresses().length === 1, 'public key and list');
n.credit(a.address, 1000);
n.submitTransferFromStore(ks, a.address, b.address, 100, 0);
n.submitTx(ks.signTransfer({ chainId: 0x7702, from: a.address, to: b.address, amount: 1, nonce: 1, fee: 3 }));
n.produceBlock(1);
assert(n.balance(b.address) === 101 && n.balance(a.address) === 1000 - 100 - 1 - 3, 'custody transfers applied');
ks.lock(); assert(!ks.unlocked, 'locked');
throws(() => n.submitTransferFromStore(ks, a.address, b.address, 1, 2), 'locked store cannot sign');
ks.unlock('hunter2');
throws(() => ks.signTransfer({ chainId: 0x7702, from: b.address, to: a.address, amount: 1, nonce: 0 }), 'unknown account');
const reloaded = bc.loadKeyStore(ks.serialise()); reloaded.unlock('hunter2');
n.submitTransferFromStore(reloaded, a.address, b.address, 2, 2); n.produceBlock(2);
assert(n.balance(b.address) === 103, 'reloaded store signs');
ks.free(); reloaded.free(); n.free();
console.log('aviary npm custody test: OK');
