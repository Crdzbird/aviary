// Snapshot and restore over the npm binding.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }
const n = bc.newNode(0x7701);
const a = n.newAccount(), b = n.newAccount();
n.credit(a.address, 500);
n.submitTransfer(a.secret, b.address, 120, 0); n.produceBlock(1);
n.submitTransfer(a.secret, b.address, 30, 1); n.produceBlock(2);
const back = bc.restoreNode(n.snapshot());
assert(back.height() === 2 && back.balance(b.address) === 150 && back.nonce(a.address) === 2, 'state restored');
assert(back.stateRoot().equals(n.stateRoot()), 'state roots equal');
assert(back.blockAt(1).equals(n.blockAt(1)), 'archive restored');
back.submitTransfer(a.secret, b.address, 1, 2);
assert(back.produceBlock(3) === 3, 'restored node keeps producing');
throws(() => bc.restoreNode(Buffer.from([1, 2, 3])), 'garbage refused');
throws(() => n.blockAt(9), 'missing block throws');
n.free(); back.free();
console.log('aviary npm persistence test: OK');
