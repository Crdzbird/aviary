part of '../../aviary.dart';

/// Turning a validator key pair into the entries the genesis, rotation and
/// tree APIs take.
/// {@category Ledger}
extension ValidatorKeyPair on KeyPairBytes {
  /// A validator set entry with [power].
  ValidatorEntry entry(int power) => (publicKey: public, power: power);

  /// A tree validator with [power]. Set [holdsSecret] false to describe a
  /// validator this process does not run.
  TreeValidator treeValidator(int power, {bool holdsSecret = true}) =>
      (publicKey: public, secret: holdsSecret ? secret : null, power: power);
}

/// The propose, justify and submit flow in one call.
/// {@category Chains}
extension ChainBlocks on Chain {
  /// Proposes, justifies and applies the next block, returning it. [justify]
  /// defaults to [certify], which is right for a BFT chain whose only
  /// validator is this node; pass [seal] for leader sealed engines. Proof of
  /// work submits the mined block itself, so call [propose], [mine] and
  /// [submit] directly there. Throws when the block is refused.
  Uint8List produceBlock(
    int timestamp, {
    Uint8List Function(Uint8List block)? justify,
  }) {
    final block = propose(timestamp);
    final justification = (justify ?? certify)(block);
    if (!submit(block, justification)) {
      throw const AviaryException('block refused');
    }
    return block;
  }
}

/// Fluent construction and inspection of chain families.
/// {@category Subchains and trees}
extension TreeNodeBuilding on TreeNode {
  /// A copy with [child] appended to [children].
  TreeNode withChild(TreeNode child) =>
      copyWith(children: [...children, child]);

  /// The node with [id] in this subtree, or null.
  TreeNode? find(int id) {
    if (chainId == id) return this;
    for (final child in children) {
      final hit = child.find(id);
      if (hit != null) return hit;
    }
    return null;
  }

  /// Every chain id in this subtree, parents first.
  List<int> get chainIds => [chainId, for (final c in children) ...c.chainIds];
}

/// Driving every chain of a family together.
/// {@category Subchains and trees}
extension ChainTreeRounds on ChainTree {
  /// Produces one block on every chain, children before parents, so each
  /// parent anchors what its children just finalized. Returns the blocks in
  /// that order.
  List<Uint8List> produceRound(int timestamp) => [
    for (final id in chainIds().reversed) produce(id, timestamp),
  ];
}
