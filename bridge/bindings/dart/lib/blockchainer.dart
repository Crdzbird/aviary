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

// CredentialGate Option B (pseudonymous).
typedef _CredPolicyEncodeExC = _BufResult Function(
    Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint32, Uint8, Uint8);
typedef _CredPolicyEncodeExD = _BufResult Function(
    int, Pointer<Uint8>, int, Pointer<Uint8>, int, int, int, int);
typedef _MembershipCommitC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _MembershipCommitD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _BlindIssueC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint8, Uint64, Uint32);
typedef _BlindIssueD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int, int, int, int);
typedef _FinalizeMembershipC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _FinalizeMembershipD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _CredPresentPseudC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint32, Pointer<Utf8>, Pointer<Uint8>, Size);
typedef _CredPresentPseudD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, int, Pointer<Utf8>, Pointer<Uint8>, int);
typedef _CredGateAdmitPseudC = _BufResult Function(
    Pointer<Void>, Uint32, Pointer<Utf8>, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint64);
typedef _CredGateAdmitPseudD = _BufResult Function(
    Pointer<Void>, int, Pointer<Utf8>, Pointer<Uint8>, int, Pointer<Uint8>, int, int);
typedef _CredGateRevokeNymC = Void Function(Pointer<Void>, Pointer<Uint8>, Size);
typedef _CredGateRevokeNymD = void Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _CredGateNoteAdmC = Int32 Function(Pointer<Void>, Uint32, Pointer<Utf8>, Pointer<Uint8>, Size);
typedef _CredGateNoteAdmD = int Function(Pointer<Void>, int, Pointer<Utf8>, Pointer<Uint8>, int);

