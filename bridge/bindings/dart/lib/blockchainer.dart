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
