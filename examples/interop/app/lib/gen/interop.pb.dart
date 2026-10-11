// This is a generated file - do not edit.
//
// Generated from interop.proto.

// @dart = 3.3

// ignore_for_file: annotate_overrides, camel_case_types, comment_references
// ignore_for_file: constant_identifier_names
// ignore_for_file: curly_braces_in_flow_control_structures
// ignore_for_file: deprecated_member_use_from_same_package, library_prefixes
// ignore_for_file: non_constant_identifier_names, prefer_relative_imports

import 'dart:core' as $core;

import 'package:fixnum/fixnum.dart' as $fixnum;
import 'package:protobuf/protobuf.dart' as $pb;

export 'package:protobuf/protobuf.dart' show GeneratedMessageGenericExtensions;

/// Inputs for the canonical-bytes handshake.
class CanonicalInput extends $pb.GeneratedMessage {
  factory CanonicalInput({
    $core.List<$core.int>? message,
    $core.int? txVersion,
    $fixnum.Int64? chainId,
    $core.List<$core.int>? txBody,
    $core.List<$core.int>? txAuthor,
    $core.List<$core.int>? txSignature,
    $core.int? hdrVersion,
    $fixnum.Int64? height,
    $core.List<$core.int>? parentHash,
    $fixnum.Int64? timestamp,
    $core.List<$core.int>? txRoot,
    $core.List<$core.int>? stateRoot,
    $core.List<$core.int>? consensus,
  }) {
    final result = create();
    if (message != null) result.message = message;
    if (txVersion != null) result.txVersion = txVersion;
    if (chainId != null) result.chainId = chainId;
    if (txBody != null) result.txBody = txBody;
    if (txAuthor != null) result.txAuthor = txAuthor;
    if (txSignature != null) result.txSignature = txSignature;
    if (hdrVersion != null) result.hdrVersion = hdrVersion;
    if (height != null) result.height = height;
    if (parentHash != null) result.parentHash = parentHash;
    if (timestamp != null) result.timestamp = timestamp;
    if (txRoot != null) result.txRoot = txRoot;
    if (stateRoot != null) result.stateRoot = stateRoot;
    if (consensus != null) result.consensus = consensus;
    return result;
  }

  CanonicalInput._();

