// Subchains over the npm binding: a single-validator BFT child finalizes real
// certified blocks; a parent registers it at genesis, anchors the child's
// certified headers, and a balance on the child is proven from the parent's
// header alone. Every tamper is refused.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }

const cv = bc.newBLSValidator(Buffer.alloc(32, 0x71));
const pv = bc.newBLSValidator(Buffer.alloc(32, 0x72));
const alice = Buffer.alloc(32, 0xa1);
const bft = new bc.Blueprint('bft', 'account');

let childGen = bc.commitBlueprint(bc.buildGenesis({ chainId: 42, accounts: [{ address: alice, balance: 1000 }], validators: [{ publicKey: cv.public, power: 1 }] }), bft);
const childHeader = bc.genesisHeader(childGen);
let parentGen = bc.commitBlueprint(bc.buildGenesis({ chainId: 1, validators: [{ publicKey: pv.public, power: 1 }] }), bft);
const policy = bc.encodeSubchainPolicy({ maxChildren: 1, maxDepth: 1, maxHeadersPerAnchor: 4,
  children: [{ chainId: 42, genesisHeader: childHeader, validators: [{ publicKey: cv.public, power: 1 }] }] });
const plainHash = bc.genesisHash(parentGen);
parentGen = bc.commitSubchainPolicy(parentGen, policy);
assert(!plainHash.equals(bc.genesisHash(parentGen)), 'policy changes the parent genesis hash');
throws(() => bc.commitSubchainPolicy(parentGen, bc.encodeSubchainPolicy({ maxChildren: 1, children: [{ chainId: 1, genesisHeader: childHeader, validators: [{ publicKey: cv.public, power: 1 }] }] })), 'own-chain child refused');
const bound = bc.commitParentBinding(childGen, 1, bc.genesisHash(parentGen));
assert(!bc.genesisHash(bound).equals(bc.genesisHash(childGen)) && bc.genesisHeader(bound).equals(childHeader), 'binding changes the hash, not the header');

const child = bc.assembleChain(bound, 0, cv.public, cv.secret);
const parent = bc.assembleChain(parentGen, 0, pv.public, pv.secret);
assert(child && parent, 'both assemble');
const certified = [];
for (let ts = 2; ts <= 3; ts++) {
  const blk = child.propose(ts);
  const cert = child.certify(blk);
  assert(child.submit(blk, cert), 'child submit');
  certified.push(bc.certifiedHeader(bc.blockHeader(blk), cert));
}
assert(child.height === 2, 'child height');
assert(parent.childTip(42).length > 0, 'registered child has a tip');
throws(() => parent.childTip(99), 'unknown child');
throws(() => child.childTip(42), 'non-parent has no children');

const anchor = bc.encodeAnchor(42, certified);
const pblk = parent.proposeAnchored(2, [anchor]);
assert(parent.submit(pblk, parent.certify(pblk)) && parent.height === 1, 'parent anchored');
const tampered = Buffer.from(certified[1]); tampered[tampered.length - 1] ^= 1;
throws(() => parent.proposeAnchored(3, [bc.encodeAnchor(42, [tampered])]), 'tampered anchor refused');
throws(() => parent.proposeAnchored(3, [anchor]), 'stale anchor refused');

const root = bc.headerSubchainRoot(bc.blockHeader(pblk));
const proof = bc.encodeSubchainProof([parent.subchainHop(42)], child.proveAccount(alice));
assert(bc.verifySubchainProof(root, proof), 'cross-chain proof verifies');
assert(!bc.verifySubchainProof(Buffer.alloc(32, 1), proof), 'wrong root fails');
const bad = Buffer.from(proof); bad[bad.length - 1] ^= 1;
assert(!bc.verifySubchainProof(root, bad), 'tampered proof fails');
assert(!bc.verifySubchainProof(root, bc.encodeSubchainProof([], child.proveAccount(alice))), 'no hops fails');

const blk3 = child.propose(4); const cert3 = child.certify(blk3); child.submit(blk3, cert3);
const pblk2 = parent.proposeAnchored(3, [bc.encodeAnchor(42, [bc.certifiedHeader(bc.blockHeader(blk3), cert3)])]);
assert(parent.submit(pblk2, parent.certify(pblk2)), 'parent block 2');
const root2 = bc.headerSubchainRoot(bc.blockHeader(pblk2));
assert(!bc.verifySubchainProof(root2, proof) && bc.verifySubchainProof(root, proof), 'old proof stale at new root, valid at old');
assert(bc.verifySubchainProof(root2, bc.encodeSubchainProof([parent.subchainHop(42)], child.proveAccount(alice))), 'fresh proof at new root');
child.free(); parent.free();
console.log('subchain test: OK (BFT child anchored by a parent; cross-chain proof from the parent header)');
