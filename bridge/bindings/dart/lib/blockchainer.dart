/// High-level Dart API over the Blockchainer C ABI — the surface the example
/// Flutter app drives. Wraps the full node (create a chain, accounts, transfer,
/// produce blocks), streaming events, observability metrics, light-client
/// verification, and multi-chain interop address derivation.
library;

import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

// ── C structs ────────────────────────────────────────────────────────────────

final class _Buffer extends Struct {
  external Pointer<Uint8> data;
  @Size()
  external int len;
}

final class _BufResult extends Struct {
  external _Buffer buf;
  external Pointer<Utf8> error;
}

// ── Native typedefs ──────────────────────────────────────────────────────────

typedef _IntVoid = Int32 Function();
typedef _PtrVoid = Pointer<Utf8> Function();
typedef _NodeNewC = Pointer<Void> Function(Uint64);
typedef _NodeNewD = Pointer<Void> Function(int);
typedef _NodeFreeC = Void Function(Pointer<Void>);
typedef _NodeFreeD = void Function(Pointer<Void>);
typedef _BufOfNodeC = _BufResult Function(Pointer<Void>);
typedef _BufOfNodeD = _BufResult Function(Pointer<Void>);
typedef _BufOfBytesC = _BufResult Function(Pointer<Uint8>, Size);
typedef _BufOfBytesD = _BufResult Function(Pointer<Uint8>, int);
typedef _BufOfPtrC = _BufResult Function(Pointer<Uint8>);
typedef _BufOfPtrD = _BufResult Function(Pointer<Uint8>);
typedef _CreditC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Uint64);
typedef _CreditD = int Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _SubmitC = _BufResult Function(
    Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Uint64, Uint64);
typedef _SubmitD = _BufResult Function(
    Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int, int);
typedef _SubmitFeeC = _BufResult Function(
    Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Uint64, Uint64, Uint64);
typedef _SubmitFeeD = _BufResult Function(
    Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int, int, int);
typedef _KsNewC = Pointer<Void> Function(Pointer<Utf8>);
typedef _KsNewD = Pointer<Void> Function(Pointer<Utf8>);
typedef _KsAddC = Int32 Function(
    Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>);
typedef _KsAddD = int Function(Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>);
typedef _KsLoadC = Pointer<Void> Function(Pointer<Uint8>, Size);
typedef _KsLoadD = Pointer<Void> Function(Pointer<Uint8>, int);
typedef _KsUnlockC = Int32 Function(Pointer<Void>, Pointer<Utf8>);
typedef _KsUnlockD = int Function(Pointer<Void>, Pointer<Utf8>);
typedef _IntOfPtrC = Int32 Function(Pointer<Void>);
typedef _IntOfPtrD = int Function(Pointer<Void>);
typedef _SubmitHybridC = _BufResult Function(Pointer<Void>, Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Uint64, Uint64);
typedef _SubmitHybridD = _BufResult Function(Pointer<Void>, Pointer<Uint8>, int,
    Pointer<Uint8>, int, Pointer<Uint8>, int, int);
typedef _ProduceC = Int32 Function(Pointer<Void>, Uint64, Pointer<Uint64>);
typedef _ProduceD = int Function(Pointer<Void>, int, Pointer<Uint64>);
typedef _U64NodeC = Uint64 Function(Pointer<Void>);
typedef _U64NodeD = int Function(Pointer<Void>);
typedef _U64NodeAddrC = Uint64 Function(Pointer<Void>, Pointer<Uint8>);
typedef _U64NodeAddrD = int Function(Pointer<Void>, Pointer<Uint8>);
typedef _BufNodeAddrC = _BufResult Function(Pointer<Void>, Pointer<Uint8>);
typedef _BufNodeAddrD = _BufResult Function(Pointer<Void>, Pointer<Uint8>);
typedef _VerifyC = Int32 Function(Pointer<Uint8>, Pointer<Uint8>, Size);
typedef _VerifyD = int Function(Pointer<Uint8>, Pointer<Uint8>, int);
typedef _TxidC = _BufResult Function(Uint8, Uint64, Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _TxidD = _BufResult Function(int, int, Pointer<Uint8>, int,
    Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _HeaderC = _BufResult Function(Uint8, Uint64, Uint64, Pointer<Uint8>,
    Uint64, Pointer<Uint8>, Pointer<Uint8>, Pointer<Uint8>, Size);
typedef _HeaderD = _BufResult Function(int, int, int, Pointer<Uint8>, int,
    Pointer<Uint8>, Pointer<Uint8>, Pointer<Uint8>, int);
typedef _VerifyCommitC = Int32 Function(
    Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _VerifyCommitD = int Function(
    int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _Erc20C = _BufResult Function(Pointer<Uint8>, Uint64);
typedef _Erc20D = _BufResult Function(Pointer<Uint8>, int);
typedef _SelectorC = _BufResult Function(Pointer<Utf8>);
typedef _SelectorD = _BufResult Function(Pointer<Utf8>);
typedef _SignLegacyC = _BufResult Function(Uint64, Uint64, Uint64, Pointer<Uint8>,
    Int32, Uint64, Pointer<Uint8>, Size, Uint64, Pointer<Uint8>);
typedef _SignLegacyD = _BufResult Function(int, int, int, Pointer<Uint8>, int,
    int, Pointer<Uint8>, int, int, Pointer<Uint8>);
typedef _BufFreeC = Void Function(Pointer<_Buffer>);
typedef _BufFreeD = void Function(Pointer<_Buffer>);
typedef _StrFreeC = Void Function(Pointer<Utf8>);
typedef _StrFreeD = void Function(Pointer<Utf8>);

// Portable account export.
typedef _KemKeygenC = _BufResult Function();
typedef _KemKeygenD = _BufResult Function();
typedef _PSealPassC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Utf8>, Uint64, Size);
typedef _PSealPassD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Utf8>, int, int);
typedef _POpenPassC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Utf8>);
typedef _POpenPassD = _BufResult Function(Pointer<Uint8>, int, Pointer<Utf8>);
typedef _PSealToC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _PSealToD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _POpenToC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _POpenToD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _PSealSignedC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _PSealSignedD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _POpenSignedC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _POpenSignedD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);

