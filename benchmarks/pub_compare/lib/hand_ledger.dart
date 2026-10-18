import 'dart:typed_data';

import 'package:dart_merkle_lib/dart_merkle_lib.dart';
import 'package:ed25519_edwards/ed25519_edwards.dart' as ed;
import 'package:hashlib/hashlib.dart' as hl;

import 'signing.dart';

/// A signed transfer as the hand built ledger keeps it.
typedef SignedTransfer = ({Uint8List body, Uint8List sig, ed.PublicKey from});

/// The ledger a Dart developer would assemble from pub.dev packages:
/// ed25519_edwards signatures, nonce and balance checks in maps, BLAKE2b
/// transaction ids and dart_merkle_lib roots over transactions and state.
/// It has no consensus: no second validation and no certificate.
final class HandLedger {
  /// Funds every key in [keys].
  HandLedger(this.keys) {
    for (final k in keys) {
      _balances[_id(k.publicKey)] = 1000000;
      _nonces[_id(k.publicKey)] = 0;
    }
  }

  /// The accounts, each a key pair.
  final List<ed.KeyPair> keys;
  final _balances = <String, int>{};
  final _nonces = <String, int>{};
  final _pending = <SignedTransfer>[];

  /// One signed transfer of 1 from every account to the first, at the
  /// account's next nonce.
  List<SignedTransfer> signAll() {
    final to = Uint8List.fromList(keys.first.publicKey.bytes);
    return [
      for (final k in keys)
        _sign(k, transferBody(_bytes(k), to, 1, _nonces[_id(k.publicKey)]!)),
    ];
  }

  /// Verifies [t] and queues it; false when the signature, nonce or balance
  /// is wrong.
  bool admit(SignedTransfer t) {
    if (!ed.verify(t.from, t.body, t.sig)) return false;
    final view = ByteData.sublistView(t.body);
    final id = _id(t.from);
    if (view.getUint64(72) != _nonces[id]) return false;
    if (_balances[id]! < view.getUint64(64)) return false;
    _pending.add(t);
    return true;
  }

  /// Applies the queue and returns the transaction root and state root.
  (Uint8List, Uint8List) produceBlock() {
    final txids = <Uint8List>[];
    for (final t in _pending) {
      final view = ByteData.sublistView(t.body);
      final amount = view.getUint64(64);
      final from = _id(t.from);
      final to = String.fromCharCodes(t.body.sublist(32, 64));
      _balances[from] = _balances[from]! - amount;
      _balances[to] = (_balances[to] ?? 0) + amount;
      _nonces[from] = _nonces[from]! + 1;
      txids.add(_h(Uint8List.fromList([...t.body, ...t.sig])));
    }
    _pending.clear();
    final ids = _balances.keys.toList()..sort();
    final leaves = [
      for (final id in ids)
        _h(
          Uint8List(48)
            ..setAll(0, id.codeUnits)
            ..buffer.asByteData().setUint64(32, _balances[id]!)
            ..buffer.asByteData().setUint64(40, _nonces[id] ?? 0),
        ),
    ];
    return (fastRoot(txids, _h), fastRoot(leaves, _h));
  }

  SignedTransfer _sign(ed.KeyPair k, Uint8List body) =>
      (body: body, sig: ed.sign(k.privateKey, body), from: k.publicKey);

  static Uint8List _bytes(ed.KeyPair k) =>
      Uint8List.fromList(k.publicKey.bytes);

  static String _id(ed.PublicKey k) => String.fromCharCodes(k.bytes);

  static Uint8List _h(Uint8List d) => hl.blake2b256.convert(d).bytes;
}
