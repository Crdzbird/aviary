// A deterministic four validator line mesh with an observer, then a secure session.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
function throws(fn, m) { let t = false; try { fn(); } catch (_) { t = true; } assert(t, m); }
const keys = [0, 1, 2, 3].map((i) => bc.newBLSValidator(Buffer.alloc(32, 0x60 + i)));
const alice = Buffer.alloc(32, 0x11);
const genesis = bc.buildGenesis({ chainId: 42, accounts: [{ address: alice, balance: 1000 }], validators: keys.map((k) => ({ publicKey: k.public, power: 1 })) });
const nodes = keys.map((k, i) => bc.newNetNode({ genesis, me: i, blsKey: k }));
nodes.push(bc.newNetNode({ genesis, me: bc.NET_OBSERVER, peers: [3] }));
for (const [x, y] of [[0, 1], [1, 2], [2, 3], [3, 4]]) { nodes[x].addPeer(y); nodes[y].addPeer(x); }
const queue = [], timeouts = [];
const apply = (i, out) => { for (const r of out.relays) queue.push([i, r.to, r.message]); for (const t of out.timeouts) timeouts.push([t.round, t.step, i]); };
const run = (done) => {
  for (let step = 0; step < 20000; step++) {
    if (done()) return;
    if (queue.length) { const [from, to, p] = queue.shift(); apply(to, nodes[to].onGossip(from, p)); continue; }
    if (!timeouts.length) return;
    timeouts.sort((p, q) => p[0] - q[0] || p[1] - q[1]);
    const [round, s, i] = timeouts.shift(); apply(i, nodes[i].onTimeout({ step: s, round }));
  }
};
const committed = () => nodes.slice(0, 4).every((n) => n.committed);
nodes.forEach((n, i) => apply(i, n.start()));
run(committed); run(() => false);
assert(committed() && nodes.every((n) => n.height === 1), 'line mesh commits through relays, observer follows');
assert(nodes[0].tip().equals(nodes[4].tip()) && nodes[4].balance(alice) === 1000, 'observer has the tip and state');
assert(nodes[0].replayLog().length > 0, 'catch-up log');
assert(bc.verifyStateProof(nodes[2].stateRoot(), nodes[2].proveAccount(alice)), 'proof from a networked node');
assert(nodes[0].onGossip(1, Buffer.from([9, 9, 9])).relays.length === 0 && !nodes[0].peerBanned(1), 'garbage ignored');
nodes.forEach((n, i) => apply(i, n.advance()));
run(committed); run(() => false);
assert(nodes.every((n) => n.height === 2), 'second height');
throws(() => nodes[0].setTransactions([Buffer.from([1, 2])]), 'malformed transaction refused');
nodes.forEach((n) => n.free());

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