// Threshold (FROST t-of-n).
typedef _ThCreateC = _BufResult Function(Uint16, Uint16);
typedef _ThCreateD = _BufResult Function(int, int);
typedef _ThBufC = _BufResult Function(Pointer<Uint8>, Size); // info + commit
typedef _ThBufD = _BufResult Function(Pointer<Uint8>, int);
typedef _ThSealC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Utf8>, Uint64, Size);
typedef _ThSealD = _BufResult Function(Pointer<Uint8>, int, Pointer<Utf8>, int, int);
typedef _ThUnsealC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Utf8>);
typedef _ThUnsealD = _BufResult Function(Pointer<Uint8>, int, Pointer<Utf8>);
typedef _ThCerNewC = Pointer<Void> Function(
    Pointer<Uint8>, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ThCerNewD = Pointer<Void> Function(
    Pointer<Uint8>, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _ThCerFreeC = Void Function(Pointer<Void>);
typedef _ThCerFreeD = void Function(Pointer<Void>);
typedef _ThSignC = _BufResult Function(Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>);
typedef _ThSignD = _BufResult Function(Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>);
typedef _ThVerifyC = Int32 Function(Pointer<Void>, Uint16, Pointer<Uint8>, Pointer<Uint8>);
typedef _ThVerifyD = int Function(Pointer<Void>, int, Pointer<Uint8>, Pointer<Uint8>);
typedef _ThAggC = _BufResult Function(Pointer<Void>, Pointer<Uint16>, Pointer<Uint8>, Size);
typedef _ThAggD = _BufResult Function(Pointer<Void>, Pointer<Uint16>, Pointer<Uint8>, int);

// AssuranceProfile + SealedMempool.
typedef _AsDescribeC = _BufResult Function(Uint16);
typedef _AsDescribeD = _BufResult Function(int);
typedef _TxEncodeC = _BufResult Function(Uint8, Uint64, Uint8, Uint64, Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _TxEncodeD = _BufResult Function(int, int, int, int, Pointer<Uint8>, int, Pointer<Uint8>,
    int, Pointer<Uint8>, int);
typedef _SmSealC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _SmSealD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _SmNewC = Pointer<Void> Function(Size);
typedef _SmNewD = Pointer<Void> Function(int);
typedef _SmAddC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size);
typedef _SmAddD = int Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _SmSizeC = Size Function(Pointer<Void>);
typedef _SmSizeD = int Function(Pointer<Void>);
typedef _SmRevealC = _BufResult Function(Pointer<Void>, Pointer<Uint8>, Size, Size);
typedef _SmRevealD = _BufResult Function(Pointer<Void>, Pointer<Uint8>, int, int);
typedef _SmFreeC = Void Function(Pointer<Void>);
typedef _SmFreeD = void Function(Pointer<Void>);

// CredentialGate (anonymous membership admission, Option A).
typedef _BbsKeygenC = _BufResult Function(Pointer<Uint8>, Size);
typedef _BbsKeygenD = _BufResult Function(Pointer<Uint8>, int);
typedef _CredPolicyEncodeC = _BufResult Function(
    Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint32, Uint8);
typedef _CredPolicyEncodeD = _BufResult Function(
    int, Pointer<Uint8>, int, Pointer<Uint8>, int, int, int);
typedef _CredPolicyHashC = _BufResult Function(Pointer<Uint8>, Size);
typedef _CredPolicyHashD = _BufResult Function(Pointer<Uint8>, int);
typedef _CredIssueC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint8, Uint64, Uint32);
typedef _CredIssueD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int, int, int, int);
typedef _CredPresentC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint32, Pointer<Utf8>, Pointer<Uint8>, Size);
typedef _CredPresentD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, int, Pointer<Utf8>, Pointer<Uint8>, int);
typedef _CredGateNewC = Pointer<Void> Function(Pointer<Uint8>, Size);
typedef _CredGateNewD = Pointer<Void> Function(Pointer<Uint8>, int);
typedef _CredGateRevokeC = Void Function(Pointer<Void>, Uint32);
typedef _CredGateRevokeD = void Function(Pointer<Void>, int);
typedef _CredGateAdmitC = Int32 Function(
    Pointer<Void>, Uint32, Pointer<Utf8>, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint64);
typedef _CredGateAdmitD = int Function(
    Pointer<Void>, int, Pointer<Utf8>, Pointer<Uint8>, int, Pointer<Uint8>, int, int);
typedef _CredGateFreeC = Void Function(Pointer<Void>);
typedef _CredGateFreeD = void Function(Pointer<Void>);

class BlockchainerException implements Exception {
  BlockchainerException(this.message);
  final String message;
  @override
  String toString() => 'BlockchainerException: $message';
}

/// A generated account: 64-byte Ed25519 secret, 32-byte public key, 32-byte
/// chain address (BLAKE3 of the public key).
class Account {
  Account(this.secret, this.publicKey, this.address);
  final Uint8List secret;
  final Uint8List publicKey;
  final Uint8List address;
}

/// The loaded library + top-level (stateless) operations.
class Blockchainer {
  Blockchainer._(this.lib);

  final DynamicLibrary lib;

  factory Blockchainer.open(String path) => Blockchainer.fromLibrary(DynamicLibrary.open(path));

  /// Build from an already-resolved library — e.g. `DynamicLibrary.process()` on
  /// iOS/macOS, where the native code is statically linked into the app.
  factory Blockchainer.fromLibrary(DynamicLibrary lib) {
    final bc = Blockchainer._(lib);
    // blockchainer_init → cryptolib_init, which is idempotent (safe to call
    // again from a preloaded isolate or on every [instance] access).
    if (lib.lookupFunction<_IntVoid, int Function()>('blockchainer_init')() != 0) {
      throw BlockchainerException('init failed');
    }
    return bc;
  }

  /// Open the native library the platform-appropriate way.
  ///
  /// With no [path] the platform default is used:
  ///   • iOS / macOS → the process image (the embedded dynamic framework's
  ///     symbols are loaded at launch — Blockchainer links cryptolib at runtime)
  ///   • Android / Linux → `libblockchainer_c.so` (which NEEDs
  ///     `libcryptolib_c.so`, provided by the cryptolib package)
  ///   • Windows → `blockchainer_c.dll`
  /// Set `BLOCKCHAINER_DYLIB` to dlopen a specific file (desktop dev / tests).
  factory Blockchainer.load([String? path]) {
    final resolved = (path != null && path.isNotEmpty)
        ? path
        : Platform.environment['BLOCKCHAINER_DYLIB'];
    final DynamicLibrary lib;
    if (resolved != null && resolved.isNotEmpty) {
      lib = DynamicLibrary.open(resolved);
    } else if (Platform.isIOS || Platform.isMacOS) {
      lib = DynamicLibrary.process();
    } else if (Platform.isAndroid || Platform.isLinux) {
      lib = DynamicLibrary.open('libblockchainer_c.so');
    } else if (Platform.isWindows) {
      lib = DynamicLibrary.open('blockchainer_c.dll');
    } else {
      lib = DynamicLibrary.process();
    }
    return Blockchainer.fromLibrary(lib);
  }

  // ── Preload + lazy synchronous singleton ───────────────────────────────────
  //
  // The native library is loaded + initialized lazily on first use of
  // [instance] — fully synchronous, so ledger/interop calls never require
  // `await`. [preload] is an OPTIONAL warm-up that does the one-time heavy work
  // (mapping the shared libraries + crypto-backend init) on a background
  // isolate, so the first call on the UI isolate is effectively free. Calling
  // preload is not required; the API works identically whether or not you do.

  static Blockchainer? _singleton;
  static Future<bool>? _warming;

  /// Process-wide instance. On first access the native library is opened and
  /// initialized **synchronously** (no `await`). If [preload]/[ensureInitialized]
  /// ran first, the OS has already mapped the libraries and the crypto backend
  /// is initialized, so this is effectively instantaneous.
  ///
  /// In a UI app, prefer `await Blockchainer.ensureInitialized()` once at start
  /// so this getter never does the (potentially tens-of-ms) load on the UI
  /// thread. Accessing it before that still works — it just loads lazily on the
  /// calling isolate.
  static Blockchainer get instance => _singleton ??= Blockchainer.load();

  /// Whether [instance] is ready to use without any load work on this isolate.
  static bool get isInitialized => _singleton != null;

  /// Alias for [isInitialized] (naming parity with the Go binding's Initialized).
  static bool get initialized => isInitialized;

