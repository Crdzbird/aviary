// CredentialGate — anonymous membership admission (Option A, unlinkable) over the
// npm binding. An issuer signs a member's attributes as a BBS credential; a
// holder admits itself to a role by proving in zero knowledge it holds an
// issuer-signed credential whose tier clears the policy and whose expiry has not
// passed — revealing nothing else.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }

const issuer = bc.newBBSIssuer(Buffer.alloc(32, 0x11));
assert(issuer.public.length === 96 && issuer.secret.length === 32, 'BBS issuer key sizes');

const policy = new bc.CredentialPolicy(7n, issuer.public, Buffer.from('aviary.credential.v1'), 4, 2);
assert(policy.hash().length === 32, 'policy hash is 32 bytes');

const memberSecret = Buffer.alloc(32, 0xAB);
const notAfter = 4102444800n; // year 2100
const now = 1700000000n;
const cred = policy.issue(issuer.secret, memberSecret, 3, notAfter, 1);

const gate = new bc.CredentialGate(policy);
const challenge = Buffer.alloc(32, 0x01);

// Happy path.
const pres = policy.present(cred, 5, 'submit', challenge);
assert(gate.admit(5, 'submit', challenge, pres, now), 'valid presentation admitted');

// Unlinkability: a fresh presentation differs on the wire yet still admits.
const pres2 = policy.present(cred, 5, 'submit', challenge);
assert(!pres.equals(pres2), 'two presentations of one credential are unlinkable');
assert(gate.admit(5, 'submit', challenge, pres2, now), 'second presentation admitted');

// Context binding (anti-replay).
assert(!gate.admit(6, 'submit', challenge, pres, now), 'wrong epoch rejected');
assert(!gate.admit(5, 'gossip', challenge, pres, now), 'wrong scope rejected');
assert(!gate.admit(5, 'submit', Buffer.alloc(32, 0x02), pres, now), 'wrong challenge rejected');

// Expiry.
const ep = policy.present(policy.issue(issuer.secret, memberSecret, 3, 1000n, 1), 5, 'submit', challenge);
assert(!gate.admit(5, 'submit', challenge, ep, now), 'expired credential rejected');

// Tier below the policy threshold.
const lp = policy.present(policy.issue(issuer.secret, memberSecret, 1, notAfter, 1), 5, 'submit', challenge);
assert(!gate.admit(5, 'submit', challenge, lp, now), 'below-min-tier rejected');

// Bulk revocation.
const revoking = new bc.CredentialGate(policy);
revoking.revokeIssuerEpoch(1);
assert(!revoking.admit(5, 'submit', challenge, pres, now), 'revoked issuer epoch rejected');
revoking.free();

// Forgery: a credential from another issuer.
const rogue = bc.newBBSIssuer(Buffer.alloc(32, 0x77));
const fp = policy.present(policy.issue(rogue.secret, memberSecret, 3, notAfter, 1), 5, 'submit', challenge);
assert(!gate.admit(5, 'submit', challenge, fp, now), 'forged (wrong-issuer) credential rejected');

// Malformed presentation is a quiet reject.
assert(!gate.admit(5, 'submit', challenge, Buffer.from('garbage'), now), 'malformed presentation rejected');

gate.free();
console.log('aviary npm credential-gate test: OK');
