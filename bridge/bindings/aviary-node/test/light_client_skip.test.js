// Light-client overlap rule over the npm binding (mirrors the Go test).
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }
const va = bc.newBLSValidator(Buffer.alloc(32, 0x91)), vb = bc.newBLSValidator(Buffer.alloc(32, 0x92));
const bft = new bc.Blueprint('bft', 'account');
const genesisSet = [{ publicKey: va.public, power: 1 }];
const gen = bc.commitBlueprint(bc.buildGenesis({ chainId: 8, validators: genesisSet }), bft);
const gh = bc.genesisHeader(gen);
const A = bc.assembleChain(gen, 0, va.public, va.secret), B = bc.assembleChain(gen, 0, vb.public, vb.secret);
const cps = {};
A.scheduleValidators([{ publicKey: va.public, power: 1 }, { publicKey: vb.public, power: 3 }]);
const b1 = A.propose(2), c1 = A.certify(b1); assert(A.submit(b1, c1) && B.submit(b1, c1), 'rotation block');
cps[1] = { header: bc.blockHeader(b1), certificate: c1, validators: genesisSet };
const newSet = B.validators();
for (let ts = 3; ts <= 4; ts++) { const blk = B.propose(ts), cert = B.certify(blk); assert(B.submit(blk, cert) && A.submit(blk, cert), 'B block'); cps[ts - 1] = { header: bc.blockHeader(blk), certificate: cert, validators: newSet }; }
const lc = new bc.LightClient(8, genesisSet, gh);
assert(lc.acceptSkipping(cps[3]) === bc.SkipInsufficientOverlap, 'no-overlap skip reports thin overlap');
assert(lc.acceptSkipping({ ...cps[3], validators: genesisSet }) === bc.SkipRefused, 'wrong set refused');
const badCert = Buffer.from(cps[3].certificate); badCert[badCert.length - 1] ^= 1;
assert(lc.acceptSkipping({ ...cps[3], certificate: badCert }) === bc.SkipRefused && lc.height === 0, 'tampered cert refused');
let fetched = 0; lc.syncTo(cps[3], 3, (h) => { fetched++; return cps[h] || null; });
assert(lc.height === 3 && fetched > 0, 'bisection synced to 3');
assert(lc.validators().length === 2 && lc.validators()[1].power === 3, 'client trusts the new set');
const b4 = B.propose(5), c4 = B.certify(b4); B.submit(b4, c4);
assert(lc.accept(bc.blockHeader(b4), c4) && lc.height === 4, 'sequential accept after a sync');
const lc2 = new bc.LightClient(8, genesisSet, gh); lc2.setMaxSkip(1);
assert(lc2.acceptSkipping(cps[2]) === bc.SkipRefused, 'max_skip refuses a 2-block jump');
lc2.setMaxSkip(0); throws(() => lc2.syncTo(cps[3], 3, () => null), 'unavailable checkpoint fails');
throws(() => new bc.LightClient(8, [], gh), 'empty set refused');
lc.free(); lc2.free(); A.free(); B.free();
console.log('light-client-skip test: OK (thin overlap -> bisection through the rotation -> follow the new set)');