  /// Initialize the native backend **off the UI isolate** and complete when it
  /// is ready to use.
  ///
  /// This is the recommended entry point for apps: the heavy one-time work
  /// (mapping `libblockchainer_c` + `libcryptolib_c` and the crypto-backend
  /// init — which can take tens of milliseconds) runs on a background isolate
  /// via [preload], so the UI thread never stalls. When the returned Future
  /// completes, [instance] is guaranteed fast (its handle is prepared on the
  /// calling isolate, cheap now that the libraries are mapped + inited).
  ///
  /// Idempotent and concurrency-safe: call it from `main()`, a `FutureBuilder`,
  /// or several places at once — the background warm-up runs at most once.
  ///
  /// ```dart
  /// Future<void> main() async {
  ///   WidgetsFlutterBinding.ensureInitialized();
  ///   await Blockchainer.ensureInitialized(); // background load, no UI stall
  ///   runApp(const MyApp());
  /// }
  /// ```
  static Future<void> ensureInitialized() async {
    if (_singleton != null) return;
    // Warm the process (dlopen + crypto-backend init) on a background isolate,
    // de-duplicating concurrent callers. Best-effort: even if it fails, the
    // lazy load below still produces a usable instance (or throws on real init
    // failure).
    _warming ??= preload();
    await _warming;
    _singleton ??= Blockchainer.load();
  }

  /// Alias for [ensureInitialized] (naming parity with the Go binding's Init).
  static Future<void> init() => ensureInitialized();

  /// Optionally warm the native library off the UI isolate.
  ///
  /// Spawns a short-lived background isolate that opens the shared library
  /// (mapping it + `libcryptolib_c` into the process / loader cache) and runs
  /// the one-time crypto-backend init. Because the dlopen mapping and the
  /// backend init are process-global, this warms the path for the main
  /// isolate's later synchronous [instance] access.
  ///
  /// Call once near app start (e.g. in `main()`); the returned Future is
  /// fire-and-forget — you may ignore it or `await` it, but the ledger API
  /// ([instance]) never requires awaiting it. Safe to call repeatedly and safe
  /// to never call. Returns true if the warm-up completed, false if it failed
  /// (in which case the lazy synchronous path still works on first real use).
  static Future<bool> preload() async {
    try {
      return await Isolate.run<bool>(() {
        // Fresh isolate: DynamicLibrary handles are per-isolate, but the dlopen
        // mapping + crypto-backend init they trigger are process-global.
        final warm = Blockchainer.load();
        warm.version(); // touch a symbol so lazy lookups resolve here too
        return true;
      });
    } on Object catch (_) {
      return false;
    }
  }

  String version() =>
      lib.lookupFunction<_PtrVoid, _PtrVoid>('blockchainer_version')().toDartString();
  String cryptoBackend() => lib
      .lookupFunction<_PtrVoid, _PtrVoid>('blockchainer_crypto_backend_version')()
      .toDartString();

  /// Alias for [cryptoBackend] (naming parity with Go's CryptoBackendVersion).
  String cryptoBackendVersion() => cryptoBackend();

  Uint8List _take(_BufResult r, String op) {
    if (r.buf.data == nullptr) {
      final err = r.error == nullptr ? '$op failed' : r.error.toDartString();
      if (r.error != nullptr) {
        lib.lookupFunction<_StrFreeC, _StrFreeD>('blockchainer_str_free')(r.error);
      }
      throw BlockchainerException(err);
    }
    final out = Uint8List.fromList(r.buf.data.asTypedList(r.buf.len));
    final tmp = calloc<_Buffer>();
    tmp.ref.data = r.buf.data;
    tmp.ref.len = r.buf.len;
    lib.lookupFunction<_BufFreeC, _BufFreeD>('blockchainer_buffer_free')(tmp);
    calloc.free(tmp);
    return out;
  }

  String _takeStr(_BufResult r, String op) => utf8.decode(_take(r, op));

  Pointer<Uint8> _alloc(Uint8List d) {
    final p = calloc<Uint8>(d.isEmpty ? 1 : d.length);
    if (d.isNotEmpty) p.asTypedList(d.length).setAll(0, d);
    return p;
  }

  // ── Interop address derivation ─────────────────────────────────────────────
  String _addr(String fn, Uint8List secret) {
    final p = _alloc(secret);
    try {
      return _takeStr(lib.lookupFunction<_BufOfPtrC, _BufOfPtrD>(fn)(p), fn);
    } finally {
      calloc.free(p);
    }
  }

  String ethAddress(Uint8List secret32) => _addr('blockchainer_eth_address', secret32);
  String btcAddress(Uint8List secret32) => _addr('blockchainer_btc_address', secret32);
  String solAddress(Uint8List edSecret64) => _addr('blockchainer_sol_address', edSecret64);

  // ── EVM smart-contract interaction ─────────────────────────────────────────

  /// ABI-encoded calldata for ERC-20 transfer(address,uint256). [to20] is 20 bytes.
  Uint8List ethErc20TransferCalldata(Uint8List to20, int amount) {
    final p = _alloc(to20);
    try {
      return _take(
          lib.lookupFunction<_Erc20C, _Erc20D>('blockchainer_eth_erc20_transfer')(p, amount),
          'ethErc20TransferCalldata');
    } finally {
      calloc.free(p);
    }
  }

  /// ABI 4-byte function selector for a signature, e.g. "balanceOf(address)".
  Uint8List ethSelector(String signature) {
    final s = signature.toNativeUtf8();
    try {
      return _take(lib.lookupFunction<_SelectorC, _SelectorD>('blockchainer_eth_selector')(s),
          'ethSelector');
    } finally {
      calloc.free(s);
    }
  }

  /// Sign an EVM legacy (EIP-155) transaction with a 32-byte secp256k1 key.
  /// Pass [to] = null for contract creation; [data] = null for a value transfer.
  /// Returns the raw signed RLP transaction, ready for eth_sendRawTransaction.
  Uint8List ethSignLegacy({
    required int nonce,
    required int gasPrice,
    required int gasLimit,
    Uint8List? to,
    int value = 0,
    Uint8List? data,
    required int chainId,
    required Uint8List secret32,
  }) {
    final pTo = _alloc(to ?? Uint8List(0));
    final pData = _alloc(data ?? Uint8List(0));
    final pSecret = _alloc(secret32);
    try {
      return _take(
          lib.lookupFunction<_SignLegacyC, _SignLegacyD>('blockchainer_eth_sign_legacy')(
              nonce,
              gasPrice,
              gasLimit,
              pTo,
              to != null && to.length == 20 ? 1 : 0,
              value,
              pData,
              data?.length ?? 0,
              chainId,
              pSecret),
          'ethSignLegacy');
    } finally {
      calloc.free(pTo);
      calloc.free(pData);
      calloc.free(pSecret);
    }
  }

  bool verifyStateProof(Uint8List stateRoot, Uint8List proof) {
    final pr = _alloc(stateRoot);
    final pp = _alloc(proof);
    try {
      return lib.lookupFunction<_VerifyC, _VerifyD>('blockchainer_verify_state_proof')(
              pr, pp, proof.length) ==
          1;
    } finally {
      calloc.free(pr);
      calloc.free(pp);
    }
  }

  /// Verify a transaction-inclusion proof against a 32-byte txRoot.
  bool verifyTxInclusion(Uint8List txRoot, Uint8List proof) {
    final pr = _alloc(txRoot);
    final pp = _alloc(proof);
    try {
      return lib.lookupFunction<_VerifyC, _VerifyD>('blockchainer_verify_tx_inclusion')(
              pr, pp, proof.length) ==
          1;
    } finally {
      calloc.free(pr);
      calloc.free(pp);
    }
  }

