// FROST t-of-n threshold accounts over the npm binding: a 2-of-3 aggregate is a
// standard Ed25519 signature under the group key; a lone share can't; a corrupt
// cosigner is attributed; a sealed share round-trips.
const bc = require('..');
const crypto = require('crypto');

function assert(ok, msg) { if (!ok) throw new Error('FAIL: ' + msg); }
function fails(fn) { try { fn(); return false; } catch { return true; } }
// Verify Ed25519 via Node's WebCrypto-free crypto: build a raw-key verifier.
function edVerify(msg, sig, rawPub) {
  const spki = Buffer.concat([Buffer.from('302a300506032b6570032100', 'hex'), rawPub]);
  const key = crypto.createPublicKey({ key: spki, format: 'der', type: 'spki' });
  return crypto.verify(null, Buffer.from(msg), key, Buffer.from(sig));
}

const cards = bc.createThresholdAccount(3, 2);
assert(cards.length === 3, '3 cards dealt');
const g0 = bc.shareInfo(cards[0]);
const groupPK = g0.groupPublicKey;
assert(groupPK.length === 32, 'group pk 32 bytes');

const msg = Buffer.from('threshold-signed transfer');
const c0 = bc.commit(cards[0]);
const c2 = bc.commit(cards[2]);
const cer = new bc.Ceremony(groupPK, msg, [c2.commitment, c0.commitment]);

const s0 = cer.sign(cards[0], c0.nonces);
const s2 = cer.sign(cards[2], c2.nonces);
const i0 = bc.shareInfo(cards[0]).index, i2 = bc.shareInfo(cards[2]).index;
const sig = cer.aggregate([i0, i2], [s0, s2]);
assert(sig.length === 64, 'aggregate is a 64-byte Ed25519 signature');
assert(edVerify(msg, sig, groupPK), 'aggregate verifies as Ed25519 under the group key');

// Attribution: honest share ok, corrupt share named.
assert(cer.verifyShare(i0, g0.publicShare, s0), 'honest share attributed');
const bad = Buffer.from(s2); bad[5] ^= 1;
assert(!cer.verifyShare(i2, bc.shareInfo(cards[2]).publicShare, bad), 'corrupt cosigner detected');
cer.free();

// Below threshold: one share alone cannot forge the group signature.
const c1 = bc.commit(cards[1]);
const solo = new bc.Ceremony(groupPK, msg, [c1.commitment]);
const sSolo = solo.sign(cards[1], c1.nonces);
let soloOk = false;
try { soloOk = edVerify(msg, solo.aggregate([bc.shareInfo(cards[1]).index], [sSolo]), groupPK); } catch {}
assert(!soloOk, 'a single share cannot produce a valid group signature');
solo.free();

// Sealed share round-trip.
const env = bc.sealShare(cards[1], 'share-pass', 3, 1 << 16);
assert(Buffer.compare(bc.unsealShare(env, 'share-pass'), cards[1]) === 0, 'sealed share round-trips');
assert(fails(() => bc.unsealShare(env, 'wrong')), 'wrong passphrase fails closed');

console.log('blockchainer npm threshold test: OK (2-of-3 → Ed25519, threshold enforced, attribution, sealed)');
