// Validator rotation over the npm binding (mirrors the Go test).
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }
const me = bc.newBLSValidator(Buffer.alloc(32, 0x81)), other = bc.newBLSValidator(Buffer.alloc(32, 0x82));
const bft = new bc.Blueprint('bft', 'account');
const chain = bc.assembleChain(bc.commitBlueprint(bc.buildGenesis({ chainId: 5, validators: [{ publicKey: me.public, power: 1 }] }), bft), 0, me.public, me.secret);
assert(chain.validators().length === 1 && chain.validators()[0].publicKey.equals(me.public), 'initial set');
const grown = [{ publicKey: me.public, power: 3 }, { publicKey: other.public, power: 1 }];
chain.scheduleValidators(grown);
const b1 = chain.propose(2); assert(chain.submit(b1, chain.certify(b1)), 'rotation block');
const vs = chain.validators(); assert(vs.length === 2 && vs[0].power === 3 && vs[1].publicKey.equals(other.public), 'set after rotation');
const b2 = chain.propose(3); assert(chain.submit(b2, chain.certify(b2)) && chain.height === 2, 'block under the new set');
throws(() => chain.scheduleValidators([]), 'empty set refused');
throws(() => chain.scheduleValidators([{ publicKey: me.public, power: 0 }]), 'zero power refused');
chain.scheduleValidators([{ publicKey: other.public, power: 1 }]);
const b3 = chain.propose(4); assert(chain.submit(b3, chain.certify(b3)) && chain.height === 3, 'leaving block');
throws(() => chain.certify(chain.propose(5)), 'a node that left cannot certify');
const pc = bc.assembleChain(bc.commitBlueprint(bc.buildGenesis({ chainId: 6, validators: [{ publicKey: me.public, power: 1 }] }), new bc.Blueprint('pos', 'account', 'round-robin')), 0, me.public, me.secret);
throws(() => pc.scheduleValidators(grown), 'PoS refuses');
chain.free(); pc.free();
console.log('validator-rotation test: OK (grow, certify under each set, rotate out, PoS refuses)');
