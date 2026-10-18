part of '../../aviary.dart';

/// A peer record's fields, read back from its encoding.
/// {@category Networking}
typedef PeerRecordInfo = ({
  int id,
  String host,
  int port,
  Uint8List kemPublic,
  Uint8List sigPublic,
  Uint8List prekey,
});

/// Self signed peer records and the address books that collect them.
/// {@category Networking}
extension AviaryPeering on Aviary {
  /// A record for validator [id] signed with [identity]'s hybrid signature
  /// key; [prekey] is the optional ratchet prekey.
  Uint8List signPeerRecord(
    int id,
    String host,
    int port,
    SecureIdentity identity, {
    Uint8List? prekey,
  }) {
    final pre = prekey ?? Uint8List(0);
    final k = identity.kem.public, s = identity.sig;
    final f = lib
        .fn<_PeerSignC>('aviary_peer_record_sign')
        .asFunction<_PeerSignD>();
    final keys = [k, s.public, pre, s.secret];
    final n = keys.map((b) => b.length).toList();
    return _withUtf8(
      host,
      (h) => _scoped(keys, (p) {
        final r = f(
          id,
          h,
          port,
          p[0],
          n[0],
          p[1],
          n[1],
          p[2],
          n[2],
          p[3],
          n[3],
        );
        return _take(r, 'peer_record_sign');
      }),
    );
  }

  /// Whether [record] decodes and its owner signature verifies.
  bool verifyPeerRecord(Uint8List record) => _scoped(
    [record],
    (p) =>
        lib
            .fn<_BytesIntC>('aviary_peer_record_verify')
            .asFunction<_BytesIntD>()(
          p[0],
          record.length,
        ) ==
        1,
  );

  /// The fields of an encoded record.
  PeerRecordInfo peerRecordInfo(Uint8List record) {
    final f = _splitBlobList(
      _scoped(
        [record],
        (p) => _take(
          lib
              .fn<_BufOfBytesC>('aviary_peer_record_fields')
              .asFunction<_BufOfBytesD>()(p[0], record.length),
          'peer_record_fields',
        ),
      ),
    );
    return _peerRecordInfo(f);
  }

  /// An empty address book, or one decoded from [encoded] (records are
  /// verified only when merged). Throws on a malformed encoding.
  AddressBook newAddressBook([Uint8List? encoded]) {
    if (encoded == null) {
      return AddressBook._(
        this,
        lib.fn<_VNoArgC>('aviary_address_book_new').asFunction<_VNoArgC>()(),
      );
    }
    final h = _scoped(
      [encoded],
      (p) => lib
          .fn<_VOfBytesC>('aviary_address_book_decode')
          .asFunction<_VOfBytesD>()(p[0], encoded.length),
    );
    return AddressBook._(this, _requireHandle(h, 'address book decode'));
  }
}
