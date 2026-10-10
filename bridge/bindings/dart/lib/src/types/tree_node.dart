part of '../../aviary.dart';

/// One chain of a family; [children] are subchains and must be BFT.
abstract final class TreeNode {
  /// Declares one chain and its subchains.
  const factory TreeNode({
    required int chainId,
    required List<TreeValidator> validators,
    Blueprint? blueprint,
    int assuranceLevel,
    List<GenesisAccountEntry> accounts,
    int maxAnchorsPerBlock,
    int maxHeadersPerAnchor,
    List<TreeNode> children,
  }) = _TreeNode;

  /// The chain id.
  int get chainId;

  /// The mixture; null means bft over the account model.
  Blueprint? get blueprint;

  /// The assurance level: 1 standard, 2 hardened, 3 fortress.
  int get assuranceLevel;

  /// Accounts funded at genesis.
  List<GenesisAccountEntry> get accounts;

  /// The validators, with the secrets this process holds.
  List<TreeValidator> get validators;

  /// Anchors per block, 0 for unlimited.
  int get maxAnchorsPerBlock;

  /// Headers per anchor, 0 for unlimited.
  int get maxHeadersPerAnchor;

  /// The subchains.
  List<TreeNode> get children;

  /// A copy with the given fields replaced.
  TreeNode copyWith({
    int? chainId,
    Blueprint? blueprint,
    int? assuranceLevel,
    List<GenesisAccountEntry>? accounts,
    List<TreeValidator>? validators,
    int? maxAnchorsPerBlock,
    int? maxHeadersPerAnchor,
    List<TreeNode>? children,
  });
}
