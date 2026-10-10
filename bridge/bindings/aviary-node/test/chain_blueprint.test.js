// Composable chains over the npm binding: discover components, declare a
// mixture, commit it into genesis (identity changes), assemble, commit blocks via
// PoS and PoW, and confirm the fail-closed rejects.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
const has = (xs, s) => xs.includes(s);

const eng = bc.engineIds();
assert(has(eng, 'bft') && has(eng, 'pow') && has(eng, 'pos') && has(eng, 'vrf-pos'), 'built-in engines listed');
assert(has(bc.stateModelIds(), 'account') && has(bc.stateModelIds(), 'utxo'), 'state models listed');
assert(has(bc.proposerIds(), 'stake-weighted') && has(bc.proposerIds(), 'round-robin'), 'proposers listed');

const v = bc.newBLSValidator(Buffer.alloc(32, 0x60));
assert(v.public.length === 48 && v.secret.length === 32, 'BLS validator key sizes');
const accounts = [{ address: Buffer.alloc(32, 0x11), balance: 1000 }];
const validators = [{ publicKey: v.public, power: 1 }];
const genesis = bc.buildGenesis({ chainId: 7, assuranceLevel: 1, accounts, validators });

const pos = new bc.Blueprint('pos', 'account', 'round-robin');
assert(pos.hash().length === 32, 'blueprint hash');
const mixed = bc.commitBlueprint(genesis, pos);
assert(!bc.genesisHash(genesis).equals(bc.genesisHash(mixed)), 'the mixture changes the genesis hash');

const chain = bc.assembleChain(mixed, 0, v.public, v.secret);
assert(chain, 'PoS chain assembles');
assert(chain.blueprint().equals(pos.encoded), 'chain reports the committed mixture');
const blk = chain.propose(2);
const just = chain.seal(blk);
assert(just.length > 0 && chain.submit(blk, just) && chain.height === 1, 'PoS block committed');
chain.free();

const pow = new bc.Blueprint('pow', 'account', '', Buffer.alloc(32, 0xff));
const pc = bc.assembleChain(bc.commitBlueprint(genesis, pow), 0, v.public, v.secret);
assert(pc, 'PoW chain assembles');
const cand = pc.propose(2);
assert(pc.seal(cand).length === 0, 'PoW does not seal');
assert(pc.submit(pc.mine(cand)) && pc.height === 1, 'PoW block committed');
pc.free();

assert(bc.assembleChain(bc.commitBlueprint(genesis, new bc.Blueprint('no-such-engine', 'account')), 0, v.public, v.secret) === null, 'unknown engine rejected');
const hardened = bc.buildGenesis({ chainId: 7, assuranceLevel: 2, accounts, validators });
assert(bc.assembleChain(bc.commitBlueprint(hardened, pos), 0, v.public, v.secret) === null, 'hardened posture rejects non-grind-proof engine');
assert(bc.assembleChain(bc.commitBlueprint(genesis, new bc.Blueprint('pos', 'utxo')), 0, v.public, v.secret) === null, 'state mismatch rejected');
console.log('aviary npm chain-blueprint test: OK');