// Composable chains (blueprint + registry + assembly).
typedef _BlsKeygenC = _BufResult Function(Pointer<Uint8>, Size);
typedef _BlsKeygenD = _BufResult Function(Pointer<Uint8>, int);
typedef _BpEncodeC = _BufResult Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Uint8>, Size);
typedef _BpEncodeD = _BufResult Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Uint8>, int);
typedef _BpHashC = _BufResult Function(Pointer<Uint8>, Size);
typedef _BpHashD = _BufResult Function(Pointer<Uint8>, int);
typedef _RegistryIdsC = _BufResult Function(Uint8);
typedef _RegistryIdsD = _BufResult Function(int);
typedef _GenesisBuildC = _BufResult Function(Uint64, Uint64, Uint16, Uint8, Pointer<Uint8>, Pointer<Uint64>, Size, Pointer<Uint8>, Pointer<Uint64>, Size);
typedef _GenesisBuildD = _BufResult Function(int, int, int, int, Pointer<Uint8>, Pointer<Uint64>, int, Pointer<Uint8>, Pointer<Uint64>, int);
typedef _GenesisCommitBpC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _GenesisCommitBpD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _GenesisHashC = _BufResult Function(Pointer<Uint8>, Size);
typedef _GenesisHashD = _BufResult Function(Pointer<Uint8>, int);
typedef _ChainAssembleC = Pointer<Void> Function(Pointer<Uint8>, Size, Uint32, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ChainAssembleD = Pointer<Void> Function(Pointer<Uint8>, int, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _ChainHeightC = Uint64 Function(Pointer<Void>);
typedef _ChainHeightD = int Function(Pointer<Void>);
typedef _ChainBufC = _BufResult Function(Pointer<Void>);
typedef _ChainBufD = _BufResult Function(Pointer<Void>);
typedef _ChainProposeC = _BufResult Function(Pointer<Void>, Uint64);
typedef _ChainProposeD = _BufResult Function(Pointer<Void>, int);
typedef _ChainBlockC = _BufResult Function(Pointer<Void>, Pointer<Uint8>, Size);
typedef _ChainBlockD = _BufResult Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _ChainSubmitC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ChainSubmitD = int Function(Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _ChainFreeC = Void Function(Pointer<Void>);
typedef _ChainFreeD = void Function(Pointer<Void>);
// Subchains.
typedef _ScBufC = _BufResult Function(Pointer<Uint8>, Size);
typedef _ScBufD = _BufResult Function(Pointer<Uint8>, int);
typedef _ScBuf2C = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ScBuf2D = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _ScChildRegC = _BufResult Function(Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Pointer<Uint64>, Size, Uint8);
typedef _ScChildRegD = _BufResult Function(int, Pointer<Uint8>, int, Pointer<Uint8>, Pointer<Uint64>, int, int);
typedef _ScPolicyC = _BufResult Function(Uint16, Uint8, Uint16, Uint16, Pointer<Uint8>, Size);
typedef _ScPolicyD = _BufResult Function(int, int, int, int, Pointer<Uint8>, int);
typedef _ScBindingC = _BufResult Function(Pointer<Uint8>, Size, Uint64, Pointer<Uint8>);
typedef _ScBindingD = _BufResult Function(Pointer<Uint8>, int, int, Pointer<Uint8>);
typedef _ScAnchorC = _BufResult Function(Uint64, Pointer<Uint8>, Size);
typedef _ScAnchorD = _BufResult Function(int, Pointer<Uint8>, int);
typedef _ScChainAnchoredC = _BufResult Function(Pointer<Void>, Uint64, Pointer<Uint8>, Size);
typedef _ScChainAnchoredD = _BufResult Function(Pointer<Void>, int, Pointer<Uint8>, int);
typedef _ScChainIdC = _BufResult Function(Pointer<Void>, Uint64);
typedef _ScChainIdD = _BufResult Function(Pointer<Void>, int);
typedef _ScChainAddrC = _BufResult Function(Pointer<Void>, Pointer<Uint8>);
typedef _ScChainAddrD = _BufResult Function(Pointer<Void>, Pointer<Uint8>);
typedef _ScVerifyC = Int32 Function(Pointer<Uint8>, Pointer<Uint8>, Size);
typedef _ScSchedC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Pointer<Uint64>, Size);
typedef _LcNewC = Pointer<Void> Function(Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _TreeNodeC = _BufResult Function(Uint64, Pointer<Uint8>, Size, Uint16, Pointer<Uint8>, Pointer<Uint64>, Size, Pointer<Uint8>, Pointer<Uint8>, Pointer<Uint64>, Size, Uint16, Uint16, Pointer<Uint8>, Size);
typedef _TreeNodeD = _BufResult Function(int, Pointer<Uint8>, int, int, Pointer<Uint8>, Pointer<Uint64>, int, Pointer<Uint8>, Pointer<Uint8>, Pointer<Uint64>, int, int, int, Pointer<Uint8>, int);
typedef _TreeBuildC = Pointer<Void> Function(Pointer<Uint8>, Size);
typedef _TreeVerifyC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size);
typedef _TreeVerifyD = int Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _TreeBuildD = Pointer<Void> Function(Pointer<Uint8>, int);
typedef _TreeU32IdC = Uint32 Function(Pointer<Void>, Uint64);
typedef _TreeU32IdD = int Function(Pointer<Void>, int);
typedef _TreeU64IdC = Uint64 Function(Pointer<Void>, Uint64);
typedef _TreeU64IdD = int Function(Pointer<Void>, int);
typedef _TreeProduceC = _BufResult Function(Pointer<Void>, Uint64, Uint64);
typedef _TreeProduceD = _BufResult Function(Pointer<Void>, int, int);
typedef _TreeProveC = _BufResult Function(Pointer<Void>, Uint64, Pointer<Uint8>);
typedef _TreeProveD = _BufResult Function(Pointer<Void>, int, Pointer<Uint8>);
typedef _TreeFinC = Void Function(Pointer<Void>, Uint64, Pointer<Uint8>, Size);
typedef _TreeTxC = Int32 Function(Pointer<Void>, Uint64, Uint64, Pointer<Uint8>, Size, Pointer<Size>);
typedef _TreeSetFinC = Void Function(Pointer<Void>, Pointer<NativeFunction<_TreeFinC>>, Pointer<Void>);
typedef _TreeSetFinD = void Function(Pointer<Void>, Pointer<NativeFunction<_TreeFinC>>, Pointer<Void>);
typedef _TreeSetTxC = Void Function(Pointer<Void>, Pointer<NativeFunction<_TreeTxC>>, Pointer<Void>);
typedef _TreeSetTxD = void Function(Pointer<Void>, Pointer<NativeFunction<_TreeTxC>>, Pointer<Void>);

/// One chain of a family; [children] are subchains (must be BFT). A validator's secret is null when
/// this process does not hold it.
class TreeNode {
  TreeNode({required this.chainId, this.blueprint, this.assuranceLevel = 1, this.accounts = const [], required this.validators,
      this.maxAnchorsPerBlock = 0, this.maxHeadersPerAnchor = 0, this.children = const []});
  int chainId; Blueprint? blueprint; int assuranceLevel;
  List<({Uint8List address, int balance})> accounts;
  List<({Uint8List publicKey, Uint8List? secret, int power})> validators;
  int maxAnchorsPerBlock, maxHeadersPerAnchor;
  List<TreeNode> children;
}
final Map<int, ChainTree> _trees = {};
void _treeFinTrampoline(Pointer<Void> ctx, int chainId, Pointer<Uint8> block, int len) {
  final t = _trees[ctx.address]; final cb = t?.onFinalized;
  if (cb != null) cb(chainId, Uint8List.fromList(block.asTypedList(len)));
}
int _treeTxTrampoline(Pointer<Void> ctx, int chainId, int ts, Pointer<Uint8> out, int cap, Pointer<Size> outLen) {
  final t = _trees[ctx.address]; final src = t?.txSource;
  if (src == null) return 0;
  final blob = _blobList(src(chainId, ts));
  if (blob.length > cap) return 0;
  out.asTypedList(cap).setAll(0, blob); outLen.value = blob.length; return 1;
}
typedef _LcNewD = Pointer<Void> Function(int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _LcAcceptC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _LcAcceptD = int Function(Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _LcSkipC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _LcSkipD = int Function(Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _LcTrustC = Void Function(Pointer<Void>, Uint32, Uint32);
typedef _LcTrustD = void Function(Pointer<Void>, int, int);
typedef _LcMaxSkipC = Void Function(Pointer<Void>, Uint64);
typedef _LcMaxSkipD = void Function(Pointer<Void>, int);

/// The canonical ValidatorSet encoding: [count]([len][pk][power u64])×.
List<({Uint8List publicKey, int power})> _decodeValidatorSet(Uint8List b) {
  final bd = ByteData.sublistView(b);
  final n = bd.getUint32(0); var off = 4;
  final out = <({Uint8List publicKey, int power})>[];
  for (var i = 0; i < n; i++) {
    final l = bd.getUint32(off); off += 4;
    final pk = Uint8List.fromList(b.sublist(off, off + l)); off += l;
    out.add((publicKey: pk, power: bd.getUint64(off))); off += 8;
  }
  return out;
}
Uint8List _encodeValidatorSet(List<({Uint8List publicKey, int power})> vs) {
  final b = BytesBuilder();
  b.add((ByteData(4)..setUint32(0, vs.length)).buffer.asUint8List());
  for (final v in vs) {
    b.add((ByteData(4)..setUint32(0, v.publicKey.length)).buffer.asUint8List());
    b.add(v.publicKey);
    b.add((ByteData(8)..setUint64(0, v.power)).buffer.asUint8List());
  }
  return b.toBytes();
}
typedef _ScSchedD = int Function(Pointer<Void>, Pointer<Uint8>, Pointer<Uint64>, int);
typedef _ScVerifyD = int Function(Pointer<Uint8>, Pointer<Uint8>, int);

/// [count u32]([len u32][bytes])× — the C ABI's blob-list convention.
Uint8List _blobList(List<Uint8List> items) {
  final b = BytesBuilder();
  final n = ByteData(4)..setUint32(0, items.length);
  b.add(n.buffer.asUint8List());
  for (final it in items) {
    final l = ByteData(4)..setUint32(0, it.length);
    b.add(l.buffer.asUint8List());
    b.add(it);
  }
  return b.toBytes();
}

// Foreign components (engines / proposers implemented in Dart).
typedef _JustifyC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Size>);
typedef _FVerifyC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ProposerC = Uint32 Function(Pointer<Void>, Uint64, Uint32);
typedef _RegEngineC = Int32 Function(Pointer<Utf8>, Pointer<NativeFunction<_JustifyC>>, Pointer<NativeFunction<_FVerifyC>>, Pointer<Void>, Int32);
typedef _RegEngineD = int Function(Pointer<Utf8>, Pointer<NativeFunction<_JustifyC>>, Pointer<NativeFunction<_FVerifyC>>, Pointer<Void>, int);
typedef _RegProposerC = Int32 Function(Pointer<Utf8>, Pointer<NativeFunction<_ProposerC>>, Pointer<Void>);
typedef _RegProposerD = int Function(Pointer<Utf8>, Pointer<NativeFunction<_ProposerC>>, Pointer<Void>);

/// A consensus engine implemented in Dart, plugged into a chain mixture by id.
/// The consensus pipeline stays native (propose/apply go through the Ledger);
/// Dart supplies the JUSTIFICATION policy. Both methods run synchronously inside
/// consensus and MUST be deterministic (no clock, no randomness) and must not
/// call back into the chain.
abstract class ForeignEngine {
  /// The justification for a block this node proposed (≤ 4096 bytes), or null to refuse.
  Uint8List? justify(Uint8List block);
  bool verify(Uint8List block, Uint8List justification);
}

final Map<int, ForeignEngine> _foreignEngines = {};
final Map<int, int Function(int height, int validators)> _foreignProposers = {};
int _nextForeignHandle = 1;
final List<NativeCallable> _foreignCallables = []; // kept alive for the process lifetime

int _justifyTrampoline(Pointer<Void> ctx, Pointer<Uint8> block, int len, Pointer<Uint8> out, int cap, Pointer<Size> outLen) {
  final e = _foreignEngines[ctx.address];
  if (e == null) return 0;
  final j = e.justify(Uint8List.fromList(block.asTypedList(len)));
  if (j == null || j.length > cap) return 0;
  out.asTypedList(cap).setAll(0, j);
  outLen.value = j.length;
  return 1;
}
int _verifyTrampoline(Pointer<Void> ctx, Pointer<Uint8> block, int len, Pointer<Uint8> just, int jlen) {
  final e = _foreignEngines[ctx.address];
  if (e == null) return 0;
  return e.verify(Uint8List.fromList(block.asTypedList(len)), Uint8List.fromList(just.asTypedList(jlen))) ? 1 : 0;
}
int _proposerTrampoline(Pointer<Void> ctx, int height, int validators) {
  final p = _foreignProposers[ctx.address];
  return p == null ? 0 : p(height, validators);
}

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
  /// 4). [mode] is 0 = unlinkable (Option A, default), 1 = pseudonymous (Option
  /// B). Its [CredentialPolicy.hash] can be committed into genesis.
  CredentialPolicy makeCredentialPolicy(
      {required int chainId,
      required Uint8List issuerPublic,
      required Uint8List header,
      int attributeCount = 4,
      int minTier = 0,
      int mode = 0}) {
    final ip = _alloc(issuerPublic), hp = _alloc(header);
    try {
      final enc = mode == 0
          ? _take(
              lib.lookupFunction<_CredPolicyEncodeC, _CredPolicyEncodeD>(
                  'blockchainer_credential_policy_encode')(
                  chainId, ip, issuerPublic.length, hp, header.length, attributeCount, minTier),
              'credential_policy_encode')
          : _take(
              lib.lookupFunction<_CredPolicyEncodeExC, _CredPolicyEncodeExD>(
                  'blockchainer_credential_policy_encode_ex')(
                  chainId, ip, issuerPublic.length, hp, header.length, attributeCount, minTier, mode),
              'credential_policy_encode_ex');
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

  /// Option B holder step 3: finalize the pseudonymous credential from the
  /// membership-commit blob and the issuer's blind-issuance blob. HOLDER-SECRET.
  Uint8List finalizeMembership(Uint8List membershipCommit, Uint8List blindIssuance) {
    final mp = _alloc(membershipCommit), bp = _alloc(blindIssuance);
    try {
      return _take(
          lib.lookupFunction<_FinalizeMembershipC, _FinalizeMembershipD>(
              'blockchainer_finalize_membership')(
              mp, membershipCommit.length, bp, blindIssuance.length),
          'finalize_membership');
    } finally {
      calloc.free(mp);
      calloc.free(bp);
    }
  }

  // ── Composable chains (blueprint + registry discovery + assembly) ──────────

  /// A validator BLS keypair (public 48 B, secret 32 B) from [keyMaterial].
  ({Uint8List public, Uint8List secret}) newBLSValidator(Uint8List keyMaterial) {
    final p = _alloc(keyMaterial);
    try {
      final blob = _take(lib.lookupFunction<_BlsKeygenC, _BlsKeygenD>('blockchainer_bls_keygen')(p, keyMaterial.length), 'bls_keygen');
      final pl = (blob[0] << 24) | (blob[1] << 16) | (blob[2] << 8) | blob[3];
      return (public: Uint8List.sublistView(blob, 4, 4 + pl), secret: Uint8List.sublistView(blob, 4 + pl));
    } finally {
      calloc.free(p);
    }
  }

  /// Declare a mixture: engine / state model / proposer ids + component params.
  Blueprint makeBlueprint({required String consensus, required String stateModel, String proposer = '', Uint8List? params}) {
    final pr = params ?? Uint8List(0);
    final cc = consensus.toNativeUtf8(), cs = stateModel.toNativeUtf8(), cp = proposer.toNativeUtf8(), pp = _alloc(pr);
    try {
      return Blueprint._(this, _take(lib.lookupFunction<_BpEncodeC, _BpEncodeD>('blockchainer_blueprint_encode')(cc, cs, cp, pp, pr.length), 'blueprint_encode'));
    } finally {
      calloc.free(cc); calloc.free(cs); calloc.free(cp); calloc.free(pp);
    }
  }

  List<String> _registryIds(int kind) {
    final b = _take(lib.lookupFunction<_RegistryIdsC, _RegistryIdsD>('blockchainer_registry_ids')(kind), 'registry_ids');
    final n = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
    final out = <String>[]; var off = 4;
    for (var i = 0; i < n; i++) {
      final l = (b[off] << 24) | (b[off + 1] << 16) | (b[off + 2] << 8) | b[off + 3]; off += 4;
      out.add(String.fromCharCodes(b.sublist(off, off + l))); off += l;
    }
    return out;
  }
  /// The engines / state models / proposer policies a mixture may name.
  List<String> engineIds() => _registryIds(0);
  List<String> stateModelIds() => _registryIds(1);
  List<String> proposerIds() => _registryIds(2);

  /// A minimal genesis: chain params + posture (1/2/3) + seeded state + validators.
  Uint8List buildGenesis({required int chainId, int timestamp = 1, int assuranceLevel = 1, int stateModel = 0,
      required List<({Uint8List address, int balance})> accounts, required List<({Uint8List publicKey, int power})> validators}) {
    final addrs = Uint8List.fromList(accounts.expand((a) => a.address).toList());
    final pks = Uint8List.fromList(validators.expand((v) => v.publicKey).toList());
    final ap = _alloc(addrs), kp = _alloc(pks);
    final bal = calloc<Uint64>(accounts.isEmpty ? 1 : accounts.length), pow = calloc<Uint64>(validators.isEmpty ? 1 : validators.length);
    for (var i = 0; i < accounts.length; i++) bal[i] = accounts[i].balance;
    for (var i = 0; i < validators.length; i++) pow[i] = validators[i].power;
    try {
      return _take(lib.lookupFunction<_GenesisBuildC, _GenesisBuildD>('blockchainer_genesis_build')(
          chainId, timestamp, assuranceLevel, stateModel, ap, bal, accounts.length, kp, pow, validators.length), 'genesis_build');
    } finally {
      calloc.free(ap); calloc.free(kp); calloc.free(bal); calloc.free(pow);
    }
  }
  /// Canonically encode one [TreeNode] (children first).
  Uint8List encodeTreeNode(TreeNode n) {
    final bp = (n.blueprint ?? makeBlueprint(consensus: 'bft', stateModel: 'account')).encoded;
    final addrs = Uint8List.fromList(n.accounts.expand((a) => a.address).toList());
    final pks = Uint8List.fromList(n.validators.expand((v) => v.publicKey).toList());
    final secrets = Uint8List.fromList(n.validators.expand((v) => v.secret ?? Uint8List(32)).toList());
    final kids = _blobList(n.children.map(encodeTreeNode).toList());
    final bpp = _alloc(bp), ap = _alloc(addrs), kp = _alloc(pks), sp = _alloc(secrets), cp = _alloc(kids);
    final bal = calloc<Uint64>(n.accounts.isEmpty ? 1 : n.accounts.length), pw = calloc<Uint64>(n.validators.isEmpty ? 1 : n.validators.length);
    for (var i = 0; i < n.accounts.length; i++) bal[i] = n.accounts[i].balance;
    for (var i = 0; i < n.validators.length; i++) pw[i] = n.validators[i].power;
    try {
      return _take(lib.lookupFunction<_TreeNodeC, _TreeNodeD>('blockchainer_chain_tree_node_encode')(
          n.chainId, bpp, bp.length, n.assuranceLevel, ap, bal, n.accounts.length, kp, sp, pw, n.validators.length,
          n.maxAnchorsPerBlock, n.maxHeadersPerAnchor, cp, kids.length), 'chain_tree_node_encode');
    } finally { calloc.free(bpp); calloc.free(ap); calloc.free(kp); calloc.free(sp); calloc.free(cp); calloc.free(bal); calloc.free(pw); }
  }
  /// Why a spec is structurally invalid ('' = valid).
  String chainTreeSpecError(Uint8List spec) => String.fromCharCodes(_bufOp('blockchainer_chain_tree_spec_error', spec));
  /// Build a family of chains from [root]; throws with the structural reason or a generic reject.
  ChainTree buildChainTree(TreeNode root) {
    final spec = encodeTreeNode(root);
    final err = chainTreeSpecError(spec);
    if (err.isNotEmpty) throw StateError('chain tree: $err');
    final sp = _alloc(spec);
    try {
      final h = lib.lookupFunction<_TreeBuildC, _TreeBuildD>('blockchainer_chain_tree_build')(sp, spec.length);
      if (h == nullptr) throw StateError('chain tree: build rejected (unknown mixture or posture-forbidden engine)');
      return ChainTree._(this, h);
    } finally { calloc.free(sp); }
  }

  /// A light client trusting [trustedHeader] and the set in force for the next header; null if malformed.
  LightClient? newLightClient(int chainId, List<({Uint8List publicKey, int power})> validators, Uint8List trustedHeader) {
    final vs = _encodeValidatorSet(validators);
    final vp = _alloc(vs), hp = _alloc(trustedHeader);
    try {
      final h = lib.lookupFunction<_LcNewC, _LcNewD>('blockchainer_light_client_new')(chainId, vp, vs.length, hp, trustedHeader.length);
      return h == nullptr ? null : LightClient._(this, h);
    } finally { calloc.free(vp); calloc.free(hp); }
  }

  // ── Subchains: a parent block anchors a child chain's finalized headers ────
  // (docs/design/SUBCHAINS.md). Register a child at the parent's genesis by its
  // genesis HEADER + fixed validator set; anchor its certified headers; prove a
  // state entry on a descendant from the parent's header. v1 anchors BFT children.

  Uint8List _bufOp(String fn, Uint8List a) {
    final p = _alloc(a);
    try { return _take(lib.lookupFunction<_ScBufC, _ScBufD>(fn)(p, a.length), fn); } finally { calloc.free(p); }
  }
  Uint8List _buf2Op(String fn, Uint8List a, Uint8List b) {
    final pa = _alloc(a), pb = _alloc(b);
    try { return _take(lib.lookupFunction<_ScBuf2C, _ScBuf2D>(fn)(pa, a.length, pb, b.length), fn); } finally { calloc.free(pa); calloc.free(pb); }
  }
  /// A genesis's genesis HEADER — the identity a parent registers a child by.
  Uint8List genesisHeader(Uint8List genesis) => _bufOp('blockchainer_genesis_header', genesis);
  /// Encode a child registration (48-byte BLS keys; [maxDepth] = the child's own subtree depth, 0 = leaf).
  Uint8List encodeChildRegistration({required int chainId, required Uint8List genesisHeader,
      required List<({Uint8List publicKey, int power})> validators, int maxDepth = 0}) {
    final pks = Uint8List.fromList(validators.expand((v) => v.publicKey).toList());
    final hp = _alloc(genesisHeader), kp = _alloc(pks), pw = calloc<Uint64>(validators.isEmpty ? 1 : validators.length);
    for (var i = 0; i < validators.length; i++) pw[i] = validators[i].power;
    try {
      return _take(lib.lookupFunction<_ScChildRegC, _ScChildRegD>('blockchainer_child_registration_encode')(
          chainId, hp, genesisHeader.length, kp, pw, validators.length, maxDepth), 'child_registration_encode');
    } finally { calloc.free(hp); calloc.free(kp); calloc.free(pw); }
  }
  /// Encode a parent's subchain policy from encoded registrations (ascending, unique chain ids). 0 = unlimited.
  Uint8List encodeSubchainPolicy({required int maxChildren, int maxDepth = 1, int maxAnchorsPerBlock = 0,
      int maxHeadersPerAnchor = 0, required List<Uint8List> children}) {
    final blob = _blobList(children);
    final p = _alloc(blob);
    try {
      return _take(lib.lookupFunction<_ScPolicyC, _ScPolicyD>('blockchainer_subchain_policy_encode')(
          maxChildren, maxDepth, maxAnchorsPerBlock, maxHeadersPerAnchor, p, blob.length), 'subchain_policy_encode');
    } finally { calloc.free(p); }
  }
  /// Commit a policy into a genesis (this chain becomes a PARENT; its hash changes). Throws on an invalid policy.
  Uint8List commitSubchainPolicy(Uint8List genesis, Uint8List policy) => _buf2Op('blockchainer_genesis_commit_subchain_policy', genesis, policy);
  /// Bind a child genesis to its parent (id + 32-byte parent genesis hash); the child can never be re-parented.
  Uint8List commitParentBinding(Uint8List genesis, int parentChainId, Uint8List parentGenesisHash) {
    final gp = _alloc(genesis), hp = _alloc(parentGenesisHash);
    try {
      return _take(lib.lookupFunction<_ScBindingC, _ScBindingD>('blockchainer_genesis_commit_parent_binding')(gp, genesis.length, parentChainId, hp), 'genesis_commit_parent_binding');
    } finally { calloc.free(gp); calloc.free(hp); }
  }
  Uint8List blockHeader(Uint8List block) => _bufOp('blockchainer_block_header', block);
  /// A header's 32-byte subchain root (zero below version 3).
  Uint8List headerSubchainRoot(Uint8List header) => _bufOp('blockchainer_header_subchain_root', header);
  Uint8List certifiedHeader(Uint8List header, Uint8List certificate) => _buf2Op('blockchainer_certified_header_encode', header, certificate);
  /// An anchor: one child's CONSECUTIVE certified headers.
  Uint8List encodeAnchor(int childChainId, List<Uint8List> certifiedHeaders) {
    final blob = _blobList(certifiedHeaders);
    final p = _alloc(blob);
    try { return _take(lib.lookupFunction<_ScAnchorC, _ScAnchorD>('blockchainer_anchor_encode')(childChainId, p, blob.length), 'anchor_encode'); } finally { calloc.free(p); }
  }
  /// A cross-chain proof: hops (parent → … → the chain holding the entry) + that chain's state proof.
  Uint8List encodeSubchainProof(List<Uint8List> hops, Uint8List stateProof) => _buf2Op('blockchainer_subchain_proof_encode', _blobList(hops), stateProof);
  /// Whether the proof is committed under the parent's 32-byte subchain root all the way down and its state proof holds.
  bool verifySubchainProof(Uint8List parentSubchainRoot, Uint8List proof) {
    if (parentSubchainRoot.length != 32) return false;
    final rp = _alloc(parentSubchainRoot), pp = _alloc(proof);
    try { return lib.lookupFunction<_ScVerifyC, _ScVerifyD>('blockchainer_subchain_proof_verify')(rp, pp, proof.length) == 1; } finally { calloc.free(rp); calloc.free(pp); }
  }

  /// Commit a mixture into a genesis (its hash changes: the mixture is chain identity).
  Uint8List commitBlueprint(Uint8List genesis, Blueprint bp) {
    final gp = _alloc(genesis), bp2 = _alloc(bp.encoded);
    try {
      return _take(lib.lookupFunction<_GenesisCommitBpC, _GenesisCommitBpD>('blockchainer_genesis_commit_blueprint')(gp, genesis.length, bp2, bp.encoded.length), 'genesis_commit_blueprint');
    } finally {
      calloc.free(gp); calloc.free(bp2);
    }
  }
  Uint8List genesisHash(Uint8List genesis) {
    final gp = _alloc(genesis);
    try {
      return _take(lib.lookupFunction<_GenesisHashC, _GenesisHashD>('blockchainer_genesis_hash')(gp, genesis.length), 'genesis_hash');
    } finally {
      calloc.free(gp);
    }
  }
  /// Assemble for validator [me]. Returns null on any fail-closed reject
  /// (unknown id, state mismatch, posture-forbidden engine).
  Chain? assembleChain(Uint8List genesis, int me, Uint8List blsPub, Uint8List blsSec) {
    final gp = _alloc(genesis), pp = _alloc(blsPub), sp = _alloc(blsSec);
    try {
      final h = lib.lookupFunction<_ChainAssembleC, _ChainAssembleD>('blockchainer_chain_assemble')(gp, genesis.length, me, pp, blsPub.length, sp, blsSec.length);
      return h == nullptr ? null : Chain._(this, h);
    } finally {
      calloc.free(gp); calloc.free(pp); calloc.free(sp);
    }
  }

  /// Register a Dart [ForeignEngine] under [id] (chain identity — never rename).
  /// [grindProof] declares whether its leader election is unpredictable.
  bool registerEngine(String id, ForeignEngine engine, {bool grindProof = false}) {
    final h = _nextForeignHandle++;
    _foreignEngines[h] = engine;
    final j = NativeCallable<_JustifyC>.isolateLocal(_justifyTrampoline, exceptionalReturn: 0)..keepIsolateAlive = false;
    final v = NativeCallable<_FVerifyC>.isolateLocal(_verifyTrampoline, exceptionalReturn: 0)..keepIsolateAlive = false;
    _foreignCallables..add(j)..add(v);
    final cid = id.toNativeUtf8();
    try {
      return lib.lookupFunction<_RegEngineC, _RegEngineD>('blockchainer_registry_add_engine')(
              cid, j.nativeFunction, v.nativeFunction, Pointer<Void>.fromAddress(h), grindProof ? 1 : 0) == 1;
    } finally {
      calloc.free(cid);
    }
  }

  /// Register a Dart proposer policy under [id]: the proposer index for a height
  /// among [validators]. Must be deterministic.
  bool registerProposer(String id, int Function(int height, int validators) policy) {
    final h = _nextForeignHandle++;
    _foreignProposers[h] = policy;
    final p = NativeCallable<_ProposerC>.isolateLocal(_proposerTrampoline, exceptionalReturn: 0)..keepIsolateAlive = false;
    _foreignCallables.add(p);
    final cid = id.toNativeUtf8();
    try {
      return lib.lookupFunction<_RegProposerC, _RegProposerD>('blockchainer_registry_add_proposer')(
              cid, p.nativeFunction, Pointer<Void>.fromAddress(h)) == 1;
    } finally {
      calloc.free(cid);
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

  // ── Option B — pseudonymous ──────────────────────────────────────────────
  /// Holder step 1: derive a nym seed from [memberSecret] and blind-commit it.
  /// The blob is HOLDER-SECRET; [extractCommitment] gives the part to send to the
  /// issuer.
  Uint8List membershipCommit(Uint8List memberSecret) {
    final ep = _bc._alloc(encoded), mp = _bc._alloc(memberSecret);
    try {
      return _bc._take(
          _bc.lib.lookupFunction<_MembershipCommitC, _MembershipCommitD>(
              'blockchainer_membership_commit')(ep, encoded.length, mp, memberSecret.length),
          'membership_commit');
    } finally {
      calloc.free(ep);
      calloc.free(mp);
    }
  }

  /// Issuer step 2: blind-sign the holder's [commitment] (never sees the seed).
  Uint8List blindIssue(Uint8List issuerSecret, Uint8List commitment, int tier, int notAfter,
      int issuerEpoch) {
    final ep = _bc._alloc(encoded), sp = _bc._alloc(issuerSecret), cp = _bc._alloc(commitment);
    try {
      return _bc._take(
          _bc.lib.lookupFunction<_BlindIssueC, _BlindIssueD>('blockchainer_blind_issue')(
              ep, encoded.length, sp, issuerSecret.length, cp, commitment.length, tier, notAfter,
              issuerEpoch),
          'blind_issue');
    } finally {
      calloc.free(ep);
      calloc.free(sp);
      calloc.free(cp);
    }
  }

  /// Holder step 3: a per-epoch pseudonymous presentation for ([epoch], [scope],
  /// [challenge]).
  Uint8List presentPseudonymous(Uint8List credential, int epoch, String scope,
      Uint8List challenge) {
    final ep = _bc._alloc(encoded), cp = _bc._alloc(credential), chp = _bc._alloc(challenge);
    final scopeP = scope.toNativeUtf8();
    try {
      return _bc._take(
          _bc.lib.lookupFunction<_CredPresentPseudC, _CredPresentPseudD>(
              'blockchainer_credential_present_pseudonymous')(
              ep, encoded.length, cp, credential.length, epoch, scopeP, chp, challenge.length),
          'credential_present_pseudonymous');
    } finally {
      calloc.free(ep);
      calloc.free(cp);
      calloc.free(chp);
      calloc.free(scopeP);
    }
  }
}

/// The commitment is the first length-prefixed field of a membership-commit blob.
Uint8List extractCommitment(Uint8List membershipCommit) {
  final n = (membershipCommit[0] << 24) |
      (membershipCommit[1] << 16) |
      (membershipCommit[2] << 8) |
      membershipCommit[3];
  return Uint8List.sublistView(membershipCommit, 4, 4 + n);
}

/// A declared chain mixture (engine / state model / proposer ids + params). Its
/// [hash] is the commitment; commit it with [Blockchainer.commitBlueprint].
class Blueprint {
  Blueprint._(this._bc, this.encoded);
  final Blockchainer _bc;
  final Uint8List encoded;
  Uint8List hash() {
    final p = _bc._alloc(encoded);
    try {
      return _bc._take(_bc.lib.lookupFunction<_BpHashC, _BpHashD>('blockchainer_blueprint_hash')(p, encoded.length), 'blueprint_hash');
    } finally {
      calloc.free(p);
    }
  }
}

/// An assembled node behind the uniform propose → (seal | mine) → submit flow.
class Chain {
  Chain._(this._bc, this._h);
  final Blockchainer _bc;
  final Pointer<Void> _h;
  int get height => _bc.lib.lookupFunction<_ChainHeightC, _ChainHeightD>('blockchainer_chain_height')(_h);
  Uint8List blueprint() => _bc._take(_bc.lib.lookupFunction<_ChainBufC, _ChainBufD>('blockchainer_chain_blueprint')(_h), 'chain_blueprint');
  Uint8List propose(int timestamp) => _bc._take(_bc.lib.lookupFunction<_ChainProposeC, _ChainProposeD>('blockchainer_chain_propose')(_h, timestamp), 'chain_propose');
  Uint8List _blockOp(String fn, Uint8List block) {
    final p = _bc._alloc(block);
    try {
      return _bc._take(_bc.lib.lookupFunction<_ChainBlockC, _ChainBlockD>(fn)(_h, p, block.length), fn);
    } finally {
      calloc.free(p);
    }
  }
  /// PoS / VRF justification; empty for engines that do not seal.
  Uint8List seal(Uint8List block) => _blockOp('blockchainer_chain_seal', block);
  /// PoW: the solved block.
  Uint8List mine(Uint8List block) => _blockOp('blockchainer_chain_mine', block);
  bool submit(Uint8List block, [Uint8List? justification]) {
    final j = justification ?? Uint8List(0);
    final bp = _bc._alloc(block), jp = _bc._alloc(j);
    try {
      return _bc.lib.lookupFunction<_ChainSubmitC, _ChainSubmitD>('blockchainer_chain_submit')(_h, bp, block.length, jp, j.length) == 1;
    } finally {
      calloc.free(bp); calloc.free(jp);
    }
  }
  void free() => _bc.lib.lookupFunction<_ChainFreeC, _ChainFreeD>('blockchainer_chain_free')(_h);

  // ── Subchains ──
  /// Propose the next block on a PARENT chain, anchoring encoded anchors (ascending child id). Throws on an invalid anchor.
  Uint8List proposeAnchored(int timestamp, List<Uint8List> anchors) {
    final blob = _blobList(anchors);
    final p = _bc._alloc(blob);
    try { return _bc._take(_bc.lib.lookupFunction<_ScChainAnchoredC, _ScChainAnchoredD>('blockchainer_chain_propose_anchored')(_h, timestamp, p, blob.length), 'chain_propose_anchored'); } finally { calloc.free(p); }
  }
  /// BFT: this validator's precommit aggregated into a commit certificate (the justification for submit; what a parent anchors).
  Uint8List certify(Uint8List block) => _blockOp('blockchainer_chain_certify', block);
  /// BFT: schedule a coordinated validator-set change; the next proposal announces it. Throws if not BFT / malformed.
  void scheduleValidators(List<({Uint8List publicKey, int power})> validators) {
    final pks = Uint8List.fromList(validators.expand((v) => v.publicKey).toList());
    final kp = _bc._alloc(pks), pw = calloc<Uint64>(validators.isEmpty ? 1 : validators.length);
    for (var i = 0; i < validators.length; i++) pw[i] = validators[i].power;
    try {
      if (_bc.lib.lookupFunction<_ScSchedC, _ScSchedD>('blockchainer_chain_schedule_validators')(_h, kp, pw, validators.length) != 1) {
        throw StateError('chain: cannot schedule validators (not BFT, or malformed set)');
      }
    } finally { calloc.free(kp); calloc.free(pw); }
  }
  /// BFT: the validator set in force for the next block.
  List<({Uint8List publicKey, int power})> validators() =>
      _decodeValidatorSet(_bc._take(_bc.lib.lookupFunction<_ChainBufC, _ChainBufD>('blockchainer_chain_validators')(_h), 'chain_validators'));
  /// A parent's last anchored header for a child.
  Uint8List childTip(int childChainId) => _bc._take(_bc.lib.lookupFunction<_ScChainIdC, _ScChainIdD>('blockchainer_chain_child_tip')(_h, childChainId), 'chain_child_tip');
  /// A proof hop from this parent's current registry into a child.
  Uint8List subchainHop(int childChainId) => _bc._take(_bc.lib.lookupFunction<_ScChainIdC, _ScChainIdD>('blockchainer_chain_subchain_hop')(_h, childChainId), 'chain_subchain_hop');
  /// State-inclusion proof for a 32-byte address on this account-model chain.
  Uint8List proveAccount(Uint8List address) {
    final p = _bc._alloc(address);
    try { return _bc._take(_bc.lib.lookupFunction<_ScChainAddrC, _ScChainAddrD>('blockchainer_chain_prove_account')(_h, p), 'chain_prove_account'); } finally { calloc.free(p); }
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

  // ── Option B — pseudonymous ──────────────────────────────────────────────
  /// Admit? Returns the 48-byte pseudonym on success, or null on any reject.
  /// Never throws.
  Uint8List? admitPseudonymous(
      int epoch, String scope, Uint8List challenge, Uint8List presentation, int now) {
    final chp = _bc._alloc(challenge), pp = _bc._alloc(presentation);
    final scopeP = scope.toNativeUtf8();
    try {
      final nym = _bc._take(
          _bc.lib.lookupFunction<_CredGateAdmitPseudC, _CredGateAdmitPseudD>(
              'blockchainer_credential_gate_admit_pseudonymous')(
              _h, epoch, scopeP, chp, challenge.length, pp, presentation.length, now),
          'admit_pseudonymous');
      return nym.isEmpty ? null : nym;
    } finally {
      calloc.free(chp);
      calloc.free(pp);
      calloc.free(scopeP);
    }
  }

  /// §6.2 targeted revocation of an exact pseudonym.
  void revokeNym(Uint8List nym) {
    final np = _bc._alloc(nym);
    try {
      _bc.lib.lookupFunction<_CredGateRevokeNymC, _CredGateRevokeNymD>(
          'blockchainer_credential_gate_revoke_nym')(_h, np, nym.length);
    } finally {
      calloc.free(np);
    }
  }

  /// §6.3 Sybil cap: record an admitted [nym]; true if first this ([epoch], [scope]).
  bool noteAdmission(int epoch, String scope, Uint8List nym) {
    final np = _bc._alloc(nym);
    final scopeP = scope.toNativeUtf8();
    try {
      return _bc.lib.lookupFunction<_CredGateNoteAdmC, _CredGateNoteAdmD>(
              'blockchainer_credential_gate_note_admission')(_h, epoch, scopeP, np, nym.length) ==
          1;
    } finally {
      calloc.free(np);
      calloc.free(scopeP);
    }
  }

  void free() =>
      _bc.lib.lookupFunction<_CredGateFreeC, _CredGateFreeD>('blockchainer_credential_gate_free')(_h);
}

/// A header, its certificate, and the validator set in force at its height.
typedef Checkpoint = ({Uint8List header, Uint8List certificate, List<({Uint8List publicKey, int power})> validators});

/// Skip results of [LightClient.acceptSkipping].
enum SkipResult { refused, accepted, insufficientOverlap }

/// A light client: follows a header chain with certificates only. Sequential [accept] plus the
/// OVERLAP (skipping) rule — [acceptSkipping] jumps to a far header when its own set certifies it
/// and the signers hold more than the trust level (default 1/3) of the set the client already
/// trusts; [syncTo] bisects when the overlap is too thin.
class LightClient {
  LightClient._(this._bc, this._h);
  final Blockchainer _bc;
  final Pointer<Void> _h;
  int get height => _bc.lib.lookupFunction<_ChainHeightC, _ChainHeightD>('blockchainer_light_client_height')(_h);
  Uint8List tip() => _bc._take(_bc.lib.lookupFunction<_ChainBufC, _ChainBufD>('blockchainer_light_client_tip')(_h), 'light_client_tip');
  List<({Uint8List publicKey, int power})> validators() =>
      _decodeValidatorSet(_bc._take(_bc.lib.lookupFunction<_ChainBufC, _ChainBufD>('blockchainer_light_client_validators')(_h), 'light_client_validators'));
  void setTrustLevel(int numerator, int denominator) => _bc.lib.lookupFunction<_LcTrustC, _LcTrustD>('blockchainer_light_client_set_trust_level')(_h, numerator, denominator);
  void setMaxSkip(int blocks) => _bc.lib.lookupFunction<_LcMaxSkipC, _LcMaxSkipD>('blockchainer_light_client_set_max_skip')(_h, blocks);
  bool accept(Uint8List header, Uint8List certificate) {
    final hp = _bc._alloc(header), cp = _bc._alloc(certificate);
    try { return _bc.lib.lookupFunction<_LcAcceptC, _LcAcceptD>('blockchainer_light_client_accept')(_h, hp, header.length, cp, certificate.length) == 1; }
    finally { calloc.free(hp); calloc.free(cp); }
  }
  SkipResult acceptSkipping(Checkpoint cp) {
    final vs = _encodeValidatorSet(cp.validators);
    final hp = _bc._alloc(cp.header), cpp = _bc._alloc(cp.certificate), vp = _bc._alloc(vs);
    try {
      final r = _bc.lib.lookupFunction<_LcSkipC, _LcSkipD>('blockchainer_light_client_accept_skipping')(_h, hp, cp.header.length, cpp, cp.certificate.length, vp, vs.length);
      return r == 1 ? SkipResult.accepted : (r == 2 ? SkipResult.insufficientOverlap : SkipResult.refused);
    } finally { calloc.free(hp); calloc.free(cpp); calloc.free(vp); }
  }
  /// Bisection: [fetch](height) → checkpoint or null. Throws on a hard refusal / missing checkpoint.
  void syncTo(Checkpoint target, int targetHeight, Checkpoint? Function(int height) fetch) {
    final stack = <(Checkpoint, int)>[(target, targetHeight)];
    for (var n = 0; stack.isNotEmpty; n++) {
      if (n > 128) throw StateError('light client: bisection exceeded its fetch budget');
      final (cp, hi) = stack.last;
      final r = acceptSkipping(cp);
      if (r == SkipResult.accepted) { stack.removeLast(); continue; }
      if (r != SkipResult.insufficientOverlap) throw StateError('light client: checkpoint refused');
      final lo = height;
      if (hi - lo <= 1) throw StateError('light client: insufficient overlap and no intermediate header');
      final mid = lo + (hi - lo) ~/ 2;
      final m = fetch(mid);
      if (m == null) throw StateError('light client: checkpoint unavailable at $mid');
      stack.add((m, mid));
    }
  }
  void free() => _bc.lib.lookupFunction<_ChainFreeC, _ChainFreeD>('blockchainer_light_client_free')(_h);
}

/// A built family of chains driven in-process: [produce] anchors children upward automatically;
/// [proveAccount] proves any descendant from the root header. Hooks: [onFinalized], [txSource].
class ChainTree {
  ChainTree._(this._bc, this._h) {
    _handle = _trees.length + 1;
    _trees[_handle] = this;
    _fin = NativeCallable<_TreeFinC>.isolateLocal(_treeFinTrampoline)..keepIsolateAlive = false;
    _tx = NativeCallable<_TreeTxC>.isolateLocal(_treeTxTrampoline, exceptionalReturn: 0)..keepIsolateAlive = false;
    _bc.lib.lookupFunction<_TreeSetFinC, _TreeSetFinD>('blockchainer_chain_tree_set_finalized_hook')(_h, _fin.nativeFunction, Pointer<Void>.fromAddress(_handle));
    _bc.lib.lookupFunction<_TreeSetTxC, _TreeSetTxD>('blockchainer_chain_tree_set_tx_source')(_h, _tx.nativeFunction, Pointer<Void>.fromAddress(_handle));
  }
  final Blockchainer _bc;
  final Pointer<Void> _h;
  late final int _handle;
  late final NativeCallable<_TreeFinC> _fin;
  late final NativeCallable<_TreeTxC> _tx;
  void Function(int chainId, Uint8List block)? onFinalized;
  List<Uint8List> Function(int chainId, int timestamp)? txSource;
  List<int> chainIds() {
    final b = _bc._take(_bc.lib.lookupFunction<_ChainBufC, _ChainBufD>('blockchainer_chain_tree_chain_ids')(_h), 'chain_tree_chain_ids');
    final bd = ByteData.sublistView(b); final n = bd.getUint32(0);
    return List.generate(n, (i) => bd.getUint64(4 + 8 * i));
  }
  int height(int chainId) => _bc.lib.lookupFunction<_TreeU64IdC, _TreeU64IdD>('blockchainer_chain_tree_height')(_h, chainId);
  Uint8List genesis(int chainId) => _bc._take(_bc.lib.lookupFunction<_ScChainIdC, _ScChainIdD>('blockchainer_chain_tree_genesis')(_h, chainId), 'chain_tree_genesis');
  Uint8List tip(int chainId) => _bc._take(_bc.lib.lookupFunction<_ScChainIdC, _ScChainIdD>('blockchainer_chain_tree_tip')(_h, chainId), 'chain_tree_tip');
  int pendingAnchors(int chainId) => _bc.lib.lookupFunction<_TreeU32IdC, _TreeU32IdD>('blockchainer_chain_tree_pending_anchors')(_h, chainId);
  Uint8List produce(int chainId, int timestamp) => _bc._take(_bc.lib.lookupFunction<_TreeProduceC, _TreeProduceD>('blockchainer_chain_tree_produce')(_h, chainId, timestamp), 'chain_tree_produce');
  Uint8List proveAccount(int chainId, Uint8List address) {
    final p = _bc._alloc(address);
    try { return _bc._take(_bc.lib.lookupFunction<_TreeProveC, _TreeProveD>('blockchainer_chain_tree_prove_account')(_h, chainId, p), 'chain_tree_prove_account'); } finally { calloc.free(p); }
  }
  bool verify(Uint8List proof) {
    final p = _bc._alloc(proof);
    try { return _bc.lib.lookupFunction<_TreeVerifyC, _TreeVerifyD>('blockchainer_chain_tree_verify')(_h, p, proof.length) == 1; } finally { calloc.free(p); }
  }
  void free() { _bc.lib.lookupFunction<_ChainFreeC, _ChainFreeD>('blockchainer_chain_tree_free')(_h); _trees.remove(_handle); _fin.close(); _tx.close(); }
}
