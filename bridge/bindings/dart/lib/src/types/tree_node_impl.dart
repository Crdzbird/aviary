part of '../../aviary.dart';

final class _TreeNode implements TreeNode {
  const _TreeNode({
    required this.chainId,
    required this.validators,
    this.blueprint,
    this.assuranceLevel = 1,
    this.accounts = const [],
    this.maxAnchorsPerBlock = 0,
    this.maxHeadersPerAnchor = 0,
    this.children = const [],
  });

  @override
  final int chainId;
  @override
  final Blueprint? blueprint;
  @override
  final int assuranceLevel;
  @override
  final List<GenesisAccountEntry> accounts;
  @override
  final List<TreeValidator> validators;
  @override
  final int maxAnchorsPerBlock;
  @override
  final int maxHeadersPerAnchor;
  @override
  final List<TreeNode> children;

  @override
  TreeNode copyWith({
    int? chainId,
    Blueprint? blueprint,
    int? assuranceLevel,
    List<GenesisAccountEntry>? accounts,
    List<TreeValidator>? validators,
    int? maxAnchorsPerBlock,
    int? maxHeadersPerAnchor,
    List<TreeNode>? children,
  }) => _TreeNode(
    chainId: chainId ?? this.chainId,
    validators: validators ?? this.validators,
    blueprint: blueprint ?? this.blueprint,
    assuranceLevel: assuranceLevel ?? this.assuranceLevel,
    accounts: accounts ?? this.accounts,
    maxAnchorsPerBlock: maxAnchorsPerBlock ?? this.maxAnchorsPerBlock,
    maxHeadersPerAnchor: maxHeadersPerAnchor ?? this.maxHeadersPerAnchor,
    children: children ?? this.children,
  );
}