  /// Verify a BFT commit certificate against an encoded validator set. Returns
  /// false for any malformed input.
  bool verifyCommit(int chainId, Uint8List validatorSet, Uint8List cert) {
    final pv = _alloc(validatorSet);
    final pc = _alloc(cert);
    try {
      return lib.lookupFunction<_VerifyCommitC, _VerifyCommitD>(
              'blockchainer_verify_commit')(
              chainId, pv, validatorSet.length, pc, cert.length) ==
          1;
    } finally {
      calloc.free(pv);
      calloc.free(pc);
    }
  }

  // ── Canonical bytes (deterministic across every binding) ───────────────────

  /// BLAKE3-256 digest of [msg] (32 bytes).
  Uint8List hash(Uint8List msg) {
    final p = _alloc(msg);
    try {
      return _take(
          lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>('blockchainer_hash')(p, msg.length),
          'hash');
    } finally {
      calloc.free(p);
    }
  }

  /// Canonical transaction id (32 bytes) for the given fields:
  /// BLAKE3-256( version ‖ canonical(tx) ). Reproducible from any binding.
  Uint8List txid(int version, int chainId, Uint8List body, Uint8List author,
      Uint8List signature) {
    final pb = _alloc(body);
    final pa = _alloc(author);
    final ps = _alloc(signature);
    try {
      return _take(
          lib.lookupFunction<_TxidC, _TxidD>('blockchainer_txid')(version, chainId, pb,
              body.length, pa, author.length, ps, signature.length),
          'txid');
    } finally {
      calloc.free(pb);
      calloc.free(pa);
      calloc.free(ps);
    }
  }

  /// Canonical block-header hash (32 bytes). [parentHash], [txRoot] and
  /// [stateRoot] must each be 32 bytes.
  Uint8List headerHash(int version, int chainId, int height, Uint8List parentHash,
      int timestamp, Uint8List txRoot, Uint8List stateRoot, Uint8List consensus) {
    final pp = _alloc(parentHash);
    final pt = _alloc(txRoot);
    final ps = _alloc(stateRoot);
    final pc = _alloc(consensus);
    try {
      return _take(
          lib.lookupFunction<_HeaderC, _HeaderD>('blockchainer_header_hash')(version,
              chainId, height, pp, timestamp, pt, ps, pc, consensus.length),
          'headerHash');
    } finally {
      calloc.free(pp);
      calloc.free(pt);
      calloc.free(ps);
      calloc.free(pc);
    }
  }

  /// Create a new in-memory single-validator chain.
  Node createNode(int chainId) {
    final h = lib.lookupFunction<_NodeNewC, _NodeNewD>('blockchainer_node_new')(chainId);
    if (h == nullptr) throw BlockchainerException('node creation failed');
    return Node._(this, h);
  }

  /// Alias for [createNode] (naming parity with the Go binding's NewNode).
  Node newNode(int chainId) => createNode(chainId);

  // Unpack the C ABI account blob [pub_len:4 BE][public][secret].
  PortableAccount _splitBlob(Uint8List blob) {
    final pl = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
    return PortableAccount(
        Uint8List.sublistView(blob, 4, 4 + pl), Uint8List.sublistView(blob, 4 + pl));
  }

  /// A hybrid-KEM device keypair (X25519 + ML-KEM-768) for the recipient-bound
  /// portable-account tiers.
  PortableAccount hybridKemKeypair() => _splitBlob(_take(
      lib.lookupFunction<_KemKeygenC, _KemKeygenD>('blockchainer_hybrid_kem_keygen')(),
      'hybrid_kem_keygen'));

  /// Tier 1 — seal an account (public + secret) under [passphrase]
  /// (MolecularVault + Argon2id; ops=mem=0 → SENSITIVE preset).
  Uint8List sealAccountPassphrase(Uint8List public, Uint8List secret, String passphrase,
      {int ops = 0, int mem = 0}) {
    final pubP = _alloc(public), secP = _alloc(secret), cp = passphrase.toNativeUtf8();
    try {
      return _take(
          lib.lookupFunction<_PSealPassC, _PSealPassD>('blockchainer_portable_seal_passphrase')(
              pubP, public.length, secP, secret.length, cp, ops, mem),
          'portable_seal_passphrase');
    } finally {
      calloc.free(pubP);
      calloc.free(secP);
      calloc.free(cp);
    }
  }

  /// Open a passphrase envelope. Throws on wrong passphrase / tamper.
  PortableAccount openAccountPassphrase(Uint8List envelope, String passphrase) {
    final envP = _alloc(envelope), cp = passphrase.toNativeUtf8();
    try {
      return _splitBlob(_take(
          lib.lookupFunction<_POpenPassC, _POpenPassD>('blockchainer_portable_open_passphrase')(
              envP, envelope.length, cp),
          'portable_open_passphrase'));
    } finally {
      calloc.free(envP);
      calloc.free(cp);
    }
  }

  /// Tier 2 — seal an account to a device's hybrid-KEM public key (recipient-
  /// bound PQ). Only that device's KEM secret opens it.
  Uint8List sealAccountTo(Uint8List public, Uint8List secret, Uint8List recipientKemPublic) {
    final pubP = _alloc(public), secP = _alloc(secret), rkP = _alloc(recipientKemPublic);
    try {
      return _take(
          lib.lookupFunction<_PSealToC, _PSealToD>('blockchainer_portable_seal_to')(
              pubP, public.length, secP, secret.length, rkP, recipientKemPublic.length),
          'portable_seal_to');
    } finally {
      calloc.free(pubP);
      calloc.free(secP);
      calloc.free(rkP);
    }
  }

  /// Open a recipient-bound envelope with the device's KEM secret.
  PortableAccount openAccountTo(Uint8List envelope, Uint8List recipientKemSecret) {
    final envP = _alloc(envelope), rkP = _alloc(recipientKemSecret);
    try {
      return _splitBlob(_take(
          lib.lookupFunction<_POpenToC, _POpenToD>('blockchainer_portable_open_to')(
              envP, envelope.length, rkP, recipientKemSecret.length),
          'portable_open_to'));
    } finally {
      calloc.free(envP);
      calloc.free(rkP);
    }
  }

  /// Tier 3 — seal to a device KEM key AND sign with the owner's hybrid signature
  /// secret (Fortress-grade: recipient-bound + authenticated).
  Uint8List sealAccountSignedTo(Uint8List public, Uint8List secret,
      Uint8List recipientKemPublic, Uint8List senderSigSecret) {
    final pubP = _alloc(public),
        secP = _alloc(secret),
        rkP = _alloc(recipientKemPublic),
        ssP = _alloc(senderSigSecret);
    try {
      return _take(
          lib.lookupFunction<_PSealSignedC, _PSealSignedD>(
              'blockchainer_portable_seal_signed_to')(pubP, public.length, secP, secret.length,
              rkP, recipientKemPublic.length, ssP, senderSigSecret.length),
          'portable_seal_signed_to');
    } finally {
      calloc.free(pubP);
      calloc.free(secP);
      calloc.free(rkP);
      calloc.free(ssP);
    }
  }

