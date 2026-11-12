// Subchains version 2 over the npm binding: the golden vectors of
// vectors/subchain_v2_vectors.txt replayed call for call, then signed
// registry operations, restart from snapshots and every rejection.
const fs = require('node:fs');
const path = require('node:path');
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throwsWith(fn, text, m) {
  let msg = null; try { fn(); } catch (e) { msg = String(e.message); }
  assert(msg !== null && (!text || msg.includes(text)), m + ' (got ' + msg + ')');
}

const bftRecord = new bc.Blueprint('bft', 'record');
const recordGenesis = (chainId, v) =>
  bc.commitBlueprint(bc.buildGenesis({ chainId, stateModel: 2, validators: [{ publicKey: v.public, power: 1 }] }), bftRecord);
function assemble(genesis, v) {
  const c = bc.assembleChain(genesis, 0, v.public, v.secret);
  assert(c, 'chain assembles'); c.enableArchive(); return c;
}
function commit(c, block) { assert(c.submit(block, c.certify(block)), 'block accepted'); return block; }
const registration = (chainId, genesis, v) =>
  bc.encodeChildRegistration({ chainId, genesisHeader: bc.genesisHeader(genesis), validators: [{ publicKey: v.public, power: 1 }] });
const singleRegistrar = (secret) => bc.encodeDynamicSubchainPolicy({
  maxDepth: 1, threshold: 1, registrars: [{ sigAlgo: bc.SigAlgo.ED25519, publicKey: secret.subarray(32) }],
});

function vectors() {
  const file = path.join(__dirname, '..', '..', '..', '..', 'vectors', 'subchain_v2_vectors.txt');
  if (!fs.existsSync(file)) return null;
  const out = new Map();
  for (const raw of fs.readFileSync(file, 'utf8').split('\n')) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const parts = line.split(/\s+/);
    assert(parts.length === 2, 'vector line ' + line);
    out.set(parts[0], parts[1]);
  }
  return out;
}

