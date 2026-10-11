import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'bytes.dart';

/// A BFT child (chain 42, [alice] funded) registered in a BFT parent
/// (chain 1), both assembled; the child has produced [certified] headers.
final class SubchainFixture {
  SubchainFixture(this.bc)
    : cv = bc.newBLSValidator(fill(0x71)),
      pv = bc.newBLSValidator(fill(0x72)) {
    final bft = bc.makeBlueprint(consensus: 'bft', stateModel: 'account');
    childGenesis = bc.commitBlueprint(
      bc.buildGenesis(
        chainId: 42,
        accounts: [(address: alice, balance: 1000)],
        validators: [childSet],
      ),
      bft,
    );
    plainParent = bc.commitBlueprint(
      bc.buildGenesis(chainId: 1, accounts: [], validators: [parentSet]),
      bft,
    );
    parentGenesis = bc.commitSubchainPolicy(
      plainParent,
      bc.encodeSubchainPolicy(
        maxChildren: 1,
        maxHeadersPerAnchor: 4,
        children: [registration(42)],
      ),
    );
    bound = bc.commitParentBinding(
      childGenesis,
      1,
      bc.genesisHash(parentGenesis),
    );
    child = bc.assembleChain(bound, 0, cv.public, cv.secret)!;
    parent = bc.assembleChain(parentGenesis, 0, pv.public, pv.secret)!;
  }

  final Aviary bc;
  final KeyPairBytes cv;
  final KeyPairBytes pv;
  final Uint8List alice = fill(0xA1);
  late final Uint8List childGenesis;
  late final Uint8List plainParent;
  late final Uint8List parentGenesis;
  late final Uint8List bound;
  late final Chain child;
  late final Chain parent;
  final List<Uint8List> certified = [];

  ValidatorEntry get childSet => (publicKey: cv.public, power: 1);
  ValidatorEntry get parentSet => (publicKey: pv.public, power: 1);
  Uint8List get childHeader => bc.genesisHeader(childGenesis);

  Uint8List registration(int chainId) => bc.encodeChildRegistration(
    chainId: chainId,
    genesisHeader: childHeader,
    validators: [childSet],
  );

  /// Produces one child block at [timestamp] and records its certified header.
  Uint8List produceChild(int timestamp) {
    final blk = child.propose(timestamp);
    final cert = child.certify(blk);
    if (!child.submit(blk, cert)) throw StateError('child submit refused');
    final header = bc.certifiedHeader(bc.blockHeader(blk), cert);
    certified.add(header);
    return header;
  }

  /// Anchors [headers] into the parent at [timestamp]; returns the block.
  Uint8List anchor(int timestamp, List<Uint8List> headers) {
    final blk = parent.proposeAnchored(timestamp, [
      bc.encodeAnchor(42, headers),
    ]);
    if (!parent.submit(blk, parent.certify(blk))) {
      throw StateError('parent submit refused');
    }
    return blk;
  }

  /// A cross chain proof of [alice]'s account through the parent.
  Uint8List proof() => bc.encodeSubchainProof([
    parent.subchainHop(42),
  ], child.proveAccount(alice));

  void free() {
    child.free();
    parent.free();
  }
}
