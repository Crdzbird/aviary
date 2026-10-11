// This is a generated file - do not edit.
//
// Generated from atlas.proto.

// @dart = 3.3

// ignore_for_file: annotate_overrides, camel_case_types, comment_references
// ignore_for_file: constant_identifier_names
// ignore_for_file: curly_braces_in_flow_control_structures
// ignore_for_file: deprecated_member_use_from_same_package, library_prefixes
// ignore_for_file: non_constant_identifier_names, prefer_relative_imports

import 'dart:core' as $core;

import 'package:fixnum/fixnum.dart' as $fixnum;
import 'package:protobuf/protobuf.dart' as $pb;

import 'atlas.pbenum.dart';

export 'package:protobuf/protobuf.dart' show GeneratedMessageGenericExtensions;

export 'atlas.pbenum.dart';

class Empty extends $pb.GeneratedMessage {
  factory Empty() => create();

  Empty._();

  factory Empty.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Empty.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Empty',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Empty clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Empty copyWith(void Function(Empty) updates) =>
      super.copyWith((message) => updates(message as Empty)) as Empty;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Empty create() => Empty._();
  @$core.override
  Empty createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Empty getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Empty>(create);
  static Empty? _defaultInstance;
}

class CreateChainRequest extends $pb.GeneratedMessage {
  factory CreateChainRequest({
    $core.String? name,
    $fixnum.Int64? chainId,
    $core.String? nativeSymbol,
  }) {
    final result = create();
    if (name != null) result.name = name;
    if (chainId != null) result.chainId = chainId;
    if (nativeSymbol != null) result.nativeSymbol = nativeSymbol;
    return result;
  }

  CreateChainRequest._();