function replayVectors(want) {
  let seen = 0;
  const check = (name, got) => { seen++; assert(want.get(name) === Buffer.from(got).toString('hex'), 'vector ' + name); };
  const secret = Buffer.from(want.get('registrar_secret'), 'hex');
  const sign = (op, id, hash) => bc.signOp(op, id, hash, 0, bc.SigAlgo.ED25519, secret);
  const server = bc.newBLSValidator(Buffer.alloc(32, 0x01));
  const user = bc.newBLSValidator(Buffer.alloc(32, 0x10));
  check('registrar_secret', secret);

  const policy = singleRegistrar(secret);
  check('policy', policy);
  const mainGenesis = bc.commitSubchainPolicy(recordGenesis(1, server), policy);
  const mainHash = bc.genesisHash(mainGenesis);
  check('main_genesis_hash', mainHash);
  check('main_genesis_header', bc.genesisHeader(mainGenesis));

  const userGenesis = bc.commitParentBinding(recordGenesis(1001, user), 1, mainHash);
  const reg = registration(1001, userGenesis, user);
  check('registration', reg);
  const opReg = bc.opRegister(0, 0, reg);
  check('op_register', opReg);
  check('op_register_digest', bc.opDigest(opReg, 1, mainHash));
  const opRegSigned = sign(opReg, 1, mainHash);
  check('op_register_signed', opRegSigned);

  const main = assemble(mainGenesis, server);
  const userChain = assemble(userGenesis, user);
  const header1 = bc.blockHeader(commit(main, main.proposeSubchain(10, [], [opRegSigned])));
  check('header_1', header1);
  check('header_1_hash', bc.encodedHeaderHash(header1));
  check('subchain_root_1', bc.headerSubchainRoot(header1));
  check('child_record_1', main.childRecord(1001).encoded);

  const key = Buffer.alloc(32, 0x42);
  userChain.addTx(bc.signRecordTx({ chainId: 1001, nonce: 0, key, op: bc.RecordOp.PUT, value: Buffer.from('door opened'), secret }));
  commit(userChain, userChain.propose(20));
  commit(userChain, userChain.propose(21));
  const anchor = bc.pendingAnchor(main, userChain);
  check('anchor', anchor);
  const header2 = bc.blockHeader(commit(main, main.proposeSubchain(11, [anchor], [])));
  check('header_2_hash', bc.encodedHeaderHash(header2));
  const root2 = bc.headerSubchainRoot(header2);
  check('subchain_root_2', root2);
  const record2 = main.childRecord(1001);
  check('child_record_2', record2.encoded);
  const proof2 = main.childProof(1001);
  check('child_proof_2', proof2);
  const proven = bc.verifyChildProof(root2, proof2);
  check('child_proof_2_verified', proven.encoded);
  assert(proven.tipHeight === 2 && proven.status === bc.ChildStatus.OPEN && proven.tipHash.equals(bc.encodedHeaderHash(userChain.tip())), 'proven record');
  const recordProof = userChain.proveRecord(key);
  assert(bc.verifyStateProof(proven.tipStateRoot, recordProof), 'record proof under the proven tip');
  const hop = main.subchainHopV2(1001);
  assert(hop[0] === bc.HopRecord, 'a dynamic parent gives a record hop');
  const pathProof = bc.encodeSubchainProofV2([hop], recordProof);
  check('path_proof_2', pathProof);
  assert(bc.verifySubchainProofV2(root2, pathProof) && !bc.verifySubchainProofV2(bc.headerSubchainRoot(header1), pathProof), 'path proof');
  check('main_snapshot_hash_2', bc.hash(main.snapshot()));
  check('user_snapshot_hash_2', bc.hash(userChain.snapshot()));

  const opCl = bc.opClose(0, 0, 1001, record2.tipHeight, record2.tipHash);
  check('op_close', opCl);
  check('op_close_digest', bc.opDigest(opCl, 1, mainHash));
  const header3 = bc.blockHeader(commit(main, main.proposeSubchain(12, [], [sign(opCl, 1, mainHash)])));
  check('header_3_hash', bc.encodedHeaderHash(header3));
  check('subchain_root_3', bc.headerSubchainRoot(header3));
  const record3 = main.childRecord(1001);
  check('child_record_3', record3.encoded);
  assert(record3.status === bc.ChildStatus.CLOSED && record3.closedHeight === 3, 'closed record');
  check('main_snapshot_hash_3', bc.hash(main.snapshot()));

  const wideGenesis = bc.commitSubchainPolicy(recordGenesis(2, server), policy);
  const wideHash = bc.genesisHash(wideGenesis);
  const wide = assemble(wideGenesis, server);
  const ops = [];
  for (let id = 2000; id < 2011; id++) ops.push(sign(bc.opRegister(0, 0, registration(id, recordGenesis(id, user), user)), 2, wideHash));
  const wideRoot = bc.headerSubchainRoot(bc.blockHeader(commit(wide, wide.proposeSubchain(10, [], ops))));
  check('wide_root', wideRoot);
  const wideProof = wide.childProof(2007);
  check('wide_proof', wideProof);
  const wideRecord = bc.verifyChildProof(wideRoot, wideProof);
  check('wide_proof_verified', wideRecord.encoded);
  assert(wideRecord.chainId === 2007 && wideRecord.index === 7, 'wide record');
  assert(seen === want.size, `replayed ${seen} vectors, the file pins ${want.size}`);
  main.free(); userChain.free(); wide.free();
  return seen;
}

