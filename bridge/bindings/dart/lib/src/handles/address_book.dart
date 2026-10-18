part of '../../aviary.dart';

/// The peers a node knows: self signed records saying where each validator
/// is reached and which keys authenticate it. [add] is the trusted local
/// path; [merge] is the peer exchange boundary, which verifies every record
/// and never overwrites a known id. Encoded books are canonical, so two nodes
/// that learned the same peers hold byte identical books.
/// {@category Networking}
final class AddressBook extends _NativeHandle {
  AddressBook._(Aviary bc, Pointer<Void> h)
    : super(bc, h, 'aviary_address_book_free');

  /// Trusted insert of an encoded record ([AviaryPeering.signPeerRecord]).
  /// Throws with [AviaryStatus.badInput] on a malformed record.
  void add(Uint8List record) {
    final rc = _scoped(
      [record],
      (p) => _bc.lib
          .fn<_HandleBytesIntC>('aviary_address_book_add')
          .asFunction<_HandleBytesIntD>()(_h, p[0], record.length),
    );
    if (rc == 0) return;
    throw const AviaryException(
      'address book: malformed record',
      AviaryStatus.badInput,
    );
  }

  /// Learns the verified records of an encoded book that are not yet known
  /// and returns how many were added. Throws when the book does not decode.
  int merge(Uint8List encodedBook) {
    final n = _scoped(
      [encodedBook],
      (p) => _bc.lib
          .fn<_BookMergeC>('aviary_address_book_merge')
          .asFunction<_BookMergeD>()(_h, p[0], encodedBook.length),
    );
    if (n >= 0) return n;
    throw const AviaryException(
      'address book: malformed book',
      AviaryStatus.badInput,
    );
  }

  /// The encoded record for [id], or null when the id is unknown.
  Uint8List? record(int id) {
    if (!contains(id)) return null;
    return _bc._take(
      _bc.lib
          .fn<_BookIdBufC>('aviary_address_book_get')
          .asFunction<_BookIdBufD>()(_h, id),
      'address_book_get',
    );
  }

  /// Whether [id] is known.
  bool contains(int id) =>
      _bc.lib
          .fn<_BookIdIntC>('aviary_address_book_contains')
          .asFunction<_BookIdIntD>()(_h, id) ==
      1;

  /// How many peers are known.
  int get length => _bc.lib
      .fn<_U64OfHandleC>('aviary_address_book_size')
      .asFunction<_U64OfHandleD>()(_h);

  /// Every encoded record in ascending id order.
  List<Uint8List> records() =>
      _splitBlobList(_buf('aviary_address_book_records'));

  /// The canonical encoding, to send to a peer or persist.
  Uint8List encode() => _buf('aviary_address_book_encode');

  Uint8List _buf(String fn) => _bc._take(
    _bc.lib.fn<_BufOfHandleC>(fn).asFunction<_BufOfHandleD>()(_h),
    fn,
  );

  /// Releases the native book.
  void free() => _release();
}
