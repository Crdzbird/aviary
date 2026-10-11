// Events and metrics on an assembled chain and a networked node.
const bc = require('..');
function assert(ok, m) { if (!ok) throw new Error('FAIL: ' + m); }
const v = bc.newBLSValidator(Buffer.alloc(32, 0x70));
const validators = [{ publicKey: v.public, power: 1 }];
const genesis = bc.commitBlueprint(bc.buildGenesis({ chainId: 9, validators }), new bc.Blueprint('pos', 'account', 'round-robin'));
const chain = bc.assembleChain(genesis, 0, v.public, v.secret);
const hooked = [];
chain.onEvent((j) => hooked.push(JSON.parse(j)));
const blk = chain.propose(2);
assert(chain.submit(blk, chain.seal(blk)), 'submit');
assert(hooked.length === 1 && hooked[0].type === 'block_finalized' && hooked[0].height === 1, 'chain pushes block_finalized');
assert(chain.drainEvents() === '[]', 'buffer empty while hooked');
chain.onEvent(null);
assert(!chain.submit(Buffer.from([1, 2, 3])), 'garbage refused');
assert(chain.metrics().includes('aviary_chain_height 1') && chain.metrics().includes('aviary_blocks_finalized_total 1'), 'chain metrics');
chain.free();

const alice = Buffer.alloc(32, 0x11);
const g2 = bc.buildGenesis({ chainId: 42, accounts: [{ address: alice, balance: 1000 }], validators });
const net = bc.newNetNode({ genesis: g2, me: 0, blsKey: v });
net.start();
assert(net.committed, 'single validator commits on start');
assert(JSON.parse(net.drainEvents()).some((e) => e.type === 'block_finalized' && e.height === 1), 'net event buffered');
net.onGossip(1, Buffer.from([9, 9]));
assert(net.metrics().includes('aviary_chain_height 1') && net.metrics().includes('aviary_gossip_received_total 1'), 'net metrics: ' + net.metrics());
const pushed = [];
net.onEvent((j) => pushed.push(JSON.parse(j)));
net.advance();
assert(pushed.length === 1 && pushed[0].height === 2, 'net pushes the next height');
net.free();
console.log('aviary npm observability test: OK');
