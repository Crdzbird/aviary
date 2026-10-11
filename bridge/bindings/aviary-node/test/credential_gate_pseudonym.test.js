// CredentialGate Option B - pseudonymous membership (per-epoch Sybil resistance)
// over the npm binding. Blind issuance (commit -> blind-sign -> finalize), then a
// per-(epoch, scope) pseudonym: same member+epoch+scope -> the same nym, a
// different epoch -> an unlinkable one.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
const eq = (a, b) => Buffer.compare(Buffer.from(a), Buffer.from(b)) === 0;

const issuer = bc.newBBSIssuer(Buffer.alloc(32, 0x2a));
const policy = new bc.CredentialPolicy(7n, issuer.public, Buffer.from('aviary.credential.v2'),
  4, 2, bc.CredentialModePseudonymous);

const now = 1700000000n;
const notAfter = 4102444800n;

function issue(memberSecret, tier, na, epoch) {
  const mc = policy.membershipCommit(memberSecret);
  const commitment = bc.extractCommitment(mc);
  const bi = policy.blindIssue(issuer.secret, commitment, tier, na, epoch);
  return bc.finalizeMembership(mc, bi);
}

const memberSecret = Buffer.alloc(32, 0x77);
const cred = issue(memberSecret, 3, notAfter, 1);

const gate = new bc.CredentialGate(policy);
const ch = (b) => Buffer.alloc(32, b);
const present = (c, epoch, scope, b) => policy.presentPseudonymous(c, epoch, scope, ch(b));

// Happy path: admit returns a 48-byte pseudonym.
const nymA = gate.admitPseudonymous(5, 'gossip', ch(1), present(cred, 5, 'gossip', 1), now);
assert(nymA && nymA.length === 48, 'admit returns a 48-byte pseudonym');

// Sybil-linkable: a fresh presentation, same member+epoch+scope -> the same nym.
const nym2 = gate.admitPseudonymous(5, 'gossip', ch(2), present(cred, 5, 'gossip', 2), now);
assert(nym2 && eq(nym2, nymA), 'same member+epoch+scope -> same nym');
assert(gate.noteAdmission(5, 'gossip', nymA), 'first admission is fresh');
assert(!gate.noteAdmission(5, 'gossip', nym2), 'a second session on the nym is flagged (Sybil cap)');

// Unlinkable across epochs / scopes.
const nymE = gate.admitPseudonymous(6, 'gossip', ch(1), present(cred, 6, 'gossip', 1), now);
assert(nymE && !eq(nymE, nymA), 'different epoch -> different nym');
const nymS = gate.admitPseudonymous(5, 'submit', ch(1), present(cred, 5, 'submit', 1), now);
assert(nymS && !eq(nymS, nymA), 'different scope -> different nym');

// Different member -> different nym.
const nymO = gate.admitPseudonymous(5, 'gossip', ch(1),
  present(issue(Buffer.alloc(32, 0x88), 3, notAfter, 1), 5, 'gossip', 1), now);
assert(nymO && !eq(nymO, nymA), 'different member -> different nym');

// Context binding (anti-replay).
const presA = present(cred, 5, 'gossip', 1);
assert(!gate.admitPseudonymous(6, 'gossip', ch(1), presA, now), 'wrong epoch rejected');
assert(!gate.admitPseudonymous(5, 'submit', ch(1), presA, now), 'wrong scope rejected');
assert(!gate.admitPseudonymous(5, 'gossip', ch(9), presA, now), 'wrong challenge rejected');

// Expiry + tier.
assert(!gate.admitPseudonymous(5, 'gossip', ch(1), present(issue(memberSecret, 3, 1000n, 1), 5, 'gossip', 1), now),
  'expired credential rejected');
assert(!gate.admitPseudonymous(5, 'gossip', ch(1), present(issue(memberSecret, 1, notAfter, 1), 5, 'gossip', 1), now),
  'below-min-tier rejected');

// Targeted nym revocation: bans epoch 5, not epoch 6.
{
  const revoking = new bc.CredentialGate(policy);
  revoking.revokeNym(nymA);
  assert(!revoking.admitPseudonymous(5, 'gossip', ch(3), present(cred, 5, 'gossip', 3), now), 'revoked nym rejected');
  assert(revoking.admitPseudonymous(6, 'gossip', ch(3), present(cred, 6, 'gossip', 3), now), "a different epoch's nym is not banned");
  revoking.free();
}

// Forged issuer + malformed reject.
const rogue = bc.newBBSIssuer(Buffer.alloc(32, 0x99));
const fmc = policy.membershipCommit(memberSecret);
const fbi = policy.blindIssue(rogue.secret, bc.extractCommitment(fmc), 3, notAfter, 1);
const forged = bc.finalizeMembership(fmc, fbi);
assert(!gate.admitPseudonymous(5, 'gossip', ch(1), present(forged, 5, 'gossip', 1), now), 'forged issuer rejected');
assert(!gate.admitPseudonymous(5, 'gossip', ch(1), Buffer.from('garbage'), now), 'malformed rejected');

gate.free();
console.log('aviary npm credential-gate pseudonymous test: OK');
