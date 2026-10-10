// Push events through the native hook.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
const n = bc.newNode(0x7703);
const seen = [];
n.onEvent((j) => seen.push(JSON.parse(j)));
const a = n.newAccount(), b = n.newAccount();
n.credit(a.address, 10);
n.submitTransfer(a.secret, b.address, 1, 0);
n.produceBlock(1);
assert(seen.length === 3 && seen[0].type === 'tx_admitted' && seen[1].type === 'block_finalized' && seen[2].type === 'tx_removed', 'three pushed events in order');
assert(seen[1].block_id.length === 64 && seen[2].txid.length === 64, 'block id and txid carried');
assert(n.drainEvents() === '[]', 'buffer empty while hooked');
n.onEvent(null);
n.submitTransfer(a.secret, b.address, 1, 1);
assert(n.drainEvents().includes('tx_admitted'), 'buffering resumes after unhook');
n.free();
console.log('aviary npm events test: OK');