  /// Open a signed-recipient envelope: decrypt with the device KEM secret AND
  /// verify the owner's signature. Throws otherwise.
  PortableAccount openAccountSignedFrom(
      Uint8List envelope, Uint8List recipientKemSecret, Uint8List senderSigPublic) {
    final envP = _alloc(envelope), rkP = _alloc(recipientKemSecret), spP = _alloc(senderSigPublic);
    try {
      return _splitBlob(_take(
          lib.lookupFunction<_POpenSignedC, _POpenSignedD>(
              'blockchainer_portable_open_signed_from')(
              envP, envelope.length, rkP, recipientKemSecret.length, spP, senderSigPublic.length),
          'portable_open_signed_from'));
    } finally {
      calloc.free(envP);
      calloc.free(rkP);
      calloc.free(spP);
    }
  }

  // ── Threshold (FROST t-of-n) accounts ──────────────────────────────────────

  /// Deal a t-of-n account: [n] share cards, any [t] of which can sign. The dealt
  /// group secret is zeroised before returning.
  List<ThresholdShare> createThresholdAccount(int n, int t) {
    final blob = _take(
        lib.lookupFunction<_ThCreateC, _ThCreateD>('blockchainer_threshold_create')(n, t),
        'threshold_create');
    final count = (blob[0] << 8) | blob[1];
    final out = <ThresholdShare>[];
    var off = 2;
    for (var i = 0; i < count; i++) {
      final len = (blob[off] << 24) | (blob[off + 1] << 16) | (blob[off + 2] << 8) | blob[off + 3];
      off += 4;
      final card = Uint8List.sublistView(blob, off, off + len);
      off += len;
      out.add(_shareInfo(card));
    }
    return out;
  }

  ThresholdShare _shareInfo(Uint8List card) {
    final cP = _alloc(card);
    try {
      final info = _take(
          lib.lookupFunction<_ThBufC, _ThBufD>('blockchainer_threshold_share_info')(cP, card.length),
          'threshold_share_info');
      return ThresholdShare(card, (info[0] << 8) | info[1],
          Uint8List.sublistView(info, 2, 34), Uint8List.sublistView(info, 34, 66));
    } finally {
      calloc.free(cP);
    }
  }

  /// Seal a share card for rest (ops=mem=0 → SENSITIVE preset).
  Uint8List sealShare(Uint8List card, String passphrase, {int ops = 0, int mem = 0}) {
    final cP = _alloc(card), pp = passphrase.toNativeUtf8();
    try {
      return _take(
          lib.lookupFunction<_ThSealC, _ThSealD>('blockchainer_threshold_share_seal')(
              cP, card.length, pp, ops, mem),
          'threshold_share_seal');
    } finally {
      calloc.free(cP);
      calloc.free(pp);
    }
  }

  /// Open a sealed share card. Throws on wrong passphrase / tamper.
  Uint8List unsealShare(Uint8List envelope, String passphrase) {
    final eP = _alloc(envelope), pp = passphrase.toNativeUtf8();
    try {
      return _take(
          lib.lookupFunction<_ThUnsealC, _ThUnsealD>('blockchainer_threshold_share_unseal')(
              eP, envelope.length, pp),
          'threshold_share_unseal');
    } finally {
      calloc.free(eP);
      calloc.free(pp);
    }
  }

  /// Round 1 for a card: 130 bytes = [nonces 64][commitment 66]. Keep the nonces
  /// secret; broadcast the commitment to the coordinator.
  ({Uint8List nonces, Uint8List commitment}) commit(Uint8List card) {
    final cP = _alloc(card);
    try {
      final b = _take(
          lib.lookupFunction<_ThBufC, _ThBufD>('blockchainer_threshold_commit')(cP, card.length),
          'threshold_commit');
      return (nonces: Uint8List.sublistView(b, 0, 64), commitment: Uint8List.sublistView(b, 64, 130));
    } finally {
      calloc.free(cP);
    }
  }

  /// Open a two-round signing [Ceremony] over [message] and the participating
  /// [commitments] (66 bytes each).
  Ceremony makeCeremony(Uint8List groupPublicKey, Uint8List message, List<Uint8List> commitments) {
    final roster = BytesBuilder();
    for (final c in commitments) roster.add(c);
    final rosterBytes = roster.toBytes();
    final gP = _alloc(groupPublicKey), mP = _alloc(message), rP = _alloc(rosterBytes);
    try {
      final h = lib.lookupFunction<_ThCerNewC, _ThCerNewD>('blockchainer_threshold_ceremony_new')(
          gP, mP, message.length, rP, rosterBytes.length);
      if (h == nullptr) throw BlockchainerException('threshold ceremony creation failed');
      return Ceremony._(this, h);
    } finally {
      calloc.free(gP);
      calloc.free(mP);
      calloc.free(rP);
    }
  }

  // ── AssuranceProfile + SealedMempool ───────────────────────────────────────

  /// Describe the security posture at [level] (1=standard, 2=hardened, 3=fortress).
  AssuranceProfile describeAssurance(int level) {
    final b = _take(
        lib.lookupFunction<_AsDescribeC, _AsDescribeD>('blockchainer_assurance_describe')(level),
        'assurance_describe');
    // [transport u8][consensus u8][threshold_custody u8][sealed_mempool u8][algo_count u8][algos...]
    return AssuranceProfile(
      ratcheted: b[0] == 1,
      vrf: b[1] == 1,
      thresholdCustody: b[2] == 1,
      sealedMempool: b[3] == 1,
      sigAlgos: Uint8List.sublistView(b, 5, 5 + b[4]),
    );
  }

  /// Canonically encode a [Transaction] from its fields (for sealing into a mempool).
  Uint8List encodeTx({
    int version = 2,
    required int chainId,
    int sigAlgo = 1,
    int fee = 0,
    required Uint8List body,
    required Uint8List author,
    required Uint8List signature,
  }) {
    final bP = _alloc(body), aP = _alloc(author), sP = _alloc(signature);
    try {
      return _take(
          lib.lookupFunction<_TxEncodeC, _TxEncodeD>('blockchainer_tx_encode')(version, chainId,
              sigAlgo, fee, bP, body.length, aP, author.length, sP, signature.length),
          'tx_encode');
    } finally {
      calloc.free(bP);
      calloc.free(aP);
      calloc.free(sP);
    }
  }

  /// HPKE-seal an encoded transaction to a proposer's KEM public key. The result
  /// is opaque until that proposer reveals it — nobody else can read or reorder it.
  Uint8List sealTx(Uint8List txEncoded, Uint8List proposerKemPublic) {
    final tP = _alloc(txEncoded), pP = _alloc(proposerKemPublic);
    try {
      return _take(
          lib.lookupFunction<_SmSealC, _SmSealD>('blockchainer_sealed_tx_seal')(
              tP, txEncoded.length, pP, proposerKemPublic.length),
          'sealed_tx_seal');
    } finally {
      calloc.free(tP);
      calloc.free(pP);
    }
  }

  /// Open an anti-MEV [SealedMempool] with room for [capacity] sealed transactions.
  SealedMempool makeSealedMempool(int capacity) {
    final h = lib.lookupFunction<_SmNewC, _SmNewD>('blockchainer_sealed_mempool_new')(capacity);
    if (h == nullptr) throw BlockchainerException('sealed mempool creation failed');
    return SealedMempool._(this, h);
  }

  // ── CredentialGate (anonymous membership admission, Option A) ────────────────

