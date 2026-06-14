/// dart:ffi binding for the Blockchainer C ABI.
///
/// This is a thin marshaling layer (ARCHITECTURE §0): it never computes
/// consensus bytes itself — it hands data to the C++ core and returns what the
/// core produced. All hashing happens in C++.
library;

import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

// ── C struct layouts (must match bridge/blockchainer_c.h) ────────────────────

final class _Buffer extends Struct {
  external Pointer<Uint8> data;
  @Size()
  external int len;
}

final class _BufferResult extends Struct {
  external _Buffer buf;
  external Pointer<Utf8> error;
}

// ── Native function typedefs ─────────────────────────────────────────────────

typedef _InitC = Int32 Function();
typedef _InitD = int Function();

typedef _VersionC = Pointer<Utf8> Function();
typedef _VersionD = Pointer<Utf8> Function();

typedef _HashC = _BufferResult Function(Pointer<Uint8>, Size);
typedef _HashD = _BufferResult Function(Pointer<Uint8>, int);

typedef _TxidC = _BufferResult Function(Uint8, Uint64, Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _TxidD = _BufferResult Function(
    int, int, Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);

typedef _HeaderHashC = _BufferResult Function(Uint8, Uint64, Uint64,
    Pointer<Uint8>, Uint64, Pointer<Uint8>, Pointer<Uint8>, Pointer<Uint8>, Size);
typedef _HeaderHashD = _BufferResult Function(int, int, int, Pointer<Uint8>,
    int, Pointer<Uint8>, Pointer<Uint8>, Pointer<Uint8>, int);

typedef _VerifyProofC = Int32 Function(Pointer<Uint8>, Pointer<Uint8>, Size);
typedef _VerifyProofD = int Function(Pointer<Uint8>, Pointer<Uint8>, int);

typedef _BufferFreeC = Void Function(Pointer<_Buffer>);
typedef _BufferFreeD = void Function(Pointer<_Buffer>);

typedef _StrFreeC = Void Function(Pointer<Utf8>);
typedef _StrFreeD = void Function(Pointer<Utf8>);

/// Thrown when a C ABI call reports an error.
class BlockchainerException implements Exception {
  BlockchainerException(this.message);
  final String message;
  @override
  String toString() => 'BlockchainerException: $message';
}

/// Loads and exposes the Blockchainer C ABI.
class Blockchainer {
  Blockchainer._(this._lib)
      : _init = _lib.lookupFunction<_InitC, _InitD>('blockchainer_init'),
        _version = _lib
            .lookupFunction<_VersionC, _VersionD>('blockchainer_version'),
        _backendVersion = _lib.lookupFunction<_VersionC, _VersionD>(
            'blockchainer_crypto_backend_version'),
        _hash = _lib.lookupFunction<_HashC, _HashD>('blockchainer_hash'),
        _txid = _lib.lookupFunction<_TxidC, _TxidD>('blockchainer_txid'),
        _headerHash = _lib.lookupFunction<_HeaderHashC, _HeaderHashD>(
            'blockchainer_header_hash'),
        _verifyStateProof = _lib.lookupFunction<_VerifyProofC, _VerifyProofD>(
            'blockchainer_verify_state_proof'),
        _verifyTxInclusion = _lib.lookupFunction<_VerifyProofC, _VerifyProofD>(
            'blockchainer_verify_tx_inclusion'),
        _bufferFree = _lib.lookupFunction<_BufferFreeC, _BufferFreeD>(
            'blockchainer_buffer_free'),
        _strFree =
            _lib.lookupFunction<_StrFreeC, _StrFreeD>('blockchainer_str_free');

  final DynamicLibrary _lib;
  final _InitD _init;
  final _VersionD _version;
  final _VersionD _backendVersion;
  final _HashD _hash;
  final _TxidD _txid;
  final _HeaderHashD _headerHash;
  final _VerifyProofD _verifyStateProof;
  final _VerifyProofD _verifyTxInclusion;
  final _BufferFreeD _bufferFree;
  final _StrFreeD _strFree;

  /// Loads `libblockchainer_c` from [path] (or the process image if null).
  factory Blockchainer.load([String? path]) {
    final lib = path != null
        ? DynamicLibrary.open(path)
        : DynamicLibrary.process();
    return Blockchainer._(lib);
  }

  /// Initializes the crypto backend. Throws on failure.
  void init() {
    if (_init() != 0) {
      throw BlockchainerException('blockchainer_init failed');
    }
  }

  String version() => _version().toDartString();

  String cryptoBackendVersion() => _backendVersion().toDartString();

  /// BLAKE3-256 of [msg].
  Uint8List hash(Uint8List msg) {
    final pm = _toC(msg);
    try {
      return _take(_hash(pm, msg.length), 'hash');
    } finally {
      calloc.free(pm);
    }
  }

  /// Canonical transaction id (32 bytes) for the given fields.
  Uint8List txid({
    required int version,
    required int chainId,
    required Uint8List body,
    required Uint8List author,
    required Uint8List signature,
  }) {
    final pb = _toC(body);
    final pa = _toC(author);
    final ps = _toC(signature);
    try {
      return _take(
          _txid(version, chainId, pb, body.length, pa, author.length, ps,
              signature.length),
          'txid');
    } finally {
      calloc.free(pb);
      calloc.free(pa);
      calloc.free(ps);
    }
  }

  /// Canonical block-header hash (32 bytes). [parentHash], [txRoot] and
  /// [stateRoot] must each be 32 bytes.
  Uint8List headerHash({
    required int version,
    required int chainId,
    required int height,
    required Uint8List parentHash,
    required int timestamp,
    required Uint8List txRoot,
    required Uint8List stateRoot,
    required Uint8List consensusData,
  }) {
    final pp = _toC(parentHash);
    final pt = _toC(txRoot);
    final pst = _toC(stateRoot);
    final pc = _toC(consensusData);
    try {
      return _take(
          _headerHash(version, chainId, height, pp, timestamp, pt, pst, pc,
              consensusData.length),
          'header_hash');
    } finally {
      calloc.free(pp);
      calloc.free(pt);
      calloc.free(pst);
      calloc.free(pc);
    }
  }

  /// Verify a state-inclusion proof against a 32-byte [stateRoot].
  bool verifyStateProof(Uint8List stateRoot, Uint8List proof) {
    final pr = _toC(stateRoot);
    final pp = _toC(proof);
    try {
      return _verifyStateProof(pr, pp, proof.length) == 1;
    } finally {
      calloc.free(pr);
      calloc.free(pp);
    }
  }

  /// Verify a transaction-inclusion proof against a 32-byte [txRoot].
  bool verifyTxInclusion(Uint8List txRoot, Uint8List proof) {
    final pr = _toC(txRoot);
    final pp = _toC(proof);
    try {
      return _verifyTxInclusion(pr, pp, proof.length) == 1;
    } finally {
      calloc.free(pr);
      calloc.free(pp);
    }
  }

  // Allocate native memory holding [data]; empty input still yields a valid
  // (1-byte) pointer so the borrowed-input contract holds. Caller frees.
  Pointer<Uint8> _toC(Uint8List data) {
    final p = calloc<Uint8>(data.isEmpty ? 1 : data.length);
    if (data.isNotEmpty) p.asTypedList(data.length).setAll(0, data);
    return p;
  }

  // Extract a returned buffer into a Dart list, freeing it via the C ABI.
  Uint8List _take(_BufferResult res, String op) {
    if (res.buf.data == nullptr) {
      final err = res.error == nullptr
          ? 'blockchainer_$op failed'
          : res.error.toDartString();
      if (res.error != nullptr) _strFree(res.error);
      throw BlockchainerException(err);
    }
    final out = Uint8List.fromList(res.buf.data.asTypedList(res.buf.len));
    final tmp = calloc<_Buffer>();
    tmp.ref.data = res.buf.data;
    tmp.ref.len = res.buf.len;
    _bufferFree(tmp);
    calloc.free(tmp);
    return out;
  }
}
