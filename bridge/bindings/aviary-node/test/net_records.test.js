const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
const keys = [0, 1, 2, 3].map((i) => bc.newBLSValidator(Buffer.alloc(32, 0x50 + i)));
const validators = keys.map((k) => ({ publicKey: k.public, power: 1 }));
const genesis = bc.buildGenesis({ chainId: 77, accounts: [], validators, stateModel: 2 });
const mesh = new bc.PeerMesh(keys.map((k, i) => bc.newNetNode({ genesis, me: i, blsKey: k })));
mesh.nodes.push(bc.newNetNode({ genesis, me: bc.NET_OBSERVER }));
mesh.linkAll();
const nodes = mesh.nodes, observer = nodes[4];
const author = bc.newNode(1).newAccount(), heir = bc.newNode(2).newAccount();
const key = Buffer.alloc(32, 0x42);
const give = (nonce, op, value) => {
  const tx = bc.signRecordTx({ chainId: 77, nonce, key, op, value, secret: author.secret });
  for (const n of nodes.slice(0, 4)) n.setTransactions([tx]);
};
give(0, bc.RecordOp.PUT, Buffer.from('v1'));
mesh.start();
assert(mesh.settle([0, 1, 2, 3]), 'height 1 commits');
for (const n of nodes) {
  const rec = n.record(key);
  assert(rec && rec.version === 1 && rec.owner.equals(author.address), 'every node holds version 1');
}
assert(observer.recordNonce(author.address) === 1, 'observer nonce');
const proof = nodes[1].proveRecord(key);
assert(bc.verifyStateProof(observer.stateRoot(), proof), 'validator proof under the observer root');
assert(bc.recordFromProof(proof).value.toString() === 'v1', 'proof carries the value');
assert(nodes[2].record(Buffer.alloc(32)) === null, 'unknown key reads null');
give(1, bc.RecordOp.TRANSFER, heir.address);
mesh.advance();
assert(mesh.settle([0, 1, 2, 3]), 'height 2 commits');
assert(nodes.every((n) => n.record(key).owner.equals(heir.address)), 'a list set after the commit lands in the next height');
mesh.free();
const account = bc.newNetNode({ genesis: bc.buildGenesis({ chainId: 78, accounts: [], validators: validators.slice(0, 1) }), me: 0, blsKey: keys[0] });
let refused = false;
try { account.record(key); } catch (e) { refused = /not a record-model chain/.test(e.message); }
assert(refused, 'an account network refuses record queries');
account.free();
console.log('aviary node networked records: OK');