  /// A BBS issuer keypair from [keyMaterial] (>= 32 bytes) — the chain's issuer
  /// identity. Returns (public 96 B, secret 32 B).
  ({Uint8List public, Uint8List secret}) newBBSIssuer(Uint8List keyMaterial) {
    final p = _alloc(keyMaterial);
    try {
      final blob = _take(
          lib.lookupFunction<_BbsKeygenC, _BbsKeygenD>('blockchainer_bbs_keygen')(p, keyMaterial.length),
          'bbs_keygen');
      final pl = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
      return (public: Uint8List.sublistView(blob, 4, 4 + pl), secret: Uint8List.sublistView(blob, 4 + pl));
    } finally {
      calloc.free(p);
    }
  }

  /// Canonically encode a membership [CredentialPolicy] (attributeCount must be
  /// 4). Its [CredentialPolicy.hash] can be committed into genesis.
  CredentialPolicy makeCredentialPolicy(
      {required int chainId,
      required Uint8List issuerPublic,
      required Uint8List header,
      int attributeCount = 4,
      int minTier = 0}) {
    final ip = _alloc(issuerPublic), hp = _alloc(header);
    try {
      final enc = _take(
          lib.lookupFunction<_CredPolicyEncodeC, _CredPolicyEncodeD>(
              'blockchainer_credential_policy_encode')(
              chainId, ip, issuerPublic.length, hp, header.length, attributeCount, minTier),
          'credential_policy_encode');
      return CredentialPolicy._(this, enc);
    } finally {
      calloc.free(ip);
      calloc.free(hp);
    }
  }

  /// Open a verifier's admission [CredentialGate] enforcing [policy].
  CredentialGate makeCredentialGate(CredentialPolicy policy) {
    final pp = _alloc(policy.encoded);
    try {
      final h = lib.lookupFunction<_CredGateNewC, _CredGateNewD>('blockchainer_credential_gate_new')(
          pp, policy.encoded.length);
      if (h == nullptr) throw BlockchainerException('credential gate creation failed');
      return CredentialGate._(this, h);
    } finally {
      calloc.free(pp);
    }
  }

  /// Create a custodial [KeyStore] protected by [passphrase] (returned unlocked).
  KeyStore createKeyStore(String passphrase) {
    final cp = passphrase.toNativeUtf8();
    try {
      final h = lib.lookupFunction<_KsNewC, _KsNewD>('blockchainer_keystore_new')(cp);
      if (h == nullptr) throw BlockchainerException('keystore creation failed');
      return KeyStore._(this, h);
    } finally {
      calloc.free(cp);
    }
  }

  /// Load a serialised [KeyStore]. It comes back LOCKED — call [KeyStore.unlock].
  KeyStore loadKeyStore(Uint8List blob) {
    final p = _alloc(blob);
    try {
      final h = lib.lookupFunction<_KsLoadC, _KsLoadD>('blockchainer_keystore_load')(p, blob.length);
      if (h == nullptr) throw BlockchainerException('keystore load failed');
      return KeyStore._(this, h);
    } finally {
      calloc.free(p);
    }
  }
}

/// A running chain node.
class Node {
  Node._(this._bc, this._h);

  final Blockchainer _bc;
  final Pointer<Void> _h;
  DynamicLibrary get _lib => _bc.lib;

  Account newAccount() {
    final secret = _bc._take(
        _lib.lookupFunction<_BufOfNodeC, _BufOfNodeD>('blockchainer_node_new_account')(_h),
        'new_account');
    final pub = Uint8List.sublistView(secret, 32, 64);
    return Account(secret, pub, accountAddress(pub));
  }

  /// Generate a fresh hybrid (Ed25519 + ML-DSA-65) post-quantum account. The
  /// returned [Account] holds the hybrid key blobs; spend with [submitTransferHybrid].
  Account newAccountHybrid() {
    final blob = _bc._take(
        _lib.lookupFunction<_BufOfNodeC, _BufOfNodeD>('blockchainer_node_new_account_hybrid')(_h),
        'new_account_hybrid');
    // Layout: [pub_len: 4 bytes big-endian][public][secret].
    final pl = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
    final pub = Uint8List.sublistView(blob, 4, 4 + pl);
    final secret = Uint8List.sublistView(blob, 4 + pl);
    return Account(secret, pub, accountAddress(pub));
  }

  Uint8List accountAddress(Uint8List publicKey) {
    final p = _bc._alloc(publicKey);
    try {
      return _bc._take(
          _lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>('blockchainer_account_address')(
              p, publicKey.length),
          'account_address');
    } finally {
      calloc.free(p);
    }
  }

  /// Credit an address at genesis. Valid until the first block is produced
  /// (reads do NOT seal genesis). Throws [BlockchainerException] afterwards —
  /// never a silent no-op.
  void credit(Uint8List address, int amount) {
    final p = _bc._alloc(address);
    try {
      final rc =
          _lib.lookupFunction<_CreditC, _CreditD>('blockchainer_node_credit')(_h, p, amount);
      if (rc == 2) {
        throw BlockchainerException(
            'genesis is sealed (a block was produced); fund accounts with transfers');
      }
      if (rc != 0) throw BlockchainerException('credit failed (status $rc)');
    } finally {
      calloc.free(p);
    }
  }

  Uint8List submitTransfer(Uint8List secret, Uint8List to, int amount, int nonce) {
    final ps = _bc._alloc(secret);
    final pt = _bc._alloc(to);
    try {
      return _bc._take(
          _lib.lookupFunction<_SubmitC, _SubmitD>('blockchainer_node_submit_transfer')(
              _h, ps, secret.length, pt, amount, nonce),
          'submit_transfer');
    } finally {
      calloc.free(ps);
      calloc.free(pt);
    }
  }

  /// Submit an Ed25519 transfer carrying a priority-fee [fee] (a version-2
  /// envelope), used for fee-market mempool priority.
  Uint8List submitTransferFee(
      Uint8List secret, Uint8List to, int amount, int nonce, int fee) {
    final ps = _bc._alloc(secret);
    final pt = _bc._alloc(to);
    try {
      return _bc._take(
          _lib.lookupFunction<_SubmitFeeC, _SubmitFeeD>(
              'blockchainer_node_submit_transfer_fee')(_h, ps, secret.length, pt, amount, nonce, fee),
          'submit_transfer_fee');
    } finally {
      calloc.free(ps);
      calloc.free(pt);
    }
  }

  /// Build, sign, and admit a hybrid (post-quantum) transfer using the hybrid
  /// key blobs from [newAccountHybrid]. Returns the txid.
  Uint8List submitTransferHybrid(
      Uint8List secret, Uint8List public, Uint8List to, int amount, int nonce) {
    final ps = _bc._alloc(secret);
    final pp = _bc._alloc(public);
    final pt = _bc._alloc(to);
    try {
      return _bc._take(
          _lib.lookupFunction<_SubmitHybridC, _SubmitHybridD>(
              'blockchainer_node_submit_transfer_hybrid')(
              _h, ps, secret.length, pp, public.length, pt, amount, nonce),
          'submit_transfer_hybrid');
    } finally {
      calloc.free(ps);
      calloc.free(pp);
      calloc.free(pt);
    }
  }

  int produceBlock(int timestamp) {
    final out = calloc<Uint64>();
    try {
      final rc = _lib.lookupFunction<_ProduceC, _ProduceD>(
          'blockchainer_node_produce_block')(_h, timestamp, out);
      if (rc != 0) throw BlockchainerException('produce_block failed');
      return out.value;
    } finally {
      calloc.free(out);
    }
  }

  int _u64(String fn) => _lib.lookupFunction<_U64NodeC, _U64NodeD>(fn)(_h);
  int _u64Addr(String fn, Uint8List addr) {
    final p = _bc._alloc(addr);
    try {
      return _lib.lookupFunction<_U64NodeAddrC, _U64NodeAddrD>(fn)(_h, p);
    } finally {
      calloc.free(p);
    }
  }

