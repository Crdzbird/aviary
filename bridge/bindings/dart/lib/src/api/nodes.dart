part of '../../aviary.dart';

/// Single validator chain nodes and custodial key stores.
extension AviaryNodes on Aviary {
  /// Creates an in memory single validator chain.
  Node createNode(int chainId) {
    final h =
        lib.lookupFunction<_NodeNewC, _NodeNewD>('aviary_node_new')(chainId);
    return Node._(this, _requireHandle(h, 'node creation'));
  }

  /// Alias of [createNode].
  Node newNode(int chainId) => createNode(chainId);

  /// Creates a custodial key store protected by [passphrase], unlocked.
  KeyStore createKeyStore(String passphrase) => _withUtf8(passphrase, (cp) {
        final h =
            lib.lookupFunction<_KsNewC, _KsNewD>('aviary_keystore_new')(cp);
        return KeyStore._(this, _requireHandle(h, 'keystore creation'));
      });

  /// Loads a serialised key store. It comes back locked.
  KeyStore loadKeyStore(Uint8List blob) => _scoped([blob], (p) {
        final h = lib.lookupFunction<_KsLoadC, _KsLoadD>(
            'aviary_keystore_load')(p[0], blob.length);
        return KeyStore._(this, _requireHandle(h, 'keystore load'));
      });
}
