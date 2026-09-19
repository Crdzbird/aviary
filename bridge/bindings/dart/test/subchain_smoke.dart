// Subchains over the Dart binding: a single-validator BFT child finalizes real
// certified blocks; a parent registers it at genesis, anchors the child's
// certified headers, and a balance on the child is proven from the parent's
// header alone. Every tamper is refused.
import 'dart:typed_data';
import 'package:blockchainer_dart/blockchainer.dart';

void expect(bool ok, String msg) { if (!ok) throw StateError('FAIL: $msg'); }
bool throws(void Function() f) { try { f(); return false; } catch (_) { return true; } }
Uint8List fill(int b, [int n = 32]) => Uint8List(n)..fillRange(0, n, b);
bool eq(Uint8List a, Uint8List b) { if (a.length != b.length) return false; for (var i = 0; i < a.length; i++) { if (a[i] != b[i]) return false; } return true; }

void main() {
  final bc = Blockchainer.instance;
  final cv = bc.newBLSValidator(fill(0x71)), pv = bc.newBLSValidator(fill(0x72));
  final alice = fill(0xA1);
  final bft = bc.makeBlueprint(consensus: 'bft', stateModel: 'account');
  final childGen = bc.commitBlueprint(bc.buildGenesis(chainId: 42, accounts: [(address: alice, balance: 1000)], validators: [(publicKey: cv.public, power: 1)]), bft);
  final childHeader = bc.genesisHeader(childGen);
  var parentGen = bc.commitBlueprint(bc.buildGenesis(chainId: 1, accounts: [], validators: [(publicKey: pv.public, power: 1)]), bft);
  final reg = bc.encodeChildRegistration(chainId: 42, genesisHeader: childHeader, validators: [(publicKey: cv.public, power: 1)]);
  final policy = bc.encodeSubchainPolicy(maxChildren: 1, maxHeadersPerAnchor: 4, children: [reg]);
  final plainHash = bc.genesisHash(parentGen);
  parentGen = bc.commitSubchainPolicy(parentGen, policy);
  expect(!eq(plainHash, bc.genesisHash(parentGen)), 'policy changes the parent genesis hash');
  final ownChild = bc.encodeSubchainPolicy(maxChildren: 1, children: [bc.encodeChildRegistration(chainId: 1, genesisHeader: childHeader, validators: [(publicKey: cv.public, power: 1)])]);
  expect(throws(() => bc.commitSubchainPolicy(parentGen, ownChild)), 'own-chain child refused');
  final bound = bc.commitParentBinding(childGen, 1, bc.genesisHash(parentGen));
  expect(!eq(bc.genesisHash(bound), bc.genesisHash(childGen)) && eq(bc.genesisHeader(bound), childHeader), 'binding changes the hash, not the header');

  final child = bc.assembleChain(bound, 0, cv.public, cv.secret)!;
  final parent = bc.assembleChain(parentGen, 0, pv.public, pv.secret)!;
  final certified = <Uint8List>[];
  for (var ts = 2; ts <= 3; ts++) {
    final blk = child.propose(ts); final cert = child.certify(blk);
    expect(child.submit(blk, cert), 'child submit');
    certified.add(bc.certifiedHeader(bc.blockHeader(blk), cert));
  }
  expect(child.height == 2 && parent.childTip(42).isNotEmpty, 'child state');
  expect(throws(() => parent.childTip(99)) && throws(() => child.childTip(42)), 'unknown child / non-parent refused');
  final anchor = bc.encodeAnchor(42, certified);
  final pblk = parent.proposeAnchored(2, [anchor]);
  expect(parent.submit(pblk, parent.certify(pblk)) && parent.height == 1, 'parent anchored');
  final tampered = Uint8List.fromList(certified[1])..[certified[1].length - 1] ^= 1;
  expect(throws(() => parent.proposeAnchored(3, [bc.encodeAnchor(42, [tampered])])), 'tampered anchor refused');
  expect(throws(() => parent.proposeAnchored(3, [anchor])), 'stale anchor refused');
  final root = bc.headerSubchainRoot(bc.blockHeader(pblk));
  final proof = bc.encodeSubchainProof([parent.subchainHop(42)], child.proveAccount(alice));
  expect(bc.verifySubchainProof(root, proof), 'cross-chain proof verifies');
  final bad = Uint8List.fromList(proof)..[proof.length - 1] ^= 1;
  expect(!bc.verifySubchainProof(fill(1), proof) && !bc.verifySubchainProof(root, bad) && !bc.verifySubchainProof(root, bc.encodeSubchainProof([], child.proveAccount(alice))), 'tampered / rootless proofs fail');
  final blk3 = child.propose(4); final cert3 = child.certify(blk3); child.submit(blk3, cert3);
  final pblk2 = parent.proposeAnchored(3, [bc.encodeAnchor(42, [bc.certifiedHeader(bc.blockHeader(blk3), cert3)])]);
  expect(parent.submit(pblk2, parent.certify(pblk2)), 'parent block 2');
  final root2 = bc.headerSubchainRoot(bc.blockHeader(pblk2));
  expect(!bc.verifySubchainProof(root2, proof) && bc.verifySubchainProof(root, proof), 'old proof stale at new root, valid at old');
  expect(bc.verifySubchainProof(root2, bc.encodeSubchainProof([parent.subchainHop(42)], child.proveAccount(alice))), 'fresh proof at new root');
  child.free(); parent.free();
  print('blockchainer dart subchain smoke: OK (BFT child anchored by a parent; cross-chain proof from the parent header)');
}