  int height() => _u64('blockchainer_node_height');
  int mempoolSize() => _u64('blockchainer_node_mempool_size');
  int balance(Uint8List address) => _u64Addr('blockchainer_node_balance', address);
  int nonce(Uint8List address) => _u64Addr('blockchainer_node_nonce', address);

  Uint8List stateRoot() => _bc._take(
      _lib.lookupFunction<_BufOfNodeC, _BufOfNodeD>('blockchainer_node_state_root')(_h),
      'state_root');

  /// The streaming surface: chain events as JSON since the last drain.
  /// Drain collected chain events as a JSON array string (the streaming
  /// surface). Parse with `jsonDecode` if you need structured events.
  String drainEvents() => _bc._takeStr(
      _lib.lookupFunction<_BufOfNodeC, _BufOfNodeD>('blockchainer_node_drain_events')(_h),
      'drain_events');

  /// Prometheus-format metrics snapshot.
  String metrics() => _bc._takeStr(
      _lib.lookupFunction<_BufOfNodeC, _BufOfNodeD>('blockchainer_node_metrics')(_h),
      'metrics');

  Uint8List proveAccount(Uint8List address) {
    final p = _bc._alloc(address);
    try {
      return _bc._take(
          _lib.lookupFunction<_BufNodeAddrC, _BufNodeAddrD>(
              'blockchainer_node_prove_account')(_h, p),
          'prove_account');
    } finally {
      calloc.free(p);
    }
  }

  void dispose() =>
      _lib.lookupFunction<_NodeFreeC, _NodeFreeD>('blockchainer_node_free')(_h);

  /// Alias for [dispose] (naming parity with the Go binding's Free).
  void free() => dispose();
}

/// A custodial store that seals account signing secrets at rest. Add accounts,
/// serialise the (encrypted) blob, reload it LOCKED, unlock, then reveal a
/// secret only to sign. Create via [Blockchainer.createKeyStore] /
/// [Blockchainer.loadKeyStore].
class KeyStore {
  KeyStore._(this._bc, this._h);

  final Blockchainer _bc;
  final Pointer<Void> _h;
  DynamicLibrary get _lib => _bc.lib;

  /// Seal an account's [secret]; returns its 32-byte address. Requires unlocked.
  Uint8List addAccount(Uint8List publicKey, Uint8List secret) {
    final pp = _bc._alloc(publicKey);
    final ps = _bc._alloc(secret);
    final pa = calloc<Uint8>(32);
    try {
      final rc = _lib.lookupFunction<_KsAddC, _KsAddD>('blockchainer_keystore_add_account')(
          _h, pp, publicKey.length, ps, secret.length, pa);
      if (rc != 0) throw BlockchainerException('keystore add account failed (locked?)');
      return Uint8List.fromList(pa.asTypedList(32));
    } finally {
      calloc.free(pp);
      calloc.free(ps);
      calloc.free(pa);
    }
  }

  /// Decrypt and return the sealed secret for [address]. Requires unlocked.
  Uint8List reveal(Uint8List address) {
    final pa = _bc._alloc(address);
    try {
      return _bc._take(
          _lib.lookupFunction<_BufNodeAddrC, _BufNodeAddrD>('blockchainer_keystore_reveal')(_h, pa),
          'keystore_reveal');
    } finally {
      calloc.free(pa);
    }
  }

  /// The persistable (encrypted) blob — contains no plaintext key.
  Uint8List serialise() => _bc._take(
      _lib.lookupFunction<_BufOfNodeC, _BufOfNodeD>('blockchainer_keystore_serialise')(_h),
      'keystore_serialise');

  /// Recover the master key from [passphrase]. Throws on a wrong passphrase.
  void unlock(String passphrase) {
    final cp = passphrase.toNativeUtf8();
    try {
      if (_lib.lookupFunction<_KsUnlockC, _KsUnlockD>('blockchainer_keystore_unlock')(_h, cp) != 0) {
        throw BlockchainerException('keystore unlock failed');
      }
    } finally {
      calloc.free(cp);
    }
  }

  /// Whether secrets can currently be revealed.
  bool get unlocked =>
      _lib.lookupFunction<_IntOfPtrC, _IntOfPtrD>('blockchainer_keystore_unlocked')(_h) == 1;

  void dispose() =>
      _lib.lookupFunction<_NodeFreeC, _NodeFreeD>('blockchainer_keystore_free')(_h);
}

/// A signing account's material — public key and secret — as returned by the
/// portable-account "open" methods. For a KEM device key from
/// [Blockchainer.hybridKemKeypair], [public] is the KEM public and [secret] the
/// KEM secret.
class PortableAccount {
  PortableAccount(this.public, this.secret);
  final Uint8List public;
  final Uint8List secret;
}

/// A participant's share of a threshold (FROST t-of-n) account. Persist [bytes]
/// sealed (Blockchainer.sealShare); never store it plaintext.
class ThresholdShare {
  ThresholdShare(this.bytes, this.index, this.groupPublicKey, this.publicShare);
  final Uint8List bytes;
  final int index;
  final Uint8List groupPublicKey; // the account's on-chain author key
  final Uint8List publicShare;
}

/// The security posture a chain commits to at genesis, as described by
/// [Blockchainer.describeAssurance]. Higher tiers only ever tighten the tier
/// below them — the flags are monotonic.
class AssuranceProfile {
  AssuranceProfile({
    required this.ratcheted,
    required this.vrf,
    required this.thresholdCustody,
    required this.sealedMempool,
    required this.sigAlgos,
  });

  /// Transport tier: false = mutually-authenticated, true = PQ-ratcheted.
  final bool ratcheted;

  /// Consensus tier: false = stake-weighted, true = VRF leader election.
  final bool vrf;

  /// Whether validator custody must be threshold (FROST t-of-n).
  final bool thresholdCustody;

  /// Whether the mempool is HPKE-sealed (anti-MEV).
  final bool sealedMempool;

  /// The signature algorithms this posture admits.
  final Uint8List sigAlgos;
}

/// A two-round FROST signing ceremony over a fixed message and roster. Call
/// [free] when done.
class Ceremony {
  Ceremony._(this._bc, this._h);
  final Blockchainer _bc;
  final Pointer<Void> _h;

  /// Round 2 for [card] using its round-1 [nonces]: a 32-byte signature share.
  Uint8List sign(Uint8List card, Uint8List nonces) {
    final cP = _bc._alloc(card), nP = _bc._alloc(nonces);
    try {
      return _bc._take(
          _bc.lib.lookupFunction<_ThSignC, _ThSignD>('blockchainer_threshold_sign')(
              _h, cP, card.length, nP),
          'threshold_sign');
    } finally {
      calloc.free(cP);
      calloc.free(nP);
    }
  }

  /// Attribute a share to participant [id] (its public share): a lying cosigner
  /// is named, not just rejected.
  bool verifyShare(int id, Uint8List publicShare, Uint8List sigShare) {
    final pP = _bc._alloc(publicShare), sP = _bc._alloc(sigShare);
    try {
      return _bc.lib.lookupFunction<_ThVerifyC, _ThVerifyD>('blockchainer_threshold_verify_share')(
              _h, id, pP, sP) ==
          1;
    } finally {
      calloc.free(pP);
      calloc.free(sP);
    }
  }

