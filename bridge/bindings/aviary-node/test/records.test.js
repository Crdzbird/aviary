// Application records on a record-model chain: signed writes, ownership, proofs.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
const v = bc.newBLSValidator(Buffer.alloc(32, 0x70));
const genesis = bc.commitBlueprint(bc.buildGenesis({ chainId: 31, stateModel: 2, validators: [{ publicKey: v.public, power: 1 }] }), new bc.Blueprint('bft', 'record'));
const chain = bc.assembleChain(genesis, 0, v.public, v.secret);
assert(chain && chain.chainId === 31, 'record chain assembles');
const alice = bc.newNode(1).newAccount(), bob = bc.newNode(2).newAccount();
const key = bc.hash(Buffer.from('asset:PUMP-0042'));
chain.write(alice.secret, key, bc.RecordOp.PUT, Buffer.from('{"status":"in service"}'));
chain.write(alice.secret, key, bc.RecordOp.PUT, Buffer.from('{"status":"serviced"}'));
assert(chain.pendingCount === 2 && chain.record(key) === null, 'queued, not committed');
let refused = false; try { chain.write(bob.secret, key, bc.RecordOp.PUT, Buffer.from('x')); } catch (e) { refused = /not the record owner/.test(e.message); }
assert(refused, 'non-owner refused with the reason');
let blk = chain.propose(2); assert(chain.submit(blk, chain.certify(blk)) && chain.pendingCount === 0, 'block drains the queue');
const rec = chain.record(key);
assert(rec.version === 2 && rec.owner.equals(alice.address) && rec.value.toString() === '{"status":"serviced"}', 'record committed');
const proof = chain.proveRecord(key);
assert(bc.verifyStateProof(chain.stateRoot(), proof), 'proof verifies');
const leaf = bc.recordFromProof(proof);
assert(leaf.key.equals(key) && leaf.version === 2 && leaf.value.equals(rec.value), 'proof carries the record');
chain.write(alice.secret, key, bc.RecordOp.TRANSFER, bob.address);
blk = chain.propose(3); chain.submit(blk, chain.certify(blk));
assert(chain.record(key).owner.equals(bob.address), 'ownership moved');
chain.write(bob.secret, key, bc.RecordOp.DELETE);
blk = chain.propose(4); chain.submit(blk, chain.certify(blk));
assert(chain.record(key) === null, 'deleted');
chain.free();
console.log('aviary npm records test: OK');
