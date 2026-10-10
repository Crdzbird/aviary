part of '../../aviary.dart';

/// Fluent construction and inspection of chain families.
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
