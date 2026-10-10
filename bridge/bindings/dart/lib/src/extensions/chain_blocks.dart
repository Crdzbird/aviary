part of '../../blockchainer.dart';

/// The propose, justify and submit flow in one call.
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
      throw const BlockchainerException('block refused');
    }
    return block;
  }
}
