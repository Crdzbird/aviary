// PortableAccount tiered export over the npm (koffi) binding.
const bc = require('..');

function eq(a, b) { return Buffer.compare(Buffer.from(a), Buffer.from(b)) === 0; }
function assert(ok, msg) { if (!ok) { throw new Error('FAIL: ' + msg); } }
function fails(fn) { try { fn(); return false; } catch { return true; } }

const node = bc.newNode(7);
const acct = node.newAccountHybrid();
const { publicKey: pub, secret } = acct;

// Tier 1 — passphrase.
const env1 = bc.sealAccountPassphrase(pub, secret, 'backup-phrase', 3, 1 << 16);
const back1 = bc.openAccountPassphrase(env1, 'backup-phrase');
assert(eq(back1.secret, secret) && eq(back1.public, pub), 'passphrase round-trip');
assert(fails(() => bc.openAccountPassphrase(env1, 'wrong')), 'wrong passphrase fails closed');

// Device key.
const dev = bc.hybridKemKeypair();

// Tier 2 — recipient-bound.
const env2 = bc.sealAccountTo(pub, secret, dev.public);
assert(eq(bc.openAccountTo(env2, dev.secret).secret, secret), 'recipient round-trip');
const stranger = bc.hybridKemKeypair();
assert(fails(() => bc.openAccountTo(env2, stranger.secret)), 'a different device cannot open');

// Tier 3 — signed-recipient (owner = the account's hybrid sig key).
const env3 = bc.sealAccountSignedTo(pub, secret, dev.public, secret);
assert(eq(bc.openAccountSignedFrom(env3, dev.secret, pub).secret, secret), 'signed round-trip');
assert(fails(() => bc.openAccountSignedFrom(env3, dev.secret, stranger.public)), 'wrong signer fails closed');
const tampered = Buffer.from(env3); tampered[tampered.length >> 1] ^= 1;
assert(fails(() => bc.openAccountSignedFrom(tampered, dev.secret, pub)), 'tampered envelope fails closed');

console.log('blockchainer npm portable test: OK (passphrase + recipient + signed tiers, all fail-closed)');