function restartAndRejections() {
  const node = bc.newNode(0x5501);
  const secret = node.newAccount().secret, stranger = node.newAccount().secret;
  node.free();
  const sign = (op, id, hash) => bc.signOp(op, id, hash, 0, bc.SigAlgo.ED25519, secret);
  const server = bc.newBLSValidator(Buffer.alloc(32, 0x02));
  const user = bc.newBLSValidator(Buffer.alloc(32, 0x12));
  const mainGenesis = bc.commitSubchainPolicy(recordGenesis(7, server), singleRegistrar(secret));
  const mainHash = bc.genesisHash(mainGenesis);
  const userGenesis = bc.commitParentBinding(recordGenesis(7001, user), 7, mainHash);
  const main = assemble(mainGenesis, server), userChain = assemble(userGenesis, user);
  const counts = main.registryCounts();
  assert(counts && counts.records === 0 && counts.open === 0 && counts.authorityEpoch === 0, 'empty dynamic registry');
  assert(userChain.registryCounts() === null, 'a leaf chain has no registry');

  const op = bc.opRegister(0, 0, registration(7001, userGenesis, user));
  throwsWith(() => main.proposeSubchain(10, [], [op]), 'not enough approvals', 'unsigned operation');
  throwsWith(() => main.proposeSubchain(10, [], [bc.signOp(op, 7, mainHash, 0, bc.SigAlgo.ED25519, stranger)]), 'bad registrar signature', 'forged operation');
  const otherParent = Buffer.from(mainHash); otherParent[0] ^= 1;
  throwsWith(() => main.proposeSubchain(10, [], [sign(op, 7, otherParent)]), '', 'an operation signed for another parent');
  throwsWith(() => bc.opDigest(op, 7, Buffer.alloc(31)), '32 bytes', 'short parent hash');
  const digest = bc.opDigest(op, 7, mainHash);
  assert(digest.length === 32, 'digest');
  const signed = sign(op, 7, mainHash);
  throwsWith(() => sign(signed, 7, mainHash), '', 'a registrar approves once');
  commit(main, main.proposeSubchain(10, [], [signed]));
  assert(main.registryCounts().records === 1 && main.registryCounts().open === 1, 'one open child');
  commit(userChain, userChain.propose(20));
  commit(main, main.proposeSubchain(11, [bc.pendingAnchor(main, userChain)], []));
  assert(bc.pendingAnchor(main, userChain) === null, 'nothing pending after anchoring');

  const mainSnap = main.snapshot(), userSnap = userChain.snapshot();
  assert(bc.chainSnapshotError(mainGenesis, mainSnap) === '', 'a good snapshot has no error');
  const mainBack = bc.restoreChain(mainGenesis, 0, server.public, server.secret, mainSnap);
  const userBack = bc.restoreChain(userGenesis, 0, user.public, user.secret, userSnap);
  assert(mainBack.snapshot().equals(mainSnap) && mainBack.tip().equals(main.tip()), 'restored chain equals the original');
  for (const u of [userChain, userBack]) commit(u, u.propose(21));
  const straight = commit(main, main.proposeSubchain(12, [bc.pendingAnchor(main, userChain)], []));
  const resumed = commit(mainBack, mainBack.proposeSubchain(12, [bc.pendingAnchor(mainBack, userBack)], []));
  assert(straight.equals(resumed) && mainBack.snapshot().equals(main.snapshot()), 'a restored chain continues byte for byte');
  const range = mainBack.archiveRange();
  assert(range && range.first === 1 && range.last === 3, 'archive range');
  assert(mainBack.archivedBlock(3).equals(straight) && mainBack.archivedCertificate(3).length > 0, 'archived block and certificate');
  assert(mainBack.pruneArchive(3) === 2 && mainBack.archiveRange().first === 3, 'prune');
  throwsWith(() => mainBack.archivedBlock(1), '', 'a pruned block is gone');

  throwsWith(() => bc.restoreChain(mainGenesis, 0, server.public, server.secret, Buffer.concat([mainSnap, Buffer.from([0])])), 'chain restore refused', 'trailing bytes refused');
  throwsWith(() => bc.restoreChain(userGenesis, 0, server.public, server.secret, mainSnap), 'another genesis', 'another genesis refused');
  assert(bc.chainSnapshotError(userGenesis, mainSnap).includes('another genesis'), 'snapshot error names the reason');

  const record = mainBack.childRecord(7001);
  assert(record.tipHeight === 2 && record.status === bc.ChildStatus.OPEN, 'record after restart');
  const root = bc.headerSubchainRoot(mainBack.tip());
  const proof = mainBack.childProof(7001);
  assert(bc.verifyChildProof(root, proof).chainId === 7001, 'child proof verifies');
  const bad = Buffer.from(proof); bad[bad.length - 1] ^= 1;
  throwsWith(() => bc.verifyChildProof(root, bad), '', 'a tampered proof is refused');
  throwsWith(() => mainBack.childRecord(9), '', 'unknown child');

  const replacement = bc.opSetRegistrars(0, 0, 1, [{ sigAlgo: bc.SigAlgo.ED25519, publicKey: stranger.subarray(32) }]);
  commit(mainBack, mainBack.proposeSubchain(13, [], [sign(replacement, 7, mainHash)]));
  assert(mainBack.registryCounts().authorityEpoch === 1, 'replacing the registrars bumps the epoch');
  const late = bc.opClose(0, 0, 7001, record.tipHeight, record.tipHash);
  throwsWith(() => mainBack.proposeSubchain(14, [], [sign(late, 7, mainHash)]), '', 'an operation for the old epoch is void');
  const closeNow = bc.opClose(1, 0, 7001, record.tipHeight, record.tipHash);
  const external = bc.addOpApproval(closeNow, 0, signDigest(bc.opDigest(closeNow, 7, mainHash), stranger));
  commit(mainBack, mainBack.proposeSubchain(14, [], [external]));
  assert(mainBack.childRecord(7001).status === bc.ChildStatus.CLOSED && mainBack.registryCounts().open === 0, 'the new registrar closes the child');

  for (const c of [main, userChain, mainBack, userBack]) c.free();
}