  /// Aggregate (id → 32-byte share) pairs into a standard 64-byte Ed25519
  /// signature under the group key. Pairs may be in any order.
  Uint8List aggregate(List<int> ids, List<Uint8List> sigShares) {
    final flat = BytesBuilder();
    for (final s in sigShares) flat.add(s);
    final flatBytes = flat.toBytes();
    final idP = calloc<Uint16>(ids.isEmpty ? 1 : ids.length);
    final fP = _bc._alloc(flatBytes);
    try {
      for (var i = 0; i < ids.length; i++) idP[i] = ids[i];
      return _bc._take(
          _bc.lib.lookupFunction<_ThAggC, _ThAggD>('blockchainer_threshold_aggregate')(
              _h, idP, fP, ids.length),
          'threshold_aggregate');
    } finally {
      calloc.free(idP);
      calloc.free(fP);
    }
  }

  void free() =>
      _bc.lib.lookupFunction<_ThCerFreeC, _ThCerFreeD>('blockchainer_threshold_ceremony_free')(_h);
}

/// An anti-MEV mempool of HPKE-sealed transactions. Each sealed tx is opaque to
/// everyone but the block's proposer, so it cannot be front-run or reordered on
/// content before inclusion. Call [free] when done.
class SealedMempool {
  SealedMempool._(this._bc, this._h);
  final Blockchainer _bc;
  final Pointer<Void> _h;

  /// Admit a sealed transaction (from [Blockchainer.sealTx]). Returns false if
  /// the pool is at capacity.
  bool add(Uint8List sealedTx) {
    final p = _bc._alloc(sealedTx);
    try {
      return _bc.lib.lookupFunction<_SmAddC, _SmAddD>('blockchainer_sealed_mempool_add')(
              _h, p, sealedTx.length) ==
          1;
    } finally {
      calloc.free(p);
    }
  }

  /// Sealed transactions currently held.
  int get size => _bc.lib.lookupFunction<_SmSizeC, _SmSizeD>('blockchainer_sealed_mempool_size')(_h);

  /// Reveal up to [max] transactions with the proposer's KEM secret. A non-proposer
  /// key opens nothing (returns empty) — that is the confidentiality guarantee.
  List<Uint8List> reveal(Uint8List proposerKemSecret, int max) {
    final sP = _bc._alloc(proposerKemSecret);
    try {
      final blob = _bc._take(
          _bc.lib.lookupFunction<_SmRevealC, _SmRevealD>('blockchainer_sealed_mempool_reveal')(
              _h, sP, proposerKemSecret.length, max),
          'sealed_mempool_reveal');
      // [count u32]([tx_len u32][tx])×
      final count = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
      final out = <Uint8List>[];
      var off = 4;
      for (var i = 0; i < count; i++) {
        final len =
            (blob[off] << 24) | (blob[off + 1] << 16) | (blob[off + 2] << 8) | blob[off + 3];
        off += 4;
        out.add(Uint8List.sublistView(blob, off, off + len));
        off += len;
      }
      return out;
    } finally {
      calloc.free(sP);
    }
  }

  void free() => _bc.lib
      .lookupFunction<_SmFreeC, _SmFreeD>('blockchainer_sealed_mempool_free')(_h);
}

/// A membership policy: the issuer key, attribute schema, and tier threshold a
/// chain enforces. Its [hash] is canonical and meant to be committed into the
/// genesis (like an AssuranceProfile). Build one with
/// [Blockchainer.makeCredentialPolicy].
class CredentialPolicy {
  CredentialPolicy._(this._bc, this.encoded);
  final Blockchainer _bc;

  /// The policy's canonical bytes (the form the gate consumes).
  final Uint8List encoded;

  /// The 32-byte commitment to commit into genesis.
  Uint8List hash() {
    final p = _bc._alloc(encoded);
    try {
      return _bc._take(
          _bc.lib.lookupFunction<_CredPolicyHashC, _CredPolicyHashD>(
              'blockchainer_credential_policy_hash')(p, encoded.length),
          'credential_policy_hash');
    } finally {
      calloc.free(p);
    }
  }

  /// Issuer side (trusted-issuer model): assemble + BBS-sign a credential. The
  /// 32-byte [memberSecret] is generated by the member. The returned blob is
  /// HOLDER-SECRET — store it sealed.
  Uint8List issue(Uint8List issuerSecret, Uint8List memberSecret, int tier, int notAfter,
      int issuerEpoch) {
    final ep = _bc._alloc(encoded), sp = _bc._alloc(issuerSecret), mp = _bc._alloc(memberSecret);
    try {
      return _bc._take(
          _bc.lib.lookupFunction<_CredIssueC, _CredIssueD>('blockchainer_credential_issue')(
              ep, encoded.length, sp, issuerSecret.length, mp, memberSecret.length, tier, notAfter,
              issuerEpoch),
          'credential_issue');
    } finally {
      calloc.free(ep);
      calloc.free(sp);
      calloc.free(mp);
    }
  }

  /// Holder side: an unlinkable presentation for ([epoch], [scope], [challenge]).
  /// Discloses {tier, not_after, issuer_epoch}; proves the rest in zero knowledge.
  Uint8List present(Uint8List credential, int epoch, String scope, Uint8List challenge) {
    final ep = _bc._alloc(encoded), cp = _bc._alloc(credential), chp = _bc._alloc(challenge);
    final scopeP = scope.toNativeUtf8();
    try {
      return _bc._take(
          _bc.lib.lookupFunction<_CredPresentC, _CredPresentD>('blockchainer_credential_present')(
              ep, encoded.length, cp, credential.length, epoch, scopeP, chp, challenge.length),
          'credential_present');
    } finally {
      calloc.free(ep);
      calloc.free(cp);
      calloc.free(chp);
      calloc.free(scopeP);
    }
  }
}

/// A verifier's anonymous-membership admission gate (Option A — unlinkable). It
/// admits a holder to a role by a zero-knowledge proof that it holds an
/// issuer-signed credential clearing the policy, revealing nothing else. Holds
/// the issuer-epoch deny-list. Call [free] when done.
class CredentialGate {
  CredentialGate._(this._bc, this._h);
  final Blockchainer _bc;
  final Pointer<Void> _h;

  /// Bulk revocation: refuse every credential issued in [epoch].
  void revokeIssuerEpoch(int epoch) => _bc.lib
      .lookupFunction<_CredGateRevokeC, _CredGateRevokeD>('blockchainer_credential_gate_revoke_epoch')(
          _h, epoch);

  /// Admit? Runs the full checklist (proof + expiry + revocation + tier), binding
  /// ([epoch], [scope], [challenge], [now]). A malformed presentation is a quiet
  /// false — this never throws.
  bool admit(int epoch, String scope, Uint8List challenge, Uint8List presentation, int now) {
    final chp = _bc._alloc(challenge), pp = _bc._alloc(presentation);
    final scopeP = scope.toNativeUtf8();
    try {
      return _bc.lib.lookupFunction<_CredGateAdmitC, _CredGateAdmitD>(
              'blockchainer_credential_gate_admit')(
              _h, epoch, scopeP, chp, challenge.length, pp, presentation.length, now) ==
          1;
    } finally {
      calloc.free(chp);
      calloc.free(pp);
      calloc.free(scopeP);
    }
  }

  void free() =>
      _bc.lib.lookupFunction<_CredGateFreeC, _CredGateFreeD>('blockchainer_credential_gate_free')(_h);
}
