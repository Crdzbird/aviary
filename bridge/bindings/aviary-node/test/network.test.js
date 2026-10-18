// A deterministic four validator line mesh with an observer, then a secure session.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }
const keys = [0, 1, 2, 3].map((i) => bc.newBLSValidator(Buffer.alloc(32, 0x60 + i)));
const alice = Buffer.alloc(32, 0x11);
const genesis = bc.buildGenesis({ chainId: 42, accounts: [{ address: alice, balance: 1000 }], validators: keys.map((k) => ({ publicKey: k.public, power: 1 })) });
const mesh = new bc.PeerMesh(keys.map((k, i) => bc.newNetNode({ genesis, me: i, blsKey: k })));
mesh.nodes.push(bc.newNetNode({ genesis, me: bc.NET_OBSERVER }));
for (let i = 0; i < 4; i++) mesh.link(i, i + 1);
const nodes = mesh.nodes, validators = [0, 1, 2, 3];
const committed = () => validators.every((i) => nodes[i].committed);
mesh.start();
assert(mesh.settle(validators), 'line mesh settles');
assert(committed() && nodes.every((n) => n.height === 1), 'line mesh commits through relays, observer follows');
assert(nodes[0].tip().equals(nodes[4].tip()) && nodes[4].balance(alice) === 1000, 'observer has the tip and state');
assert(nodes[0].replayLog().length > 0, 'catch-up log');
assert(bc.verifyStateProof(nodes[2].stateRoot(), nodes[2].proveAccount(alice)), 'proof from a networked node');
assert(nodes[0].onGossip(1, Buffer.from([9, 9, 9])).relays.length === 0 && !nodes[0].peerBanned(1), 'garbage ignored');
mesh.advance();
assert(mesh.settle(validators), 'second height settles');
assert(nodes.every((n) => n.height === 2), 'second height');
throws(() => nodes[0].setTransactions([Buffer.from([1, 2])]), 'malformed transaction refused');
mesh.free();
const full = new bc.PeerMesh(keys.map((k, i) => bc.newNetNode({ genesis, me: i, blsKey: k })));
full.linkAll(); full.start();
assert(full.settle() && full.nodes.every((n) => n.height === 1), 'full mesh');
assert(!full.run(() => false, 1), 'run reports a miss');
full.free();

const a = bc.generateSecureIdentity(), b = bc.generateSecureIdentity();
const init = bc.initiateSecureSession(b.kem.public, a.sig);
assert(!init.open, 'initiator waits');
const resp = bc.acceptSecureSession(b, init.handshakeMessage);
assert(resp.open && resp.peerIdentity.equals(a.sig.public), 'responder open and pinned');
init.complete(resp.handshakeMessage);
assert(init.open && init.peerIdentity.equals(b.sig.public), 'initiator open and pinned');
assert(resp.decrypt(init.encrypt(Buffer.from('hello'))).toString() === 'hello', 'a to b');
assert(init.decrypt(resp.encrypt(Buffer.from('back'))).toString() === 'back', 'b to a');
const frame = init.encrypt(Buffer.from('x')); frame[frame.length - 1] ^= 1;
throws(() => resp.decrypt(frame), 'tampered frame refused');
throws(() => bc.acceptSecureSession(a, bc.initiateSecureSession(b.kem.public, a.sig).handshakeMessage), 'wrong responder identity refused');
init.free(); resp.free();
console.log('aviary npm network test: OK');