function signDigest(digest, edSecret) {
  const crypto = require('node:crypto');
  const pkcs8 = Buffer.concat([Buffer.from('302e020100300506032b657004220420', 'hex'), edSecret.subarray(0, 32)]);
  return crypto.sign(null, digest, crypto.createPrivateKey({ key: pkcs8, format: 'der', type: 'pkcs8' }));
}

function treeSnapshot() {
  const rootKey = bc.newBLSValidator(Buffer.alloc(32, 0x03));
  const childKey = bc.newBLSValidator(Buffer.alloc(32, 0x13));
  const spec = {
    chainId: 1, blueprint: new bc.Blueprint('pos', 'account', 'round-robin'),
    validators: [{ publicKey: rootKey.public, secret: rootKey.secret, power: 1 }],
    children: [{ chainId: 42, blueprint: new bc.Blueprint('bft', 'account'), validators: [{ publicKey: childKey.public, secret: childKey.secret, power: 1 }] }],
  };
  const round = (t, ts) => Buffer.concat([t.produce(42, ts), t.produce(1, ts)]);
  const straight = new bc.ChainTree(spec), first = new bc.ChainTree(spec);
  assert(round(straight, 10).equals(round(first, 10)), 'two trees from one spec agree');
  const snapshot = first.snapshot(); first.free();
  const second = bc.ChainTree.restore(spec, snapshot);
  assert(second.snapshot().equals(snapshot) && round(straight, 11).equals(round(second, 11)), 'a restored tree continues byte for byte');
  throwsWith(() => bc.ChainTree.restore(spec, Buffer.concat([snapshot, Buffer.from([1])])), 'restore refused', 'trailing bytes refused');
  straight.free(); second.free();
}

const want = vectors();
const replayed = want ? replayVectors(want) : 0;
restartAndRejections();
treeSnapshot();
console.log(`dynamic subchain test: OK (${replayed} golden vectors, signed operations, restart, tree snapshot)`);