  factory CreateChainRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CreateChainRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CreateChainRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'name')
    ..a<$fixnum.Int64>(2, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(3, _omitFieldNames ? '' : 'nativeSymbol')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CreateChainRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CreateChainRequest copyWith(void Function(CreateChainRequest) updates) =>
      super.copyWith((message) => updates(message as CreateChainRequest))
          as CreateChainRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CreateChainRequest create() => CreateChainRequest._();
  @$core.override
  CreateChainRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CreateChainRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CreateChainRequest>(create);
  static CreateChainRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get name => $_getSZ(0);
  @$pb.TagNumber(1)
  set name($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasName() => $_has(0);
  @$pb.TagNumber(1)
  void clearName() => $_clearField(1);

  @$pb.TagNumber(2)
  $fixnum.Int64 get chainId => $_getI64(1);
  @$pb.TagNumber(2)
  set chainId($fixnum.Int64 value) => $_setInt64(1, value);
  @$pb.TagNumber(2)
  $core.bool hasChainId() => $_has(1);
  @$pb.TagNumber(2)
  void clearChainId() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get nativeSymbol => $_getSZ(2);
  @$pb.TagNumber(3)
  set nativeSymbol($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasNativeSymbol() => $_has(2);
  @$pb.TagNumber(3)
  void clearNativeSymbol() => $_clearField(3);
}

class Chain extends $pb.GeneratedMessage {
  factory Chain({
    $core.String? name,
    $fixnum.Int64? chainId,
    $core.String? nativeSymbol,
    $fixnum.Int64? height,
    $core.int? walletCount,
    $fixnum.Int64? totalIssued,
  }) {
    final result = create();
    if (name != null) result.name = name;
    if (chainId != null) result.chainId = chainId;
    if (nativeSymbol != null) result.nativeSymbol = nativeSymbol;
    if (height != null) result.height = height;
    if (walletCount != null) result.walletCount = walletCount;
    if (totalIssued != null) result.totalIssued = totalIssued;
    return result;
  }

  Chain._();

  factory Chain.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Chain.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Chain',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'name')
    ..a<$fixnum.Int64>(2, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(3, _omitFieldNames ? '' : 'nativeSymbol')
    ..a<$fixnum.Int64>(4, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aI(5, _omitFieldNames ? '' : 'walletCount',
        fieldType: $pb.PbFieldType.OU3)
    ..a<$fixnum.Int64>(
        6, _omitFieldNames ? '' : 'totalIssued', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Chain clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Chain copyWith(void Function(Chain) updates) =>
      super.copyWith((message) => updates(message as Chain)) as Chain;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Chain create() => Chain._();
  @$core.override
  Chain createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Chain getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Chain>(create);
  static Chain? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get name => $_getSZ(0);
  @$pb.TagNumber(1)
  set name($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasName() => $_has(0);
  @$pb.TagNumber(1)
  void clearName() => $_clearField(1);

  @$pb.TagNumber(2)
  $fixnum.Int64 get chainId => $_getI64(1);
  @$pb.TagNumber(2)
  set chainId($fixnum.Int64 value) => $_setInt64(1, value);
  @$pb.TagNumber(2)
  $core.bool hasChainId() => $_has(1);
  @$pb.TagNumber(2)
  void clearChainId() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get nativeSymbol => $_getSZ(2);
  @$pb.TagNumber(3)
  set nativeSymbol($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasNativeSymbol() => $_has(2);
  @$pb.TagNumber(3)
  void clearNativeSymbol() => $_clearField(3);

  @$pb.TagNumber(4)
  $fixnum.Int64 get height => $_getI64(3);
  @$pb.TagNumber(4)
  set height($fixnum.Int64 value) => $_setInt64(3, value);
  @$pb.TagNumber(4)
  $core.bool hasHeight() => $_has(3);
  @$pb.TagNumber(4)
  void clearHeight() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.int get walletCount => $_getIZ(4);
  @$pb.TagNumber(5)
  set walletCount($core.int value) => $_setUnsignedInt32(4, value);
  @$pb.TagNumber(5)
  $core.bool hasWalletCount() => $_has(4);
  @$pb.TagNumber(5)
  void clearWalletCount() => $_clearField(5);

  @$pb.TagNumber(6)
  $fixnum.Int64 get totalIssued => $_getI64(5);
  @$pb.TagNumber(6)
  set totalIssued($fixnum.Int64 value) => $_setInt64(5, value);
  @$pb.TagNumber(6)
  $core.bool hasTotalIssued() => $_has(5);
  @$pb.TagNumber(6)
  void clearTotalIssued() => $_clearField(6);
}

class ChainList extends $pb.GeneratedMessage {
  factory ChainList({
    $core.Iterable<Chain>? chains,
  }) {
    final result = create();
    if (chains != null) result.chains.addAll(chains);
    return result;
  }

  ChainList._();

  factory ChainList.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ChainList.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ChainList',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..pPM<Chain>(1, _omitFieldNames ? '' : 'chains', subBuilder: Chain.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ChainList clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ChainList copyWith(void Function(ChainList) updates) =>
      super.copyWith((message) => updates(message as ChainList)) as ChainList;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ChainList create() => ChainList._();
  @$core.override
  ChainList createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ChainList getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<ChainList>(create);
  static ChainList? _defaultInstance;

  @$pb.TagNumber(1)
  $pb.PbList<Chain> get chains => $_getList(0);
}

class ChainRef extends $pb.GeneratedMessage {
  factory ChainRef({
    $fixnum.Int64? chainId,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    return result;
  }

  ChainRef._();

  factory ChainRef.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory ChainRef.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'ChainRef',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ChainRef clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  ChainRef copyWith(void Function(ChainRef) updates) =>
      super.copyWith((message) => updates(message as ChainRef)) as ChainRef;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static ChainRef create() => ChainRef._();
  @$core.override
  ChainRef createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static ChainRef getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<ChainRef>(create);
  static ChainRef? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);
}

class CreateWalletRequest extends $pb.GeneratedMessage {
  factory CreateWalletRequest({
    $fixnum.Int64? chainId,
    $core.String? label,
    $core.String? institution,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (label != null) result.label = label;
    if (institution != null) result.institution = institution;
    return result;
  }

  CreateWalletRequest._();

  factory CreateWalletRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CreateWalletRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CreateWalletRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'label')
    ..aOS(3, _omitFieldNames ? '' : 'institution')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CreateWalletRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CreateWalletRequest copyWith(void Function(CreateWalletRequest) updates) =>
      super.copyWith((message) => updates(message as CreateWalletRequest))
          as CreateWalletRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CreateWalletRequest create() => CreateWalletRequest._();
  @$core.override
  CreateWalletRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CreateWalletRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CreateWalletRequest>(create);
  static CreateWalletRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get label => $_getSZ(1);
  @$pb.TagNumber(2)
  set label($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasLabel() => $_has(1);
  @$pb.TagNumber(2)
  void clearLabel() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get institution => $_getSZ(2);
  @$pb.TagNumber(3)
  set institution($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasInstitution() => $_has(2);
  @$pb.TagNumber(3)
  void clearInstitution() => $_clearField(3);
}

class Wallet extends $pb.GeneratedMessage {
  factory Wallet({
    $core.String? id,
    $core.String? label,
    $core.String? institution,
    $core.String? address,
    $fixnum.Int64? balance,
    $fixnum.Int64? nonce,
    $core.String? ethAddress,
    $core.String? btcAddress,
    $core.String? solAddress,
  }) {
    final result = create();
    if (id != null) result.id = id;
    if (label != null) result.label = label;
    if (institution != null) result.institution = institution;
    if (address != null) result.address = address;
    if (balance != null) result.balance = balance;
    if (nonce != null) result.nonce = nonce;
    if (ethAddress != null) result.ethAddress = ethAddress;
    if (btcAddress != null) result.btcAddress = btcAddress;
    if (solAddress != null) result.solAddress = solAddress;
    return result;
  }

  Wallet._();

  factory Wallet.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Wallet.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Wallet',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'id')
    ..aOS(2, _omitFieldNames ? '' : 'label')
    ..aOS(3, _omitFieldNames ? '' : 'institution')
    ..aOS(4, _omitFieldNames ? '' : 'address')
    ..a<$fixnum.Int64>(5, _omitFieldNames ? '' : 'balance', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(6, _omitFieldNames ? '' : 'nonce', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(7, _omitFieldNames ? '' : 'ethAddress')
    ..aOS(8, _omitFieldNames ? '' : 'btcAddress')
    ..aOS(9, _omitFieldNames ? '' : 'solAddress')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Wallet clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Wallet copyWith(void Function(Wallet) updates) =>
      super.copyWith((message) => updates(message as Wallet)) as Wallet;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Wallet create() => Wallet._();
  @$core.override
  Wallet createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Wallet getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Wallet>(create);
  static Wallet? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get id => $_getSZ(0);
  @$pb.TagNumber(1)
  set id($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasId() => $_has(0);
  @$pb.TagNumber(1)
  void clearId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get label => $_getSZ(1);
  @$pb.TagNumber(2)
  set label($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasLabel() => $_has(1);
  @$pb.TagNumber(2)
  void clearLabel() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get institution => $_getSZ(2);
  @$pb.TagNumber(3)
  set institution($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasInstitution() => $_has(2);
  @$pb.TagNumber(3)
  void clearInstitution() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.String get address => $_getSZ(3);
  @$pb.TagNumber(4)
  set address($core.String value) => $_setString(3, value);
  @$pb.TagNumber(4)
  $core.bool hasAddress() => $_has(3);
  @$pb.TagNumber(4)
  void clearAddress() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get balance => $_getI64(4);
  @$pb.TagNumber(5)
  set balance($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasBalance() => $_has(4);
  @$pb.TagNumber(5)
  void clearBalance() => $_clearField(5);

  @$pb.TagNumber(6)
  $fixnum.Int64 get nonce => $_getI64(5);
  @$pb.TagNumber(6)
  set nonce($fixnum.Int64 value) => $_setInt64(5, value);
  @$pb.TagNumber(6)
  $core.bool hasNonce() => $_has(5);
  @$pb.TagNumber(6)
  void clearNonce() => $_clearField(6);

  /// Cross-chain receive addresses (for bridging external assets in).
  @$pb.TagNumber(7)
  $core.String get ethAddress => $_getSZ(6);
  @$pb.TagNumber(7)
  set ethAddress($core.String value) => $_setString(6, value);
  @$pb.TagNumber(7)
  $core.bool hasEthAddress() => $_has(6);
  @$pb.TagNumber(7)
  void clearEthAddress() => $_clearField(7);

  @$pb.TagNumber(8)
  $core.String get btcAddress => $_getSZ(7);
  @$pb.TagNumber(8)
  set btcAddress($core.String value) => $_setString(7, value);
  @$pb.TagNumber(8)
  $core.bool hasBtcAddress() => $_has(7);
  @$pb.TagNumber(8)
  void clearBtcAddress() => $_clearField(8);

  @$pb.TagNumber(9)
  $core.String get solAddress => $_getSZ(8);
  @$pb.TagNumber(9)
  set solAddress($core.String value) => $_setString(8, value);
  @$pb.TagNumber(9)
  $core.bool hasSolAddress() => $_has(8);
  @$pb.TagNumber(9)
  void clearSolAddress() => $_clearField(9);
}

class WalletList extends $pb.GeneratedMessage {
  factory WalletList({
    $core.Iterable<Wallet>? wallets,
  }) {
    final result = create();
    if (wallets != null) result.wallets.addAll(wallets);
    return result;
  }

  WalletList._();

  factory WalletList.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory WalletList.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'WalletList',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..pPM<Wallet>(1, _omitFieldNames ? '' : 'wallets',
        subBuilder: Wallet.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  WalletList clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  WalletList copyWith(void Function(WalletList) updates) =>
      super.copyWith((message) => updates(message as WalletList)) as WalletList;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static WalletList create() => WalletList._();
  @$core.override
  WalletList createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static WalletList getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<WalletList>(create);
  static WalletList? _defaultInstance;

  @$pb.TagNumber(1)
  $pb.PbList<Wallet> get wallets => $_getList(0);
}

class WalletRef extends $pb.GeneratedMessage {
  factory WalletRef({
    $fixnum.Int64? chainId,
    $core.String? walletId,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (walletId != null) result.walletId = walletId;
    return result;
  }

  WalletRef._();

  factory WalletRef.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory WalletRef.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'WalletRef',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'walletId')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  WalletRef clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  WalletRef copyWith(void Function(WalletRef) updates) =>
      super.copyWith((message) => updates(message as WalletRef)) as WalletRef;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static WalletRef create() => WalletRef._();
  @$core.override
  WalletRef createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static WalletRef getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<WalletRef>(create);
  static WalletRef? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get walletId => $_getSZ(1);
  @$pb.TagNumber(2)
  set walletId($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasWalletId() => $_has(1);
  @$pb.TagNumber(2)
  void clearWalletId() => $_clearField(2);
}

class IssueRequest extends $pb.GeneratedMessage {
  factory IssueRequest({
    $fixnum.Int64? chainId,
    $core.String? walletId,
    $fixnum.Int64? amount,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (walletId != null) result.walletId = walletId;
    if (amount != null) result.amount = amount;
    return result;
  }

  IssueRequest._();

  factory IssueRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory IssueRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'IssueRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'walletId')
    ..a<$fixnum.Int64>(3, _omitFieldNames ? '' : 'amount', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  IssueRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  IssueRequest copyWith(void Function(IssueRequest) updates) =>
      super.copyWith((message) => updates(message as IssueRequest))
          as IssueRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static IssueRequest create() => IssueRequest._();
  @$core.override
  IssueRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static IssueRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<IssueRequest>(create);
  static IssueRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get walletId => $_getSZ(1);
  @$pb.TagNumber(2)
  set walletId($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasWalletId() => $_has(1);
  @$pb.TagNumber(2)
  void clearWalletId() => $_clearField(2);

  @$pb.TagNumber(3)
  $fixnum.Int64 get amount => $_getI64(2);
  @$pb.TagNumber(3)
  set amount($fixnum.Int64 value) => $_setInt64(2, value);
  @$pb.TagNumber(3)
  $core.bool hasAmount() => $_has(2);
  @$pb.TagNumber(3)
  void clearAmount() => $_clearField(3);
}

class SettleRequest extends $pb.GeneratedMessage {
  factory SettleRequest({
    $fixnum.Int64? chainId,
    $core.String? fromWalletId,
    $core.String? toWalletId,
    $fixnum.Int64? amount,
    $core.String? memo,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (fromWalletId != null) result.fromWalletId = fromWalletId;
    if (toWalletId != null) result.toWalletId = toWalletId;
    if (amount != null) result.amount = amount;
    if (memo != null) result.memo = memo;
    return result;
  }

  SettleRequest._();

  factory SettleRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SettleRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SettleRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'fromWalletId')
    ..aOS(3, _omitFieldNames ? '' : 'toWalletId')
    ..a<$fixnum.Int64>(4, _omitFieldNames ? '' : 'amount', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(5, _omitFieldNames ? '' : 'memo')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SettleRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SettleRequest copyWith(void Function(SettleRequest) updates) =>
      super.copyWith((message) => updates(message as SettleRequest))
          as SettleRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SettleRequest create() => SettleRequest._();
  @$core.override
  SettleRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SettleRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SettleRequest>(create);
  static SettleRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get fromWalletId => $_getSZ(1);
  @$pb.TagNumber(2)
  set fromWalletId($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasFromWalletId() => $_has(1);
  @$pb.TagNumber(2)
  void clearFromWalletId() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get toWalletId => $_getSZ(2);
  @$pb.TagNumber(3)
  set toWalletId($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasToWalletId() => $_has(2);
  @$pb.TagNumber(3)
  void clearToWalletId() => $_clearField(3);

  @$pb.TagNumber(4)
  $fixnum.Int64 get amount => $_getI64(3);
  @$pb.TagNumber(4)
  set amount($fixnum.Int64 value) => $_setInt64(3, value);
  @$pb.TagNumber(4)
  $core.bool hasAmount() => $_has(3);
  @$pb.TagNumber(4)
  void clearAmount() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.String get memo => $_getSZ(4);
  @$pb.TagNumber(5)
  set memo($core.String value) => $_setString(4, value);
  @$pb.TagNumber(5)
  $core.bool hasMemo() => $_has(4);
  @$pb.TagNumber(5)
  void clearMemo() => $_clearField(5);
}

class SettlementReceipt extends $pb.GeneratedMessage {
  factory SettlementReceipt({
    $core.String? txid,
    $fixnum.Int64? chainId,
    $core.String? fromAddress,
    $core.String? toAddress,
    $fixnum.Int64? amount,
    $core.bool? finalized,
  }) {
    final result = create();
    if (txid != null) result.txid = txid;
    if (chainId != null) result.chainId = chainId;
    if (fromAddress != null) result.fromAddress = fromAddress;
    if (toAddress != null) result.toAddress = toAddress;
    if (amount != null) result.amount = amount;
    if (finalized != null) result.finalized = finalized;
    return result;
  }

  SettlementReceipt._();

  factory SettlementReceipt.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SettlementReceipt.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SettlementReceipt',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'txid')
    ..a<$fixnum.Int64>(2, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(3, _omitFieldNames ? '' : 'fromAddress')
    ..aOS(4, _omitFieldNames ? '' : 'toAddress')
    ..a<$fixnum.Int64>(5, _omitFieldNames ? '' : 'amount', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOB(6, _omitFieldNames ? '' : 'finalized')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SettlementReceipt clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SettlementReceipt copyWith(void Function(SettlementReceipt) updates) =>
      super.copyWith((message) => updates(message as SettlementReceipt))
          as SettlementReceipt;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SettlementReceipt create() => SettlementReceipt._();
  @$core.override
  SettlementReceipt createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SettlementReceipt getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SettlementReceipt>(create);
  static SettlementReceipt? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get txid => $_getSZ(0);
  @$pb.TagNumber(1)
  set txid($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasTxid() => $_has(0);
  @$pb.TagNumber(1)
  void clearTxid() => $_clearField(1);

  @$pb.TagNumber(2)
  $fixnum.Int64 get chainId => $_getI64(1);
  @$pb.TagNumber(2)
  set chainId($fixnum.Int64 value) => $_setInt64(1, value);
  @$pb.TagNumber(2)
  $core.bool hasChainId() => $_has(1);
  @$pb.TagNumber(2)
  void clearChainId() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get fromAddress => $_getSZ(2);
  @$pb.TagNumber(3)
  set fromAddress($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasFromAddress() => $_has(2);
  @$pb.TagNumber(3)
  void clearFromAddress() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.String get toAddress => $_getSZ(3);
  @$pb.TagNumber(4)
  set toAddress($core.String value) => $_setString(3, value);
  @$pb.TagNumber(4)
  $core.bool hasToAddress() => $_has(3);
  @$pb.TagNumber(4)
  void clearToAddress() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get amount => $_getI64(4);
  @$pb.TagNumber(5)
  set amount($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasAmount() => $_has(4);
  @$pb.TagNumber(5)
  void clearAmount() => $_clearField(5);

  @$pb.TagNumber(6)
  $core.bool get finalized => $_getBF(5);
  @$pb.TagNumber(6)
  set finalized($core.bool value) => $_setBool(5, value);
  @$pb.TagNumber(6)
  $core.bool hasFinalized() => $_has(5);
  @$pb.TagNumber(6)
  void clearFinalized() => $_clearField(6);
}

class Block extends $pb.GeneratedMessage {
  factory Block({
    $fixnum.Int64? height,
    $fixnum.Int64? txCount,
    $core.String? stateRoot,
  }) {
    final result = create();
    if (height != null) result.height = height;
    if (txCount != null) result.txCount = txCount;
    if (stateRoot != null) result.stateRoot = stateRoot;
    return result;
  }

  Block._();

  factory Block.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Block.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Block',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(2, _omitFieldNames ? '' : 'txCount', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(3, _omitFieldNames ? '' : 'stateRoot')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Block clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Block copyWith(void Function(Block) updates) =>
      super.copyWith((message) => updates(message as Block)) as Block;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Block create() => Block._();
  @$core.override
  Block createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Block getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Block>(create);
  static Block? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get height => $_getI64(0);
  @$pb.TagNumber(1)
  set height($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasHeight() => $_has(0);
  @$pb.TagNumber(1)
  void clearHeight() => $_clearField(1);

  @$pb.TagNumber(2)
  $fixnum.Int64 get txCount => $_getI64(1);
  @$pb.TagNumber(2)
  set txCount($fixnum.Int64 value) => $_setInt64(1, value);
  @$pb.TagNumber(2)
  $core.bool hasTxCount() => $_has(1);
  @$pb.TagNumber(2)
  void clearTxCount() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get stateRoot => $_getSZ(2);
  @$pb.TagNumber(3)
  set stateRoot($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasStateRoot() => $_has(2);
  @$pb.TagNumber(3)
  void clearStateRoot() => $_clearField(3);
}

class BalanceProof extends $pb.GeneratedMessage {
  factory BalanceProof({
    $fixnum.Int64? balance,
    $core.String? stateRoot,
    $core.bool? verified,
    $core.int? proofBytes,
  }) {
    final result = create();
    if (balance != null) result.balance = balance;
    if (stateRoot != null) result.stateRoot = stateRoot;
    if (verified != null) result.verified = verified;
    if (proofBytes != null) result.proofBytes = proofBytes;
    return result;
  }

  BalanceProof._();

  factory BalanceProof.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory BalanceProof.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'BalanceProof',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'balance', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'stateRoot')
    ..aOB(3, _omitFieldNames ? '' : 'verified')
    ..aI(4, _omitFieldNames ? '' : 'proofBytes', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  BalanceProof clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  BalanceProof copyWith(void Function(BalanceProof) updates) =>
      super.copyWith((message) => updates(message as BalanceProof))
          as BalanceProof;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static BalanceProof create() => BalanceProof._();
  @$core.override
  BalanceProof createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static BalanceProof getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<BalanceProof>(create);
  static BalanceProof? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get balance => $_getI64(0);
  @$pb.TagNumber(1)
  set balance($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasBalance() => $_has(0);
  @$pb.TagNumber(1)
  void clearBalance() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get stateRoot => $_getSZ(1);
  @$pb.TagNumber(2)
  set stateRoot($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasStateRoot() => $_has(1);
  @$pb.TagNumber(2)
  void clearStateRoot() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.bool get verified => $_getBF(2);
  @$pb.TagNumber(3)
  set verified($core.bool value) => $_setBool(2, value);
  @$pb.TagNumber(3)
  $core.bool hasVerified() => $_has(2);
  @$pb.TagNumber(3)
  void clearVerified() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.int get proofBytes => $_getIZ(3);
  @$pb.TagNumber(4)
  set proofBytes($core.int value) => $_setUnsignedInt32(3, value);
  @$pb.TagNumber(4)
  $core.bool hasProofBytes() => $_has(3);
  @$pb.TagNumber(4)
  void clearProofBytes() => $_clearField(4);
}

class Erc20Request extends $pb.GeneratedMessage {
  factory Erc20Request({
    $fixnum.Int64? chainId,
    $core.String? walletId,
    $core.String? tokenAddress,
    $core.String? toAddress,
    $fixnum.Int64? amount,
    $fixnum.Int64? nonce,
    $fixnum.Int64? gasPrice,
    $fixnum.Int64? gasLimit,
    $fixnum.Int64? evmChainId,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (walletId != null) result.walletId = walletId;
    if (tokenAddress != null) result.tokenAddress = tokenAddress;
    if (toAddress != null) result.toAddress = toAddress;
    if (amount != null) result.amount = amount;
    if (nonce != null) result.nonce = nonce;
    if (gasPrice != null) result.gasPrice = gasPrice;
    if (gasLimit != null) result.gasLimit = gasLimit;
    if (evmChainId != null) result.evmChainId = evmChainId;
    return result;
  }

  Erc20Request._();

  factory Erc20Request.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Erc20Request.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Erc20Request',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'walletId')
    ..aOS(3, _omitFieldNames ? '' : 'tokenAddress')
    ..aOS(4, _omitFieldNames ? '' : 'toAddress')
    ..a<$fixnum.Int64>(5, _omitFieldNames ? '' : 'amount', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(6, _omitFieldNames ? '' : 'nonce', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(
        7, _omitFieldNames ? '' : 'gasPrice', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(
        8, _omitFieldNames ? '' : 'gasLimit', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(
        9, _omitFieldNames ? '' : 'evmChainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Erc20Request clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Erc20Request copyWith(void Function(Erc20Request) updates) =>
      super.copyWith((message) => updates(message as Erc20Request))
          as Erc20Request;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Erc20Request create() => Erc20Request._();
  @$core.override
  Erc20Request createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Erc20Request getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<Erc20Request>(create);
  static Erc20Request? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get walletId => $_getSZ(1);
  @$pb.TagNumber(2)
  set walletId($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasWalletId() => $_has(1);
  @$pb.TagNumber(2)
  void clearWalletId() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get tokenAddress => $_getSZ(2);
  @$pb.TagNumber(3)
  set tokenAddress($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasTokenAddress() => $_has(2);
  @$pb.TagNumber(3)
  void clearTokenAddress() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.String get toAddress => $_getSZ(3);
  @$pb.TagNumber(4)
  set toAddress($core.String value) => $_setString(3, value);
  @$pb.TagNumber(4)
  $core.bool hasToAddress() => $_has(3);
  @$pb.TagNumber(4)
  void clearToAddress() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get amount => $_getI64(4);
  @$pb.TagNumber(5)
  set amount($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasAmount() => $_has(4);
  @$pb.TagNumber(5)
  void clearAmount() => $_clearField(5);

  @$pb.TagNumber(6)
  $fixnum.Int64 get nonce => $_getI64(5);
  @$pb.TagNumber(6)
  set nonce($fixnum.Int64 value) => $_setInt64(5, value);
  @$pb.TagNumber(6)
  $core.bool hasNonce() => $_has(5);
  @$pb.TagNumber(6)
  void clearNonce() => $_clearField(6);

  @$pb.TagNumber(7)
  $fixnum.Int64 get gasPrice => $_getI64(6);
  @$pb.TagNumber(7)
  set gasPrice($fixnum.Int64 value) => $_setInt64(6, value);
  @$pb.TagNumber(7)
  $core.bool hasGasPrice() => $_has(6);
  @$pb.TagNumber(7)
  void clearGasPrice() => $_clearField(7);

  @$pb.TagNumber(8)
  $fixnum.Int64 get gasLimit => $_getI64(7);
  @$pb.TagNumber(8)
  set gasLimit($fixnum.Int64 value) => $_setInt64(7, value);
  @$pb.TagNumber(8)
  $core.bool hasGasLimit() => $_has(7);
  @$pb.TagNumber(8)
  void clearGasLimit() => $_clearField(8);

  @$pb.TagNumber(9)
  $fixnum.Int64 get evmChainId => $_getI64(8);
  @$pb.TagNumber(9)
  set evmChainId($fixnum.Int64 value) => $_setInt64(8, value);
  @$pb.TagNumber(9)
  $core.bool hasEvmChainId() => $_has(8);
  @$pb.TagNumber(9)
  void clearEvmChainId() => $_clearField(9);
}

class SignedEvmTx extends $pb.GeneratedMessage {
  factory SignedEvmTx({
    $core.String? rawTxHex,
    $core.String? calldataHex,
    $core.String? fromAddress,
  }) {
    final result = create();
    if (rawTxHex != null) result.rawTxHex = rawTxHex;
    if (calldataHex != null) result.calldataHex = calldataHex;
    if (fromAddress != null) result.fromAddress = fromAddress;
    return result;
  }

  SignedEvmTx._();

  factory SignedEvmTx.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SignedEvmTx.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SignedEvmTx',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'rawTxHex')
    ..aOS(2, _omitFieldNames ? '' : 'calldataHex')
    ..aOS(3, _omitFieldNames ? '' : 'fromAddress')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SignedEvmTx clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SignedEvmTx copyWith(void Function(SignedEvmTx) updates) =>
      super.copyWith((message) => updates(message as SignedEvmTx))
          as SignedEvmTx;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SignedEvmTx create() => SignedEvmTx._();
  @$core.override
  SignedEvmTx createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SignedEvmTx getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SignedEvmTx>(create);
  static SignedEvmTx? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get rawTxHex => $_getSZ(0);
  @$pb.TagNumber(1)
  set rawTxHex($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasRawTxHex() => $_has(0);
  @$pb.TagNumber(1)
  void clearRawTxHex() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get calldataHex => $_getSZ(1);
  @$pb.TagNumber(2)
  set calldataHex($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasCalldataHex() => $_has(1);
  @$pb.TagNumber(2)
  void clearCalldataHex() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get fromAddress => $_getSZ(2);
  @$pb.TagNumber(3)
  set fromAddress($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasFromAddress() => $_has(2);
  @$pb.TagNumber(3)
  void clearFromAddress() => $_clearField(3);
}

class SelectorRequest extends $pb.GeneratedMessage {
  factory SelectorRequest({
    $core.String? signature,
  }) {
    final result = create();
    if (signature != null) result.signature = signature;
    return result;
  }

  SelectorRequest._();

  factory SelectorRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SelectorRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SelectorRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'signature')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelectorRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelectorRequest copyWith(void Function(SelectorRequest) updates) =>
      super.copyWith((message) => updates(message as SelectorRequest))
          as SelectorRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SelectorRequest create() => SelectorRequest._();
  @$core.override
  SelectorRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SelectorRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SelectorRequest>(create);
  static SelectorRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get signature => $_getSZ(0);
  @$pb.TagNumber(1)
  set signature($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasSignature() => $_has(0);
  @$pb.TagNumber(1)
  void clearSignature() => $_clearField(1);
}

class SelectorReply extends $pb.GeneratedMessage {
  factory SelectorReply({
    $core.String? selectorHex,
  }) {
    final result = create();
    if (selectorHex != null) result.selectorHex = selectorHex;
    return result;
  }

  SelectorReply._();

  factory SelectorReply.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory SelectorReply.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'SelectorReply',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'selectorHex')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelectorReply clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  SelectorReply copyWith(void Function(SelectorReply) updates) =>
      super.copyWith((message) => updates(message as SelectorReply))
          as SelectorReply;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static SelectorReply create() => SelectorReply._();
  @$core.override
  SelectorReply createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static SelectorReply getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<SelectorReply>(create);
  static SelectorReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get selectorHex => $_getSZ(0);
  @$pb.TagNumber(1)
  set selectorHex($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasSelectorHex() => $_has(0);
  @$pb.TagNumber(1)
  void clearSelectorHex() => $_clearField(1);
}

class Metrics extends $pb.GeneratedMessage {
  factory Metrics({
    $fixnum.Int64? height,
    $fixnum.Int64? blocks,
    $fixnum.Int64? transactions,
    $fixnum.Int64? mempool,
  }) {
    final result = create();
    if (height != null) result.height = height;
    if (blocks != null) result.blocks = blocks;
    if (transactions != null) result.transactions = transactions;
    if (mempool != null) result.mempool = mempool;
    return result;
  }

  Metrics._();

  factory Metrics.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Metrics.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Metrics',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(2, _omitFieldNames ? '' : 'blocks', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(
        3, _omitFieldNames ? '' : 'transactions', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(4, _omitFieldNames ? '' : 'mempool', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Metrics clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Metrics copyWith(void Function(Metrics) updates) =>
      super.copyWith((message) => updates(message as Metrics)) as Metrics;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Metrics create() => Metrics._();
  @$core.override
  Metrics createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Metrics getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Metrics>(create);
  static Metrics? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get height => $_getI64(0);
  @$pb.TagNumber(1)
  set height($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasHeight() => $_has(0);
  @$pb.TagNumber(1)
  void clearHeight() => $_clearField(1);

  @$pb.TagNumber(2)
  $fixnum.Int64 get blocks => $_getI64(1);
  @$pb.TagNumber(2)
  set blocks($fixnum.Int64 value) => $_setInt64(1, value);
  @$pb.TagNumber(2)
  $core.bool hasBlocks() => $_has(1);
  @$pb.TagNumber(2)
  void clearBlocks() => $_clearField(2);

  @$pb.TagNumber(3)
  $fixnum.Int64 get transactions => $_getI64(2);
  @$pb.TagNumber(3)
  set transactions($fixnum.Int64 value) => $_setInt64(2, value);
  @$pb.TagNumber(3)
  $core.bool hasTransactions() => $_has(2);
  @$pb.TagNumber(3)
  void clearTransactions() => $_clearField(3);

  @$pb.TagNumber(4)
  $fixnum.Int64 get mempool => $_getI64(3);
  @$pb.TagNumber(4)
  set mempool($fixnum.Int64 value) => $_setInt64(3, value);
  @$pb.TagNumber(4)
  $core.bool hasMempool() => $_has(3);
  @$pb.TagNumber(4)
  void clearMempool() => $_clearField(4);
}

class Event extends $pb.GeneratedMessage {
  factory Event({
    EventType? type,
    $fixnum.Int64? chainId,
    $fixnum.Int64? height,
    $fixnum.Int64? txCount,
    $core.String? txid,
    $fixnum.Int64? mempoolSize,
    $core.String? stateRoot,
    $fixnum.Int64? unixMillis,
  }) {
    final result = create();
    if (type != null) result.type = type;
    if (chainId != null) result.chainId = chainId;
    if (height != null) result.height = height;
    if (txCount != null) result.txCount = txCount;
    if (txid != null) result.txid = txid;
    if (mempoolSize != null) result.mempoolSize = mempoolSize;
    if (stateRoot != null) result.stateRoot = stateRoot;
    if (unixMillis != null) result.unixMillis = unixMillis;
    return result;
  }

  Event._();

  factory Event.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Event.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Event',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aE<EventType>(1, _omitFieldNames ? '' : 'type',
        enumValues: EventType.values)
    ..a<$fixnum.Int64>(2, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(3, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(4, _omitFieldNames ? '' : 'txCount', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(5, _omitFieldNames ? '' : 'txid')
    ..a<$fixnum.Int64>(
        6, _omitFieldNames ? '' : 'mempoolSize', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(7, _omitFieldNames ? '' : 'stateRoot')
    ..aInt64(8, _omitFieldNames ? '' : 'unixMillis')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Event clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Event copyWith(void Function(Event) updates) =>
      super.copyWith((message) => updates(message as Event)) as Event;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Event create() => Event._();
  @$core.override
  Event createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Event getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Event>(create);
  static Event? _defaultInstance;

  @$pb.TagNumber(1)
  EventType get type => $_getN(0);
  @$pb.TagNumber(1)
  set type(EventType value) => $_setField(1, value);
  @$pb.TagNumber(1)
  $core.bool hasType() => $_has(0);
  @$pb.TagNumber(1)
  void clearType() => $_clearField(1);

  @$pb.TagNumber(2)
  $fixnum.Int64 get chainId => $_getI64(1);
  @$pb.TagNumber(2)
  set chainId($fixnum.Int64 value) => $_setInt64(1, value);
  @$pb.TagNumber(2)
  $core.bool hasChainId() => $_has(1);
  @$pb.TagNumber(2)
  void clearChainId() => $_clearField(2);

  @$pb.TagNumber(3)
  $fixnum.Int64 get height => $_getI64(2);
  @$pb.TagNumber(3)
  set height($fixnum.Int64 value) => $_setInt64(2, value);
  @$pb.TagNumber(3)
  $core.bool hasHeight() => $_has(2);
  @$pb.TagNumber(3)
  void clearHeight() => $_clearField(3);

  @$pb.TagNumber(4)
  $fixnum.Int64 get txCount => $_getI64(3);
  @$pb.TagNumber(4)
  set txCount($fixnum.Int64 value) => $_setInt64(3, value);
  @$pb.TagNumber(4)
  $core.bool hasTxCount() => $_has(3);
  @$pb.TagNumber(4)
  void clearTxCount() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.String get txid => $_getSZ(4);
  @$pb.TagNumber(5)
  set txid($core.String value) => $_setString(4, value);
  @$pb.TagNumber(5)
  $core.bool hasTxid() => $_has(4);
  @$pb.TagNumber(5)
  void clearTxid() => $_clearField(5);

  @$pb.TagNumber(6)
  $fixnum.Int64 get mempoolSize => $_getI64(5);
  @$pb.TagNumber(6)
  set mempoolSize($fixnum.Int64 value) => $_setInt64(5, value);
  @$pb.TagNumber(6)
  $core.bool hasMempoolSize() => $_has(5);
  @$pb.TagNumber(6)
  void clearMempoolSize() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.String get stateRoot => $_getSZ(6);
  @$pb.TagNumber(7)
  set stateRoot($core.String value) => $_setString(6, value);
  @$pb.TagNumber(7)
  $core.bool hasStateRoot() => $_has(6);
  @$pb.TagNumber(7)
  void clearStateRoot() => $_clearField(7);

  @$pb.TagNumber(8)
  $fixnum.Int64 get unixMillis => $_getI64(7);
  @$pb.TagNumber(8)
  set unixMillis($fixnum.Int64 value) => $_setInt64(7, value);
  @$pb.TagNumber(8)
  $core.bool hasUnixMillis() => $_has(7);
  @$pb.TagNumber(8)
  void clearUnixMillis() => $_clearField(8);
}

/// -- Governance ----------------------------------------------------------------
class CouncilValidator extends $pb.GeneratedMessage {
  factory CouncilValidator({
    $core.String? publicKey,
    $fixnum.Int64? power,
    $core.bool? operator,
  }) {
    final result = create();
    if (publicKey != null) result.publicKey = publicKey;
    if (power != null) result.power = power;
    if (operator != null) result.operator = operator;
    return result;
  }

  CouncilValidator._();

  factory CouncilValidator.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CouncilValidator.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CouncilValidator',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'publicKey')
    ..a<$fixnum.Int64>(2, _omitFieldNames ? '' : 'power', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOB(3, _omitFieldNames ? '' : 'operator')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CouncilValidator clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CouncilValidator copyWith(void Function(CouncilValidator) updates) =>
      super.copyWith((message) => updates(message as CouncilValidator))
          as CouncilValidator;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CouncilValidator create() => CouncilValidator._();
  @$core.override
  CouncilValidator createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CouncilValidator getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CouncilValidator>(create);
  static CouncilValidator? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get publicKey => $_getSZ(0);
  @$pb.TagNumber(1)
  set publicKey($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasPublicKey() => $_has(0);
  @$pb.TagNumber(1)
  void clearPublicKey() => $_clearField(1);

  @$pb.TagNumber(2)
  $fixnum.Int64 get power => $_getI64(1);
  @$pb.TagNumber(2)
  set power($fixnum.Int64 value) => $_setInt64(1, value);
  @$pb.TagNumber(2)
  $core.bool hasPower() => $_has(1);
  @$pb.TagNumber(2)
  void clearPower() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.bool get operator => $_getBF(2);
  @$pb.TagNumber(3)
  set operator($core.bool value) => $_setBool(2, value);
  @$pb.TagNumber(3)
  $core.bool hasOperator() => $_has(2);
  @$pb.TagNumber(3)
  void clearOperator() => $_clearField(3);
}

class CouncilStep extends $pb.GeneratedMessage {
  factory CouncilStep({
    $core.String? label,
    $core.bool? ok,
    $core.String? detail,
  }) {
    final result = create();
    if (label != null) result.label = label;
    if (ok != null) result.ok = ok;
    if (detail != null) result.detail = detail;
    return result;
  }

  CouncilStep._();

  factory CouncilStep.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CouncilStep.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CouncilStep',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'label')
    ..aOB(2, _omitFieldNames ? '' : 'ok')
    ..aOS(3, _omitFieldNames ? '' : 'detail')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CouncilStep clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CouncilStep copyWith(void Function(CouncilStep) updates) =>
      super.copyWith((message) => updates(message as CouncilStep))
          as CouncilStep;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CouncilStep create() => CouncilStep._();
  @$core.override
  CouncilStep createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CouncilStep getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CouncilStep>(create);
  static CouncilStep? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get label => $_getSZ(0);
  @$pb.TagNumber(1)
  set label($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasLabel() => $_has(0);
  @$pb.TagNumber(1)
  void clearLabel() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.bool get ok => $_getBF(1);
  @$pb.TagNumber(2)
  set ok($core.bool value) => $_setBool(1, value);
  @$pb.TagNumber(2)
  $core.bool hasOk() => $_has(1);
  @$pb.TagNumber(2)
  void clearOk() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get detail => $_getSZ(2);
  @$pb.TagNumber(3)
  set detail($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasDetail() => $_has(2);
  @$pb.TagNumber(3)
  void clearDetail() => $_clearField(3);
}

class Council extends $pb.GeneratedMessage {
  factory Council({
    $fixnum.Int64? chainId,
    $fixnum.Int64? height,
    $core.Iterable<CouncilValidator>? validators,
    $fixnum.Int64? auditorHeight,
    $core.int? rotations,
    $core.Iterable<CouncilStep>? log,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (height != null) result.height = height;
    if (validators != null) result.validators.addAll(validators);
    if (auditorHeight != null) result.auditorHeight = auditorHeight;
    if (rotations != null) result.rotations = rotations;
    if (log != null) result.log.addAll(log);
    return result;
  }

  Council._();

  factory Council.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Council.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Council',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(2, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..pPM<CouncilValidator>(3, _omitFieldNames ? '' : 'validators',
        subBuilder: CouncilValidator.create)
    ..a<$fixnum.Int64>(
        4, _omitFieldNames ? '' : 'auditorHeight', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aI(5, _omitFieldNames ? '' : 'rotations', fieldType: $pb.PbFieldType.OU3)
    ..pPM<CouncilStep>(6, _omitFieldNames ? '' : 'log',
        subBuilder: CouncilStep.create)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Council clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Council copyWith(void Function(Council) updates) =>
      super.copyWith((message) => updates(message as Council)) as Council;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Council create() => Council._();
  @$core.override
  Council createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Council getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<Council>(create);
  static Council? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $fixnum.Int64 get height => $_getI64(1);
  @$pb.TagNumber(2)
  set height($fixnum.Int64 value) => $_setInt64(1, value);
  @$pb.TagNumber(2)
  $core.bool hasHeight() => $_has(1);
  @$pb.TagNumber(2)
  void clearHeight() => $_clearField(2);

  @$pb.TagNumber(3)
  $pb.PbList<CouncilValidator> get validators => $_getList(2);

  @$pb.TagNumber(4)
  $fixnum.Int64 get auditorHeight => $_getI64(3);
  @$pb.TagNumber(4)
  set auditorHeight($fixnum.Int64 value) => $_setInt64(3, value);
  @$pb.TagNumber(4)
  $core.bool hasAuditorHeight() => $_has(3);
  @$pb.TagNumber(4)
  void clearAuditorHeight() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.int get rotations => $_getIZ(4);
  @$pb.TagNumber(5)
  set rotations($core.int value) => $_setUnsignedInt32(4, value);
  @$pb.TagNumber(5)
  $core.bool hasRotations() => $_has(4);
  @$pb.TagNumber(5)
  void clearRotations() => $_clearField(5);

  @$pb.TagNumber(6)
  $pb.PbList<CouncilStep> get log => $_getList(5);
}

class CouncilRequest extends $pb.GeneratedMessage {
  factory CouncilRequest({
    $fixnum.Int64? chainId,
    CouncilOp? op,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (op != null) result.op = op;
    return result;
  }

  CouncilRequest._();

  factory CouncilRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory CouncilRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'CouncilRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'atlas.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aE<CouncilOp>(2, _omitFieldNames ? '' : 'op',
        enumValues: CouncilOp.values)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CouncilRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  CouncilRequest copyWith(void Function(CouncilRequest) updates) =>
      super.copyWith((message) => updates(message as CouncilRequest))
          as CouncilRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static CouncilRequest create() => CouncilRequest._();
  @$core.override
  CouncilRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static CouncilRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<CouncilRequest>(create);
  static CouncilRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  CouncilOp get op => $_getN(1);
  @$pb.TagNumber(2)
  set op(CouncilOp value) => $_setField(2, value);
  @$pb.TagNumber(2)
  $core.bool hasOp() => $_has(1);
  @$pb.TagNumber(2)
  void clearOp() => $_clearField(2);
}

const $core.bool _omitFieldNames =
    $core.bool.fromEnvironment('protobuf.omit_field_names');
const $core.bool _omitMessageNames =
    $core.bool.fromEnvironment('protobuf.omit_message_names');
