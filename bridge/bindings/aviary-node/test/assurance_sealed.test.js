// AssuranceProfile tiers + SealedMempool (anti-MEV) over the npm binding.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }

const std = bc.describeAssurance(1);
assert(!std.ratcheted && !std.vrf && !std.thresholdCustody && !std.sealedMempool, 'standard baseline');
const hard = bc.describeAssurance(2);
assert(hard.ratcheted && hard.vrf && hard.thresholdCustody && !hard.sealedMempool, 'hardened tiers');
const fort = bc.describeAssurance(3);
assert(fort.ratcheted && fort.vrf && fort.thresholdCustody && fort.sealedMempool, 'fortress: all on');
assert(std.sigAlgos.includes(1) && !hard.sigAlgos.includes(1) && !fort.sigAlgos.includes(1),
  'stronger levels drop bare Ed25519');
let unknownThrew = false;
try { bc.describeAssurance(99); } catch { unknownThrew = true; }
assert(unknownThrew, 'unknown level errors');

// Sealed mempool: seal to a proposer, order by fee, only the proposer opens.
const proposer = bc.hybridKemKeypair();
const mk = (fee, tag) => bc.encodeTx({ version: 2, chainId: 7, sigAlgo: 1, fee,
  body: Buffer.from([tag]), author: Buffer.alloc(32), signature: Buffer.alloc(64) });
const pool = new bc.SealedMempool(64);
for (const f of [50, 300, 120]) assert(pool.add(bc.sealTx(mk(f, f & 0xff), proposer.public)), 'add');
assert(pool.size === 3, 'pool holds 3');
const opened = pool.reveal(proposer.secret, 10);
assert(opened.length === 3, 'proposer reveals 3');
const stranger = bc.hybridKemKeypair();
assert(pool.reveal(stranger.secret, 10).length === 0, 'a non-proposer opens nothing');
pool.free();

console.log('aviary npm assurance+sealed-mempool test: OK');
