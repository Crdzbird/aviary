// Records from a key store and hybrid keys, peer records and address books,
// Noise sessions and the admission handshake.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }

const v = bc.newBLSValidator(Buffer.alloc(32, 0x71));
const genesis = bc.commitBlueprint(bc.buildGenesis({ chainId: 41, stateModel: 2, validators: [{ publicKey: v.public, power: 1 }] }), new bc.Blueprint('bft', 'record'));
const chain = bc.assembleChain(genesis, 0, v.public, v.secret);
const node = bc.newNode(1);
const ed = node.newAccount(), hy = node.newAccountHybrid();
const store = bc.newKeyStore('pass');
const edAddr = store.addAccount(ed.publicKey, ed.secret);
const k1 = bc.hash(Buffer.from('asset:1')), k2 = bc.hash(Buffer.from('asset:2'));
chain.writeFromStore(store, edAddr, k1, bc.RecordOp.PUT, Buffer.from('a'));
chain.addTx(bc.signRecordTxHybrid({ chainId: 41, nonce: 0, key: k2, op: bc.RecordOp.PUT, value: Buffer.from('b'), publicKey: hy.publicKey, secret: hy.secret }));
assert(JSON.parse(chain.drainEvents()).filter((e) => e.type === 'tx_admitted').length === 2, 'admission events');
const blk = chain.propose(2); assert(chain.submit(blk, chain.certify(blk)), 'record block');
assert(JSON.parse(chain.drainEvents()).filter((e) => e.type === 'tx_removed').length === 2, 'removal events');
assert(chain.record(k1).owner.equals(edAddr) && chain.record(k2).owner.equals(hy.address), 'ed25519 and hybrid authors');
store.lock(); throws(() => chain.writeFromStore(store, edAddr, k1, bc.RecordOp.DELETE), 'locked store refuses');
store.free(); node.free(); chain.free();

const a = bc.generateSecureIdentity(), b = bc.generateSecureIdentity();
const ra = bc.signPeerRecord({ id: 1, host: '10.0.0.1', port: 7001, identity: a });
const rb = bc.signPeerRecord({ id: 2, host: '10.0.0.2', port: 7002, identity: b });
const info = bc.readPeerRecord(rb);
assert(info.id === 2 && info.host === '10.0.0.2' && info.port === 7002 && info.sigPublic.equals(b.sig.public), 'record fields');
const forged = Buffer.from(rb); forged[forged.length - 1] ^= 1;
assert(bc.verifyPeerRecord(ra) && !bc.verifyPeerRecord(forged), 'record verification');
const mine = new bc.AddressBook(), theirs = new bc.AddressBook();
mine.add(ra); theirs.add(rb); theirs.add(ra);
assert(mine.merge(theirs.encode()) === 1 && mine.size === 2 && mine.record(2).equals(rb) && mine.record(9) === null, 'merge');
assert(mine.encode().equals(theirs.encode()), 'canonical books');
const evil = new bc.AddressBook(); evil.add(forged);
assert(new bc.AddressBook().merge(evil.encode()) === 0, 'forged record not merged');
throws(() => new bc.AddressBook(Buffer.from([1, 2, 3])), 'malformed book');
[mine, theirs, evil].forEach((x) => x.free());

const na = bc.generateNoiseIdentity(), nb = bc.generateNoiseIdentity();
const prologue = Buffer.from('aviary');
const init = bc.initiateNoiseSession(na, prologue);
const resp = bc.acceptNoiseSession(nb, init.handshakeMessage, prologue);
resp.completeResponder(init.completeInitiator(resp.handshakeMessage));
throws(() => init.encrypt(Buffer.from('x')), 'closed before the proofs');
resp.verifyIdentityProof(init.identityProof()); init.verifyIdentityProof(resp.identityProof());
assert(init.open && resp.open && init.peerIdentity.equals(nb.sig.public), 'open and pinned');
assert(init.handshakeHash.equals(resp.handshakeHash), 'same handshake hash');
assert(resp.decrypt(init.encrypt(Buffer.from('hello'))).toString() === 'hello', 'noise round trip');
init.free(); resp.free();

const issuer = bc.newBBSIssuer(Buffer.alloc(32, 0x11));
const policy = new bc.CredentialPolicy(7, issuer.public, Buffer.from('aviary.credential.v1'), 4, 2);
const cred = policy.issue(issuer.secret, Buffer.alloc(32, 0x77), 3, 4102444800, 1);
const gate = new bc.CredentialGate(policy);
const ch = gate.admissionChallenge(5);
const answer = policy.admissionAnswer(cred, ch.wire);
assert(gate.admissionGranted(5, ch.nonce, answer, 1700000000) && !gate.admissionGranted(6, ch.nonce, answer, 1700000000), 'admission');
gate.free();
console.log('aviary npm peering test: OK');