  factory CanonicalInput.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CanonicalInput.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CanonicalInput',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..a<$core.List<$core.int>>(
        1, _omitFieldNames ? '' : 'message', $pb.PbFieldType.OY)
    ..aI(2, _omitFieldNames ? '' : 'txVersion', fieldType: $pb.PbFieldType.OU3)
    ..a<$fixnum.Int64>(3, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$core.List<$core.int>>(
        4, _omitFieldNames ? '' : 'txBody', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        5, _omitFieldNames ? '' : 'txAuthor', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        6, _omitFieldNames ? '' : 'txSignature', $pb.PbFieldType.OY)
    ..aI(7, _omitFieldNames ? '' : 'hdrVersion', fieldType: $pb.PbFieldType.OU3)
    ..a<$fixnum.Int64>(8, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$core.List<$core.int>>(
        9, _omitFieldNames ? '' : 'parentHash', $pb.PbFieldType.OY)
    ..a<$fixnum.Int64>(
        10, _omitFieldNames ? '' : 'timestamp', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$core.List<$core.int>>(
        11, _omitFieldNames ? '' : 'txRoot', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        12, _omitFieldNames ? '' : 'stateRoot', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        13, _omitFieldNames ? '' : 'consensus', $pb.PbFieldType.OY)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CanonicalInput clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CanonicalInput copyWith(void Function(CanonicalInput) updates) =>
      super.copyWith((message) => updates(message as CanonicalInput))
          as CanonicalInput;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CanonicalInput create() => CanonicalInput._();
  @$core.override
  CanonicalInput createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CanonicalInput getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CanonicalInput>(create);
  static CanonicalInput? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<$core.int> get message => $_getN(0);
  @$pb.TagNumber(1)
  set message($core.List<$core.int> value) => $_setBytes(0, value);
  @$pb.TagNumber(1)
  $core.bool hasMessage() => $_has(0);
  @$pb.TagNumber(1)
  void clearMessage() => $_clearField(1);

  /// canonical txid fields
  @$pb.TagNumber(2)
  $core.int get txVersion => $_getIZ(1);
  @$pb.TagNumber(2)
  set txVersion($core.int value) => $_setUnsignedInt32(1, value);
  @$pb.TagNumber(2)
  $core.bool hasTxVersion() => $_has(1);
  @$pb.TagNumber(2)
  void clearTxVersion() => $_clearField(2);

  @$pb.TagNumber(3)
  $fixnum.Int64 get chainId => $_getI64(2);
  @$pb.TagNumber(3)
  set chainId($fixnum.Int64 value) => $_setInt64(2, value);
  @$pb.TagNumber(3)
  $core.bool hasChainId() => $_has(2);
  @$pb.TagNumber(3)
  void clearChainId() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.List<$core.int> get txBody => $_getN(3);
  @$pb.TagNumber(4)
  set txBody($core.List<$core.int> value) => $_setBytes(3, value);
  @$pb.TagNumber(4)
  $core.bool hasTxBody() => $_has(3);
  @$pb.TagNumber(4)
  void clearTxBody() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.List<$core.int> get txAuthor => $_getN(4);
  @$pb.TagNumber(5)
  set txAuthor($core.List<$core.int> value) => $_setBytes(4, value);
  @$pb.TagNumber(5)
  $core.bool hasTxAuthor() => $_has(4);
  @$pb.TagNumber(5)
  void clearTxAuthor() => $_clearField(5);

  @$pb.TagNumber(6)
  $core.List<$core.int> get txSignature => $_getN(5);
  @$pb.TagNumber(6)
  set txSignature($core.List<$core.int> value) => $_setBytes(5, value);
  @$pb.TagNumber(6)
  $core.bool hasTxSignature() => $_has(5);
  @$pb.TagNumber(6)
  void clearTxSignature() => $_clearField(6);

  /// canonical header-hash fields
  @$pb.TagNumber(7)
  $core.int get hdrVersion => $_getIZ(6);
  @$pb.TagNumber(7)
  set hdrVersion($core.int value) => $_setUnsignedInt32(6, value);
  @$pb.TagNumber(7)
  $core.bool hasHdrVersion() => $_has(6);
  @$pb.TagNumber(7)
  void clearHdrVersion() => $_clearField(7);

  @$pb.TagNumber(8)
  $fixnum.Int64 get height => $_getI64(7);
  @$pb.TagNumber(8)
  set height($fixnum.Int64 value) => $_setInt64(7, value);
  @$pb.TagNumber(8)
  $core.bool hasHeight() => $_has(7);
  @$pb.TagNumber(8)
  void clearHeight() => $_clearField(8);

  @$pb.TagNumber(9)
  $core.List<$core.int> get parentHash => $_getN(8);
  @$pb.TagNumber(9)
  set parentHash($core.List<$core.int> value) => $_setBytes(8, value);
  @$pb.TagNumber(9)
  $core.bool hasParentHash() => $_has(8);
  @$pb.TagNumber(9)
  void clearParentHash() => $_clearField(9);

  @$pb.TagNumber(10)
  $fixnum.Int64 get timestamp => $_getI64(9);
  @$pb.TagNumber(10)
  set timestamp($fixnum.Int64 value) => $_setInt64(9, value);
  @$pb.TagNumber(10)
  $core.bool hasTimestamp() => $_has(9);
  @$pb.TagNumber(10)
  void clearTimestamp() => $_clearField(10);

  @$pb.TagNumber(11)
  $core.List<$core.int> get txRoot => $_getN(10);
  @$pb.TagNumber(11)
  set txRoot($core.List<$core.int> value) => $_setBytes(10, value);
  @$pb.TagNumber(11)
  $core.bool hasTxRoot() => $_has(10);
  @$pb.TagNumber(11)
  void clearTxRoot() => $_clearField(11);

  @$pb.TagNumber(12)
  $core.List<$core.int> get stateRoot => $_getN(11);
  @$pb.TagNumber(12)
  set stateRoot($core.List<$core.int> value) => $_setBytes(11, value);
  @$pb.TagNumber(12)
  $core.bool hasStateRoot() => $_has(11);
  @$pb.TagNumber(12)
  void clearStateRoot() => $_clearField(12);

  @$pb.TagNumber(13)
  $core.List<$core.int> get consensus => $_getN(12);
  @$pb.TagNumber(13)
  set consensus($core.List<$core.int> value) => $_setBytes(12, value);
  @$pb.TagNumber(13)
  $core.bool hasConsensus() => $_has(12);
  @$pb.TagNumber(13)
  void clearConsensus() => $_clearField(13);
}

class CanonicalOutput extends $pb.GeneratedMessage {
  factory CanonicalOutput({
    $core.List<$core.int>? hash,
    $core.List<$core.int>? txid,
    $core.List<$core.int>? headerHash,
  }) {
    final result = create();
    if (hash != null) result.hash = hash;
    if (txid != null) result.txid = txid;
    if (headerHash != null) result.headerHash = headerHash;
    return result;
  }

  CanonicalOutput._();

  factory CanonicalOutput.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CanonicalOutput.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CanonicalOutput',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..a<$core.List<$core.int>>(
        1, _omitFieldNames ? '' : 'hash', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        2, _omitFieldNames ? '' : 'txid', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        3, _omitFieldNames ? '' : 'headerHash', $pb.PbFieldType.OY)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CanonicalOutput clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CanonicalOutput copyWith(void Function(CanonicalOutput) updates) =>
      super.copyWith((message) => updates(message as CanonicalOutput))
          as CanonicalOutput;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CanonicalOutput create() => CanonicalOutput._();
  @$core.override
  CanonicalOutput createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CanonicalOutput getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CanonicalOutput>(create);
  static CanonicalOutput? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<$core.int> get hash => $_getN(0);
  @$pb.TagNumber(1)
  set hash($core.List<$core.int> value) => $_setBytes(0, value);
  @$pb.TagNumber(1)
  $core.bool hasHash() => $_has(0);
  @$pb.TagNumber(1)
  void clearHash() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.int> get txid => $_getN(1);
  @$pb.TagNumber(2)
  set txid($core.List<$core.int> value) => $_setBytes(1, value);
  @$pb.TagNumber(2)
  $core.bool hasTxid() => $_has(1);
  @$pb.TagNumber(2)
  void clearTxid() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.List<$core.int> get headerHash => $_getN(2);
  @$pb.TagNumber(3)
  set headerHash($core.List<$core.int> value) => $_setBytes(2, value);
  @$pb.TagNumber(3)
  $core.bool hasHeaderHash() => $_has(2);
  @$pb.TagNumber(3)
  void clearHeaderHash() => $_clearField(3);
}

/// A deterministic scenario both sides apply identically. Sharing the secrets
/// makes the accounts (and the Ed25519-signed transfers, which are deterministic)
/// byte-identical, so the resulting state roots must match.
class ChainScenario extends $pb.GeneratedMessage {
  factory ChainScenario({
    $fixnum.Int64? chainId,
    $core.List<$core.int>? aliceSecret,
    $core.List<$core.int>? bobSecret,
    $fixnum.Int64? aliceCredit,
    $fixnum.Int64? transferAmount,
    $fixnum.Int64? timestamp,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (aliceSecret != null) result.aliceSecret = aliceSecret;
    if (bobSecret != null) result.bobSecret = bobSecret;
    if (aliceCredit != null) result.aliceCredit = aliceCredit;
    if (transferAmount != null) result.transferAmount = transferAmount;
    if (timestamp != null) result.timestamp = timestamp;
    return result;
  }

  ChainScenario._();

  factory ChainScenario.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ChainScenario.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ChainScenario',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$core.List<$core.int>>(
        2, _omitFieldNames ? '' : 'aliceSecret', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        3, _omitFieldNames ? '' : 'bobSecret', $pb.PbFieldType.OY)
    ..a<$fixnum.Int64>(
        4, _omitFieldNames ? '' : 'aliceCredit', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(
        5, _omitFieldNames ? '' : 'transferAmount', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(
        6, _omitFieldNames ? '' : 'timestamp', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ChainScenario clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ChainScenario copyWith(void Function(ChainScenario) updates) =>
      super.copyWith((message) => updates(message as ChainScenario))
          as ChainScenario;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ChainScenario create() => ChainScenario._();
  @$core.override
  ChainScenario createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ChainScenario getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ChainScenario>(create);
  static ChainScenario? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.int> get aliceSecret => $_getN(1);
  @$pb.TagNumber(2)
  set aliceSecret($core.List<$core.int> value) => $_setBytes(1, value);
  @$pb.TagNumber(2)
  $core.bool hasAliceSecret() => $_has(1);
  @$pb.TagNumber(2)
  void clearAliceSecret() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.List<$core.int> get bobSecret => $_getN(2);
  @$pb.TagNumber(3)
  set bobSecret($core.List<$core.int> value) => $_setBytes(2, value);
  @$pb.TagNumber(3)
  $core.bool hasBobSecret() => $_has(2);
  @$pb.TagNumber(3)
  void clearBobSecret() => $_clearField(3);

  @$pb.TagNumber(4)
  $fixnum.Int64 get aliceCredit => $_getI64(3);
  @$pb.TagNumber(4)
  set aliceCredit($fixnum.Int64 value) => $_setInt64(3, value);
  @$pb.TagNumber(4)
  $core.bool hasAliceCredit() => $_has(3);
  @$pb.TagNumber(4)
  void clearAliceCredit() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get transferAmount => $_getI64(4);
  @$pb.TagNumber(5)
  set transferAmount($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasTransferAmount() => $_has(4);
  @$pb.TagNumber(5)
  void clearTransferAmount() => $_clearField(5);

  @$pb.TagNumber(6)
  $fixnum.Int64 get timestamp => $_getI64(5);
  @$pb.TagNumber(6)
  set timestamp($fixnum.Int64 value) => $_setInt64(5, value);
  @$pb.TagNumber(6)
  $core.bool hasTimestamp() => $_has(5);
  @$pb.TagNumber(6)
  void clearTimestamp() => $_clearField(6);
}

class ChainResult extends $pb.GeneratedMessage {
  factory ChainResult({
    $fixnum.Int64? height,
    $core.List<$core.int>? stateRoot,
    $core.List<$core.int>? aliceAddress,
    $core.List<$core.int>? bobAddress,
    $fixnum.Int64? aliceBalance,
    $fixnum.Int64? bobBalance,
    $core.List<$core.int>? proof,
  }) {
    final result = create();
    if (height != null) result.height = height;
    if (stateRoot != null) result.stateRoot = stateRoot;
    if (aliceAddress != null) result.aliceAddress = aliceAddress;
    if (bobAddress != null) result.bobAddress = bobAddress;
    if (aliceBalance != null) result.aliceBalance = aliceBalance;
    if (bobBalance != null) result.bobBalance = bobBalance;
    if (proof != null) result.proof = proof;
    return result;
  }

  ChainResult._();

  factory ChainResult.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ChainResult.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ChainResult',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$core.List<$core.int>>(
        2, _omitFieldNames ? '' : 'stateRoot', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        3, _omitFieldNames ? '' : 'aliceAddress', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        4, _omitFieldNames ? '' : 'bobAddress', $pb.PbFieldType.OY)
    ..a<$fixnum.Int64>(
        5, _omitFieldNames ? '' : 'aliceBalance', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(
        6, _omitFieldNames ? '' : 'bobBalance', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$core.List<$core.int>>(
        7, _omitFieldNames ? '' : 'proof', $pb.PbFieldType.OY)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ChainResult clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ChainResult copyWith(void Function(ChainResult) updates) =>
      super.copyWith((message) => updates(message as ChainResult))
          as ChainResult;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ChainResult create() => ChainResult._();
  @$core.override
  ChainResult createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ChainResult getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ChainResult>(create);
  static ChainResult? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get height => $_getI64(0);
  @$pb.TagNumber(1)
  set height($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasHeight() => $_has(0);
  @$pb.TagNumber(1)
  void clearHeight() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.int> get stateRoot => $_getN(1);
  @$pb.TagNumber(2)
  set stateRoot($core.List<$core.int> value) => $_setBytes(1, value);
  @$pb.TagNumber(2)
  $core.bool hasStateRoot() => $_has(1);
  @$pb.TagNumber(2)
  void clearStateRoot() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.List<$core.int> get aliceAddress => $_getN(2);
  @$pb.TagNumber(3)
  set aliceAddress($core.List<$core.int> value) => $_setBytes(2, value);
  @$pb.TagNumber(3)
  $core.bool hasAliceAddress() => $_has(2);
  @$pb.TagNumber(3)
  void clearAliceAddress() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.List<$core.int> get bobAddress => $_getN(3);
  @$pb.TagNumber(4)
  set bobAddress($core.List<$core.int> value) => $_setBytes(3, value);
  @$pb.TagNumber(4)
  $core.bool hasBobAddress() => $_has(3);
  @$pb.TagNumber(4)
  void clearBobAddress() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get aliceBalance => $_getI64(4);
  @$pb.TagNumber(5)
  set aliceBalance($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasAliceBalance() => $_has(4);
  @$pb.TagNumber(5)
  void clearAliceBalance() => $_clearField(5);

  @$pb.TagNumber(6)
  $fixnum.Int64 get bobBalance => $_getI64(5);
  @$pb.TagNumber(6)
  set bobBalance($fixnum.Int64 value) => $_setInt64(5, value);
  @$pb.TagNumber(6)
  $core.bool hasBobBalance() => $_has(5);
  @$pb.TagNumber(6)
  void clearBobBalance() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.List<$core.int> get proof => $_getN(6);
  @$pb.TagNumber(7)
  set proof($core.List<$core.int> value) => $_setBytes(6, value);
  @$pb.TagNumber(7)
  $core.bool hasProof() => $_has(6);
  @$pb.TagNumber(7)
  void clearProof() => $_clearField(7);
}

class ProofBundle extends $pb.GeneratedMessage {
  factory ProofBundle({
    $core.List<$core.int>? stateRoot,
    $core.List<$core.int>? proof,
  }) {
    final result = create();
    if (stateRoot != null) result.stateRoot = stateRoot;
    if (proof != null) result.proof = proof;
    return result;
  }

  ProofBundle._();

  factory ProofBundle.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ProofBundle.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ProofBundle',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..a<$core.List<$core.int>>(
        1, _omitFieldNames ? '' : 'stateRoot', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        2, _omitFieldNames ? '' : 'proof', $pb.PbFieldType.OY)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ProofBundle clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ProofBundle copyWith(void Function(ProofBundle) updates) =>
      super.copyWith((message) => updates(message as ProofBundle))
          as ProofBundle;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ProofBundle create() => ProofBundle._();
  @$core.override
  ProofBundle createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ProofBundle getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<ProofBundle>(create);
  static ProofBundle? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<$core.int> get stateRoot => $_getN(0);
  @$pb.TagNumber(1)
  set stateRoot($core.List<$core.int> value) => $_setBytes(0, value);
  @$pb.TagNumber(1)
  $core.bool hasStateRoot() => $_has(0);
  @$pb.TagNumber(1)
  void clearStateRoot() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.int> get proof => $_getN(1);
  @$pb.TagNumber(2)
  set proof($core.List<$core.int> value) => $_setBytes(1, value);
  @$pb.TagNumber(2)
  $core.bool hasProof() => $_has(1);
  @$pb.TagNumber(2)
  void clearProof() => $_clearField(2);
}

class VerifyResult extends $pb.GeneratedMessage {
  factory VerifyResult({
    $core.bool? ok,
  }) {
    final result = create();
    if (ok != null) result.ok = ok;
    return result;
  }

  VerifyResult._();

  factory VerifyResult.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory VerifyResult.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'VerifyResult',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'ok')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  VerifyResult clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  VerifyResult copyWith(void Function(VerifyResult) updates) =>
      super.copyWith((message) => updates(message as VerifyResult))
          as VerifyResult;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static VerifyResult create() => VerifyResult._();
  @$core.override
  VerifyResult createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static VerifyResult getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<VerifyResult>(create);
  static VerifyResult? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get ok => $_getBF(0);
  @$pb.TagNumber(1)
  set ok($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasOk() => $_has(0);
  @$pb.TagNumber(1)
  void clearOk() => $_clearField(1);
}

/// -- Chain tree -----------------------------------------------------------------
class TreeScenario extends $pb.GeneratedMessage {
  factory TreeScenario({
    $core.List<$core.int>? seed,
    $core.List<$core.int>? aliceAddress,
    $fixnum.Int64? aliceCredit,
  }) {
    final result = create();
    if (seed != null) result.seed = seed;
    if (aliceAddress != null) result.aliceAddress = aliceAddress;
    if (aliceCredit != null) result.aliceCredit = aliceCredit;
    return result;
  }

  TreeScenario._();

  factory TreeScenario.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory TreeScenario.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'TreeScenario',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..a<$core.List<$core.int>>(
        1, _omitFieldNames ? '' : 'seed', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        2, _omitFieldNames ? '' : 'aliceAddress', $pb.PbFieldType.OY)
    ..a<$fixnum.Int64>(
        3, _omitFieldNames ? '' : 'aliceCredit', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TreeScenario clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TreeScenario copyWith(void Function(TreeScenario) updates) =>
      super.copyWith((message) => updates(message as TreeScenario))
          as TreeScenario;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static TreeScenario create() => TreeScenario._();
  @$core.override
  TreeScenario createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static TreeScenario getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<TreeScenario>(create);
  static TreeScenario? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<$core.int> get seed => $_getN(0);
  @$pb.TagNumber(1)
  set seed($core.List<$core.int> value) => $_setBytes(0, value);
  @$pb.TagNumber(1)
  $core.bool hasSeed() => $_has(0);
  @$pb.TagNumber(1)
  void clearSeed() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.int> get aliceAddress => $_getN(1);
  @$pb.TagNumber(2)
  set aliceAddress($core.List<$core.int> value) => $_setBytes(1, value);
  @$pb.TagNumber(2)
  $core.bool hasAliceAddress() => $_has(1);
  @$pb.TagNumber(2)
  void clearAliceAddress() => $_clearField(2);

  @$pb.TagNumber(3)
  $fixnum.Int64 get aliceCredit => $_getI64(2);
  @$pb.TagNumber(3)
  set aliceCredit($fixnum.Int64 value) => $_setInt64(2, value);
  @$pb.TagNumber(3)
  $core.bool hasAliceCredit() => $_has(2);
  @$pb.TagNumber(3)
  void clearAliceCredit() => $_clearField(3);
}

class TreeResult extends $pb.GeneratedMessage {
  factory TreeResult({
    $core.Iterable<$fixnum.Int64>? chainIds,
    $core.List<$core.int>? rootSubchainRoot,
    $core.List<$core.int>? rootTipHeader,
    $core.List<$core.int>? proof,
    $fixnum.Int64? rootHeight,
  }) {
    final result = create();
    if (chainIds != null) result.chainIds.addAll(chainIds);
    if (rootSubchainRoot != null) result.rootSubchainRoot = rootSubchainRoot;
    if (rootTipHeader != null) result.rootTipHeader = rootTipHeader;
    if (proof != null) result.proof = proof;
    if (rootHeight != null) result.rootHeight = rootHeight;
    return result;
  }

  TreeResult._();

  factory TreeResult.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory TreeResult.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'TreeResult',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..p<$fixnum.Int64>(
        1, _omitFieldNames ? '' : 'chainIds', $pb.PbFieldType.KU6)
    ..a<$core.List<$core.int>>(
        2, _omitFieldNames ? '' : 'rootSubchainRoot', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        3, _omitFieldNames ? '' : 'rootTipHeader', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        4, _omitFieldNames ? '' : 'proof', $pb.PbFieldType.OY)
    ..a<$fixnum.Int64>(
        5, _omitFieldNames ? '' : 'rootHeight', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TreeResult clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TreeResult copyWith(void Function(TreeResult) updates) =>
      super.copyWith((message) => updates(message as TreeResult)) as TreeResult;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static TreeResult create() => TreeResult._();
  @$core.override
  TreeResult createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static TreeResult getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<TreeResult>(create);
  static TreeResult? _defaultInstance;

  @$pb.TagNumber(1)
  $pb.PbList<$fixnum.Int64> get chainIds => $_getList(0);

  @$pb.TagNumber(2)
  $core.List<$core.int> get rootSubchainRoot => $_getN(1);
  @$pb.TagNumber(2)
  set rootSubchainRoot($core.List<$core.int> value) => $_setBytes(1, value);
  @$pb.TagNumber(2)
  $core.bool hasRootSubchainRoot() => $_has(1);
  @$pb.TagNumber(2)
  void clearRootSubchainRoot() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.List<$core.int> get rootTipHeader => $_getN(2);
  @$pb.TagNumber(3)
  set rootTipHeader($core.List<$core.int> value) => $_setBytes(2, value);
  @$pb.TagNumber(3)
  $core.bool hasRootTipHeader() => $_has(2);
  @$pb.TagNumber(3)
  void clearRootTipHeader() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.List<$core.int> get proof => $_getN(3);
  @$pb.TagNumber(4)
  set proof($core.List<$core.int> value) => $_setBytes(3, value);
  @$pb.TagNumber(4)
  $core.bool hasProof() => $_has(3);
  @$pb.TagNumber(4)
  void clearProof() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get rootHeight => $_getI64(4);
  @$pb.TagNumber(5)
  set rootHeight($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasRootHeight() => $_has(4);
  @$pb.TagNumber(5)
  void clearRootHeight() => $_clearField(5);
}

class TreeProofBundle extends $pb.GeneratedMessage {
  factory TreeProofBundle({
    $core.List<$core.int>? rootSubchainRoot,
    $core.List<$core.int>? proof,
  }) {
    final result = create();
    if (rootSubchainRoot != null) result.rootSubchainRoot = rootSubchainRoot;
    if (proof != null) result.proof = proof;
    return result;
  }

  TreeProofBundle._();

  factory TreeProofBundle.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory TreeProofBundle.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'TreeProofBundle',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'interop.v1'),
      createEmptyInstance: create)
    ..a<$core.List<$core.int>>(
        1, _omitFieldNames ? '' : 'rootSubchainRoot', $pb.PbFieldType.OY)
    ..a<$core.List<$core.int>>(
        2, _omitFieldNames ? '' : 'proof', $pb.PbFieldType.OY)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TreeProofBundle clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  TreeProofBundle copyWith(void Function(TreeProofBundle) updates) =>
      super.copyWith((message) => updates(message as TreeProofBundle))
          as TreeProofBundle;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static TreeProofBundle create() => TreeProofBundle._();
  @$core.override
  TreeProofBundle createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static TreeProofBundle getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<TreeProofBundle>(create);
  static TreeProofBundle? _defaultInstance;

  @$pb.TagNumber(1)
  $core.List<$core.int> get rootSubchainRoot => $_getN(0);
  @$pb.TagNumber(1)
  set rootSubchainRoot($core.List<$core.int> value) => $_setBytes(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRootSubchainRoot() => $_has(0);
  @$pb.TagNumber(1)
  void clearRootSubchainRoot() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.List<$core.int> get proof => $_getN(1);
  @$pb.TagNumber(2)
  set proof($core.List<$core.int> value) => $_setBytes(1, value);
  @$pb.TagNumber(2)
  $core.bool hasProof() => $_has(1);
  @$pb.TagNumber(2)
  void clearProof() => $_clearField(2);
}

const $core.bool _omitFieldNames =
    $core.bool.fromEnvironment('protobuf.omit_field_names');
const $core.bool _omitMessageNames =
    $core.bool.fromEnvironment('protobuf.omit_message_names');
