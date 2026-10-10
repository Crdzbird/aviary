part of '../../aviary.dart';

/// Handles released at the end of a scope, whatever [body] does.
extension AviaryScopes on Aviary {
  /// Runs [body] with a fresh single validator node and disposes it after.
  R withNode<R>(int chainId, R Function(Node node) body) {
    final node = createNode(chainId);
    try {
      return body(node);
    } finally {
      node.dispose();
    }
  }

  /// Runs [body] with an unlocked key store and releases it after.
  R withKeyStore<R>(String passphrase, R Function(KeyStore store) body) {
    final store = createKeyStore(passphrase);
    try {
      return body(store);
    } finally {
      store.dispose();
    }
  }

  /// Runs [body] with a built chain tree and frees it, hooks included, after.
  R withTree<R>(TreeNode root, R Function(ChainTree tree) body) {
    final tree = buildChainTree(root);
    try {
      return body(tree);
    } finally {
      tree.free();
    }
  }
}
