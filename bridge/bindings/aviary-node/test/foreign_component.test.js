// Foreign components over the npm binding: a consensus engine and a proposer
// policy implemented IN JAVASCRIPT join a chain mixture by id; blocks commit
// through JS's justification policy; a tampered justification is refused by the
// JS verifier with the ledger untouched.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }

const calls = { justify: 0, verify: 0 };
const tag = (block) => Buffer.concat([Buffer.from('js-just:'), block.subarray(0, 16)]);
const jsEngine = {
  justify(block) { calls.justify++; return tag(block); },
  verify(block, justification) { calls.verify++; return justification.equals(tag(block)); },
};
assert(bc.registerEngine('js-engine', jsEngine), 'register JS engine');
assert(bc.registerProposer('js-first', () => 0), 'register JS proposer');
assert(bc.engineIds().includes('js-engine') && bc.proposerIds().includes('js-first'), 'discoverable');

const v = bc.newBLSValidator(Buffer.alloc(32, 0x60));
const genesis = bc.buildGenesis({ chainId: 7, assuranceLevel: 1,
  accounts: [{ address: Buffer.alloc(32, 0x11), balance: 1000 }], validators: [{ publicKey: v.public, power: 1 }] });

const chain = bc.assembleChain(bc.commitBlueprint(genesis, new bc.Blueprint('js-engine', 'account')), 0, v.public, v.secret);
assert(chain, 'JS-engine chain assembles');
const blk = chain.propose(2);
const just = chain.seal(blk); // → JS justify
assert(just.subarray(0, 8).toString() === 'js-just:', 'seal via JS justify');
const bad = Buffer.from(just); bad[bad.length - 1] ^= 1;
assert(!chain.submit(blk, bad) && chain.height === 0, 'tampered justification refused, ledger untouched');
assert(chain.submit(blk, just) && chain.height === 1, 'JS-justified block committed');
assert(calls.justify >= 1 && calls.verify >= 2, 'callbacks crossed the boundary');
chain.free();

const pc = bc.assembleChain(bc.commitBlueprint(genesis, new bc.Blueprint('pos', 'account', 'js-first')), 0, v.public, v.secret);
assert(pc, 'PoS with JS proposer assembles');
const pb = pc.propose(2);
assert(pc.submit(pb, pc.seal(pb)) && pc.height === 1, 'PoS with a JS proposer committed');
pc.free();

// A throwing JS verifier is a failed verification, never a crash.
assert(bc.registerEngine('js-throws', { justify: (b) => tag(b), verify: () => { throw new Error('nope'); } }), 'register throwing engine');
const tc = bc.assembleChain(bc.commitBlueprint(genesis, new bc.Blueprint('js-throws', 'account')), 0, v.public, v.secret);
const tb = tc.propose(2);
assert(!tc.submit(tb, tc.seal(tb)) && tc.height === 0, 'throwing verifier → refused, ledger untouched');
tc.free();
console.log('foreign-component test: OK (JS engine + proposer in the mixture)');
