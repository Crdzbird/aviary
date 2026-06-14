// This is a generated file - do not edit.
//
// Generated from sentinel.proto.

// @dart = 3.3

// ignore_for_file: annotate_overrides, camel_case_types, comment_references
// ignore_for_file: constant_identifier_names
// ignore_for_file: curly_braces_in_flow_control_structures
// ignore_for_file: deprecated_member_use_from_same_package, library_prefixes
// ignore_for_file: non_constant_identifier_names, prefer_relative_imports

import 'dart:core' as $core;

import 'package:fixnum/fixnum.dart' as $fixnum;
import 'package:protobuf/protobuf.dart' as $pb;

import 'sentinel.pbenum.dart';

export 'package:protobuf/protobuf.dart' show GeneratedMessageGenericExtensions;

export 'sentinel.pbenum.dart';

class DeviceInfo extends $pb.GeneratedMessage {
  factory DeviceInfo({
    $core.String? deviceId,
    $core.String? model,
    $core.String? platform,
    $core.String? osVersion,
    $core.String? label,
    $core.List<$core.int>? credential,
  }) {
    final result = create();
    if (deviceId != null) result.deviceId = deviceId;
    if (model != null) result.model = model;
    if (platform != null) result.platform = platform;
    if (osVersion != null) result.osVersion = osVersion;
    if (label != null) result.label = label;
    if (credential != null) result.credential = credential;
    return result;
  }

  DeviceInfo._();

  factory DeviceInfo.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory DeviceInfo.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'DeviceInfo',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'sentinel.v1'),
      createEmptyInstance: create)
    ..aOS(1, _omitFieldNames ? '' : 'deviceId')
    ..aOS(2, _omitFieldNames ? '' : 'model')
    ..aOS(3, _omitFieldNames ? '' : 'platform')
    ..aOS(4, _omitFieldNames ? '' : 'osVersion')
    ..aOS(5, _omitFieldNames ? '' : 'label')
    ..a<$core.List<$core.int>>(
        6, _omitFieldNames ? '' : 'credential', $pb.PbFieldType.OY)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  DeviceInfo clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  DeviceInfo copyWith(void Function(DeviceInfo) updates) =>
      super.copyWith((message) => updates(message as DeviceInfo)) as DeviceInfo;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static DeviceInfo create() => DeviceInfo._();
  @$core.override
  DeviceInfo createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static DeviceInfo getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<DeviceInfo>(create);
  static DeviceInfo? _defaultInstance;

  @$pb.TagNumber(1)
  $core.String get deviceId => $_getSZ(0);
  @$pb.TagNumber(1)
  set deviceId($core.String value) => $_setString(0, value);
  @$pb.TagNumber(1)
  $core.bool hasDeviceId() => $_has(0);
  @$pb.TagNumber(1)
  void clearDeviceId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get model => $_getSZ(1);
  @$pb.TagNumber(2)
  set model($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasModel() => $_has(1);
  @$pb.TagNumber(2)
  void clearModel() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get platform => $_getSZ(2);
  @$pb.TagNumber(3)
  set platform($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasPlatform() => $_has(2);
  @$pb.TagNumber(3)
  void clearPlatform() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.String get osVersion => $_getSZ(3);
  @$pb.TagNumber(4)
  set osVersion($core.String value) => $_setString(3, value);
  @$pb.TagNumber(4)
  $core.bool hasOsVersion() => $_has(3);
  @$pb.TagNumber(4)
  void clearOsVersion() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.String get label => $_getSZ(4);
  @$pb.TagNumber(5)
  set label($core.String value) => $_setString(4, value);
  @$pb.TagNumber(5)
  $core.bool hasLabel() => $_has(4);
  @$pb.TagNumber(5)
  void clearLabel() => $_clearField(5);

  @$pb.TagNumber(6)
  $core.List<$core.int> get credential => $_getN(5);
  @$pb.TagNumber(6)
  set credential($core.List<$core.int> value) => $_setBytes(5, value);
  @$pb.TagNumber(6)
  $core.bool hasCredential() => $_has(5);
  @$pb.TagNumber(6)
  void clearCredential() => $_clearField(6);
}

class Provenance extends $pb.GeneratedMessage {
  factory Provenance({
    $fixnum.Int64? chainId,
    $core.String? deviceFingerprint,
    $core.String? genesisHash,
    $core.String? deviceAddress,
    $fixnum.Int64? genesisHeight,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (deviceFingerprint != null) result.deviceFingerprint = deviceFingerprint;
    if (genesisHash != null) result.genesisHash = genesisHash;
    if (deviceAddress != null) result.deviceAddress = deviceAddress;
    if (genesisHeight != null) result.genesisHeight = genesisHeight;
    return result;
  }

  Provenance._();

  factory Provenance.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory Provenance.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'Provenance',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'sentinel.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'deviceFingerprint')
    ..aOS(3, _omitFieldNames ? '' : 'genesisHash')
    ..aOS(4, _omitFieldNames ? '' : 'deviceAddress')
    ..a<$fixnum.Int64>(
        5, _omitFieldNames ? '' : 'genesisHeight', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Provenance clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  Provenance copyWith(void Function(Provenance) updates) =>
      super.copyWith((message) => updates(message as Provenance)) as Provenance;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static Provenance create() => Provenance._();
  @$core.override
  Provenance createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static Provenance getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<Provenance>(create);
  static Provenance? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get deviceFingerprint => $_getSZ(1);
  @$pb.TagNumber(2)
  set deviceFingerprint($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasDeviceFingerprint() => $_has(1);
  @$pb.TagNumber(2)
  void clearDeviceFingerprint() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get genesisHash => $_getSZ(2);
  @$pb.TagNumber(3)
  set genesisHash($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasGenesisHash() => $_has(2);
  @$pb.TagNumber(3)
  void clearGenesisHash() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.String get deviceAddress => $_getSZ(3);
  @$pb.TagNumber(4)
  set deviceAddress($core.String value) => $_setString(3, value);
  @$pb.TagNumber(4)
  $core.bool hasDeviceAddress() => $_has(3);
  @$pb.TagNumber(4)
  void clearDeviceAddress() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get genesisHeight => $_getI64(4);
  @$pb.TagNumber(5)
  set genesisHeight($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasGenesisHeight() => $_has(4);
  @$pb.TagNumber(5)
  void clearGenesisHeight() => $_clearField(5);
}

class DataRecord extends $pb.GeneratedMessage {
  factory DataRecord({
    $fixnum.Int64? chainId,
    DataKind? kind,
    $core.List<$core.int>? payload,
    $core.String? mime,
    $core.double? lat,
    $core.double? lon,
    $core.double? accuracyM,
    $core.Iterable<$core.MapEntry<$core.String, $core.double>>? sensors,
    $fixnum.Int64? capturedUnixMillis,
    $core.String? note,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (kind != null) result.kind = kind;
    if (payload != null) result.payload = payload;
    if (mime != null) result.mime = mime;
    if (lat != null) result.lat = lat;
    if (lon != null) result.lon = lon;
    if (accuracyM != null) result.accuracyM = accuracyM;
    if (sensors != null) result.sensors.addEntries(sensors);
    if (capturedUnixMillis != null)
      result.capturedUnixMillis = capturedUnixMillis;
    if (note != null) result.note = note;
    return result;
  }

  DataRecord._();

  factory DataRecord.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory DataRecord.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'DataRecord',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'sentinel.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aE<DataKind>(2, _omitFieldNames ? '' : 'kind',
        enumValues: DataKind.values)
    ..a<$core.List<$core.int>>(
        3, _omitFieldNames ? '' : 'payload', $pb.PbFieldType.OY)
    ..aOS(4, _omitFieldNames ? '' : 'mime')
    ..aD(5, _omitFieldNames ? '' : 'lat')
    ..aD(6, _omitFieldNames ? '' : 'lon')
    ..aD(7, _omitFieldNames ? '' : 'accuracyM')
    ..m<$core.String, $core.double>(8, _omitFieldNames ? '' : 'sensors',
        entryClassName: 'DataRecord.SensorsEntry',
        keyFieldType: $pb.PbFieldType.OS,
        valueFieldType: $pb.PbFieldType.OD,
        packageName: const $pb.PackageName('sentinel.v1'))
    ..aInt64(9, _omitFieldNames ? '' : 'capturedUnixMillis')
    ..aOS(10, _omitFieldNames ? '' : 'note')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  DataRecord clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  DataRecord copyWith(void Function(DataRecord) updates) =>
      super.copyWith((message) => updates(message as DataRecord)) as DataRecord;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static DataRecord create() => DataRecord._();
  @$core.override
  DataRecord createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static DataRecord getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<DataRecord>(create);
  static DataRecord? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  DataKind get kind => $_getN(1);
  @$pb.TagNumber(2)
  set kind(DataKind value) => $_setField(2, value);
  @$pb.TagNumber(2)
  $core.bool hasKind() => $_has(1);
  @$pb.TagNumber(2)
  void clearKind() => $_clearField(2);

  /// The captured bytes (a photo, a video chunk). The server hashes these with
  /// BLAKE3. For LOCATION/SENSOR records this may be empty and the canonical
  /// metadata below is hashed instead.
  @$pb.TagNumber(3)
  $core.List<$core.int> get payload => $_getN(2);
  @$pb.TagNumber(3)
  set payload($core.List<$core.int> value) => $_setBytes(2, value);
  @$pb.TagNumber(3)
  $core.bool hasPayload() => $_has(2);
  @$pb.TagNumber(3)
  void clearPayload() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.String get mime => $_getSZ(3);
  @$pb.TagNumber(4)
  set mime($core.String value) => $_setString(3, value);
  @$pb.TagNumber(4)
  $core.bool hasMime() => $_has(3);
  @$pb.TagNumber(4)
  void clearMime() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.double get lat => $_getN(4);
  @$pb.TagNumber(5)
  set lat($core.double value) => $_setDouble(4, value);
  @$pb.TagNumber(5)
  $core.bool hasLat() => $_has(4);
  @$pb.TagNumber(5)
  void clearLat() => $_clearField(5);

  @$pb.TagNumber(6)
  $core.double get lon => $_getN(5);
  @$pb.TagNumber(6)
  set lon($core.double value) => $_setDouble(5, value);
  @$pb.TagNumber(6)
  $core.bool hasLon() => $_has(5);
  @$pb.TagNumber(6)
  void clearLon() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.double get accuracyM => $_getN(6);
  @$pb.TagNumber(7)
  set accuracyM($core.double value) => $_setDouble(6, value);
  @$pb.TagNumber(7)
  $core.bool hasAccuracyM() => $_has(6);
  @$pb.TagNumber(7)
  void clearAccuracyM() => $_clearField(7);

  @$pb.TagNumber(8)
  $pb.PbMap<$core.String, $core.double> get sensors => $_getMap(7);

  @$pb.TagNumber(9)
  $fixnum.Int64 get capturedUnixMillis => $_getI64(8);
  @$pb.TagNumber(9)
  set capturedUnixMillis($fixnum.Int64 value) => $_setInt64(8, value);
  @$pb.TagNumber(9)
  $core.bool hasCapturedUnixMillis() => $_has(8);
  @$pb.TagNumber(9)
  void clearCapturedUnixMillis() => $_clearField(9);

  @$pb.TagNumber(10)
  $core.String get note => $_getSZ(9);
  @$pb.TagNumber(10)
  set note($core.String value) => $_setString(9, value);
  @$pb.TagNumber(10)
  $core.bool hasNote() => $_has(9);
  @$pb.TagNumber(10)
  void clearNote() => $_clearField(10);
}

class AnchorReceipt extends $pb.GeneratedMessage {
  factory AnchorReceipt({
    $fixnum.Int64? sequence,
    DataKind? kind,
    $core.String? contentHash,
    $core.String? txid,
    $core.bool? finalized,
    $fixnum.Int64? height,
    $core.String? summary,
    $fixnum.Int64? anchoredUnixMillis,
  }) {
    final result = create();
    if (sequence != null) result.sequence = sequence;
    if (kind != null) result.kind = kind;
    if (contentHash != null) result.contentHash = contentHash;
    if (txid != null) result.txid = txid;
    if (finalized != null) result.finalized = finalized;
    if (height != null) result.height = height;
    if (summary != null) result.summary = summary;
    if (anchoredUnixMillis != null)
      result.anchoredUnixMillis = anchoredUnixMillis;
    return result;
  }

  AnchorReceipt._();

  factory AnchorReceipt.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory AnchorReceipt.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'AnchorReceipt',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'sentinel.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(
        1, _omitFieldNames ? '' : 'sequence', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aE<DataKind>(2, _omitFieldNames ? '' : 'kind',
        enumValues: DataKind.values)
    ..aOS(3, _omitFieldNames ? '' : 'contentHash')
    ..aOS(4, _omitFieldNames ? '' : 'txid')
    ..aOB(5, _omitFieldNames ? '' : 'finalized')
    ..a<$fixnum.Int64>(6, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(7, _omitFieldNames ? '' : 'summary')
    ..aInt64(8, _omitFieldNames ? '' : 'anchoredUnixMillis')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  AnchorReceipt clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  AnchorReceipt copyWith(void Function(AnchorReceipt) updates) =>
      super.copyWith((message) => updates(message as AnchorReceipt))
          as AnchorReceipt;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static AnchorReceipt create() => AnchorReceipt._();
  @$core.override
  AnchorReceipt createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static AnchorReceipt getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<AnchorReceipt>(create);
  static AnchorReceipt? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get sequence => $_getI64(0);
  @$pb.TagNumber(1)
  set sequence($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasSequence() => $_has(0);
  @$pb.TagNumber(1)
  void clearSequence() => $_clearField(1);

  @$pb.TagNumber(2)
  DataKind get kind => $_getN(1);
  @$pb.TagNumber(2)
  set kind(DataKind value) => $_setField(2, value);
  @$pb.TagNumber(2)
  $core.bool hasKind() => $_has(1);
  @$pb.TagNumber(2)
  void clearKind() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get contentHash => $_getSZ(2);
  @$pb.TagNumber(3)
  set contentHash($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasContentHash() => $_has(2);
  @$pb.TagNumber(3)
  void clearContentHash() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.String get txid => $_getSZ(3);
  @$pb.TagNumber(4)
  set txid($core.String value) => $_setString(3, value);
  @$pb.TagNumber(4)
  $core.bool hasTxid() => $_has(3);
  @$pb.TagNumber(4)
  void clearTxid() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.bool get finalized => $_getBF(4);
  @$pb.TagNumber(5)
  set finalized($core.bool value) => $_setBool(4, value);
  @$pb.TagNumber(5)
  $core.bool hasFinalized() => $_has(4);
  @$pb.TagNumber(5)
  void clearFinalized() => $_clearField(5);

  @$pb.TagNumber(6)
  $fixnum.Int64 get height => $_getI64(5);
  @$pb.TagNumber(6)
  set height($fixnum.Int64 value) => $_setInt64(5, value);
  @$pb.TagNumber(6)
  $core.bool hasHeight() => $_has(5);
  @$pb.TagNumber(6)
  void clearHeight() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.String get summary => $_getSZ(6);
  @$pb.TagNumber(7)
  set summary($core.String value) => $_setString(6, value);
  @$pb.TagNumber(7)
  $core.bool hasSummary() => $_has(6);
  @$pb.TagNumber(7)
  void clearSummary() => $_clearField(7);

  @$pb.TagNumber(8)
  $fixnum.Int64 get anchoredUnixMillis => $_getI64(7);
  @$pb.TagNumber(8)
  set anchoredUnixMillis($fixnum.Int64 value) => $_setInt64(7, value);
  @$pb.TagNumber(8)
  $core.bool hasAnchoredUnixMillis() => $_has(7);
  @$pb.TagNumber(8)
  void clearAnchoredUnixMillis() => $_clearField(8);
}

class VerifyRequest extends $pb.GeneratedMessage {
  factory VerifyRequest({
    $fixnum.Int64? chainId,
    $core.String? contentHash,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (contentHash != null) result.contentHash = contentHash;
    return result;
  }

  VerifyRequest._();

  factory VerifyRequest.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory VerifyRequest.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'VerifyRequest',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'sentinel.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'contentHash')
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  VerifyRequest clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  VerifyRequest copyWith(void Function(VerifyRequest) updates) =>
      super.copyWith((message) => updates(message as VerifyRequest))
          as VerifyRequest;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static VerifyRequest create() => VerifyRequest._();
  @$core.override
  VerifyRequest createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static VerifyRequest getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<VerifyRequest>(create);
  static VerifyRequest? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get contentHash => $_getSZ(1);
  @$pb.TagNumber(2)
  set contentHash($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasContentHash() => $_has(1);
  @$pb.TagNumber(2)
  void clearContentHash() => $_clearField(2);
}

class VerifyReply extends $pb.GeneratedMessage {
  factory VerifyReply({
    $core.bool? anchored,
    $core.bool? verified,
    $fixnum.Int64? height,
    $core.String? stateRoot,
    $core.int? proofBytes,
  }) {
    final result = create();
    if (anchored != null) result.anchored = anchored;
    if (verified != null) result.verified = verified;
    if (height != null) result.height = height;
    if (stateRoot != null) result.stateRoot = stateRoot;
    if (proofBytes != null) result.proofBytes = proofBytes;
    return result;
  }

  VerifyReply._();

  factory VerifyReply.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory VerifyReply.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'VerifyReply',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'sentinel.v1'),
      createEmptyInstance: create)
    ..aOB(1, _omitFieldNames ? '' : 'anchored')
    ..aOB(2, _omitFieldNames ? '' : 'verified')
    ..a<$fixnum.Int64>(3, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(4, _omitFieldNames ? '' : 'stateRoot')
    ..aI(5, _omitFieldNames ? '' : 'proofBytes', fieldType: $pb.PbFieldType.OU3)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  VerifyReply clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  VerifyReply copyWith(void Function(VerifyReply) updates) =>
      super.copyWith((message) => updates(message as VerifyReply))
          as VerifyReply;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static VerifyReply create() => VerifyReply._();
  @$core.override
  VerifyReply createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static VerifyReply getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<VerifyReply>(create);
  static VerifyReply? _defaultInstance;

  @$pb.TagNumber(1)
  $core.bool get anchored => $_getBF(0);
  @$pb.TagNumber(1)
  set anchored($core.bool value) => $_setBool(0, value);
  @$pb.TagNumber(1)
  $core.bool hasAnchored() => $_has(0);
  @$pb.TagNumber(1)
  void clearAnchored() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.bool get verified => $_getBF(1);
  @$pb.TagNumber(2)
  set verified($core.bool value) => $_setBool(1, value);
  @$pb.TagNumber(2)
  $core.bool hasVerified() => $_has(1);
  @$pb.TagNumber(2)
  void clearVerified() => $_clearField(2);

  @$pb.TagNumber(3)
  $fixnum.Int64 get height => $_getI64(2);
  @$pb.TagNumber(3)
  set height($fixnum.Int64 value) => $_setInt64(2, value);
  @$pb.TagNumber(3)
  $core.bool hasHeight() => $_has(2);
  @$pb.TagNumber(3)
  void clearHeight() => $_clearField(3);

  @$pb.TagNumber(4)
  $core.String get stateRoot => $_getSZ(3);
  @$pb.TagNumber(4)
  set stateRoot($core.String value) => $_setString(3, value);
  @$pb.TagNumber(4)
  $core.bool hasStateRoot() => $_has(3);
  @$pb.TagNumber(4)
  void clearStateRoot() => $_clearField(4);

  @$pb.TagNumber(5)
  $core.int get proofBytes => $_getIZ(4);
  @$pb.TagNumber(5)
  set proofBytes($core.int value) => $_setUnsignedInt32(4, value);
  @$pb.TagNumber(5)
  $core.bool hasProofBytes() => $_has(4);
  @$pb.TagNumber(5)
  void clearProofBytes() => $_clearField(5);
}

class DeviceRef extends $pb.GeneratedMessage {
  factory DeviceRef({
    $fixnum.Int64? chainId,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    return result;
  }

  DeviceRef._();

  factory DeviceRef.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory DeviceRef.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'DeviceRef',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'sentinel.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  DeviceRef clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  DeviceRef copyWith(void Function(DeviceRef) updates) =>
      super.copyWith((message) => updates(message as DeviceRef)) as DeviceRef;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static DeviceRef create() => DeviceRef._();
  @$core.override
  DeviceRef createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static DeviceRef getDefault() =>
      _defaultInstance ??= $pb.GeneratedMessage.$_defaultFor<DeviceRef>(create);
  static DeviceRef? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);
}

class LedgerStatus extends $pb.GeneratedMessage {
  factory LedgerStatus({
    $fixnum.Int64? chainId,
    $core.String? deviceAddress,
    $core.String? deviceFingerprint,
    $fixnum.Int64? height,
    $fixnum.Int64? records,
    $fixnum.Int64? mempool,
    $core.String? stateRoot,
    $core.Iterable<$core.MapEntry<$core.String, $fixnum.Int64>>? kindCounts,
  }) {
    final result = create();
    if (chainId != null) result.chainId = chainId;
    if (deviceAddress != null) result.deviceAddress = deviceAddress;
    if (deviceFingerprint != null) result.deviceFingerprint = deviceFingerprint;
    if (height != null) result.height = height;
    if (records != null) result.records = records;
    if (mempool != null) result.mempool = mempool;
    if (stateRoot != null) result.stateRoot = stateRoot;
    if (kindCounts != null) result.kindCounts.addEntries(kindCounts);
    return result;
  }

  LedgerStatus._();

  factory LedgerStatus.fromBuffer($core.List<$core.int> data,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromBuffer(data, registry);
  factory LedgerStatus.fromJson($core.String json,
          [$pb.ExtensionRegistry registry = $pb.ExtensionRegistry.EMPTY]) =>
      create()..mergeFromJson(json, registry);

  static final $pb.BuilderInfo _i = $pb.BuilderInfo(
      _omitMessageNames ? '' : 'LedgerStatus',
      package: const $pb.PackageName(_omitMessageNames ? '' : 'sentinel.v1'),
      createEmptyInstance: create)
    ..a<$fixnum.Int64>(1, _omitFieldNames ? '' : 'chainId', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(2, _omitFieldNames ? '' : 'deviceAddress')
    ..aOS(3, _omitFieldNames ? '' : 'deviceFingerprint')
    ..a<$fixnum.Int64>(4, _omitFieldNames ? '' : 'height', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(5, _omitFieldNames ? '' : 'records', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..a<$fixnum.Int64>(6, _omitFieldNames ? '' : 'mempool', $pb.PbFieldType.OU6,
        defaultOrMaker: $fixnum.Int64.ZERO)
    ..aOS(7, _omitFieldNames ? '' : 'stateRoot')
    ..m<$core.String, $fixnum.Int64>(8, _omitFieldNames ? '' : 'kindCounts',
        entryClassName: 'LedgerStatus.KindCountsEntry',
        keyFieldType: $pb.PbFieldType.OS,
        valueFieldType: $pb.PbFieldType.OU6,
        packageName: const $pb.PackageName('sentinel.v1'))
    ..hasRequiredFields = false;

  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  LedgerStatus clone() => deepCopy();
  @$core.Deprecated('See https://github.com/google/protobuf.dart/issues/998.')
  LedgerStatus copyWith(void Function(LedgerStatus) updates) =>
      super.copyWith((message) => updates(message as LedgerStatus))
          as LedgerStatus;

  @$core.override
  $pb.BuilderInfo get info_ => _i;

  @$core.pragma('dart2js:noInline')
  static LedgerStatus create() => LedgerStatus._();
  @$core.override
  LedgerStatus createEmptyInstance() => create();
  @$core.pragma('dart2js:noInline')
  static LedgerStatus getDefault() => _defaultInstance ??=
      $pb.GeneratedMessage.$_defaultFor<LedgerStatus>(create);
  static LedgerStatus? _defaultInstance;

  @$pb.TagNumber(1)
  $fixnum.Int64 get chainId => $_getI64(0);
  @$pb.TagNumber(1)
  set chainId($fixnum.Int64 value) => $_setInt64(0, value);
  @$pb.TagNumber(1)
  $core.bool hasChainId() => $_has(0);
  @$pb.TagNumber(1)
  void clearChainId() => $_clearField(1);

  @$pb.TagNumber(2)
  $core.String get deviceAddress => $_getSZ(1);
  @$pb.TagNumber(2)
  set deviceAddress($core.String value) => $_setString(1, value);
  @$pb.TagNumber(2)
  $core.bool hasDeviceAddress() => $_has(1);
  @$pb.TagNumber(2)
  void clearDeviceAddress() => $_clearField(2);

  @$pb.TagNumber(3)
  $core.String get deviceFingerprint => $_getSZ(2);
  @$pb.TagNumber(3)
  set deviceFingerprint($core.String value) => $_setString(2, value);
  @$pb.TagNumber(3)
  $core.bool hasDeviceFingerprint() => $_has(2);
  @$pb.TagNumber(3)
  void clearDeviceFingerprint() => $_clearField(3);

  @$pb.TagNumber(4)
  $fixnum.Int64 get height => $_getI64(3);
  @$pb.TagNumber(4)
  set height($fixnum.Int64 value) => $_setInt64(3, value);
  @$pb.TagNumber(4)
  $core.bool hasHeight() => $_has(3);
  @$pb.TagNumber(4)
  void clearHeight() => $_clearField(4);

  @$pb.TagNumber(5)
  $fixnum.Int64 get records => $_getI64(4);
  @$pb.TagNumber(5)
  set records($fixnum.Int64 value) => $_setInt64(4, value);
  @$pb.TagNumber(5)
  $core.bool hasRecords() => $_has(4);
  @$pb.TagNumber(5)
  void clearRecords() => $_clearField(5);

  @$pb.TagNumber(6)
  $fixnum.Int64 get mempool => $_getI64(5);
  @$pb.TagNumber(6)
  set mempool($fixnum.Int64 value) => $_setInt64(5, value);
  @$pb.TagNumber(6)
  $core.bool hasMempool() => $_has(5);
  @$pb.TagNumber(6)
  void clearMempool() => $_clearField(6);

  @$pb.TagNumber(7)
  $core.String get stateRoot => $_getSZ(6);
  @$pb.TagNumber(7)
  set stateRoot($core.String value) => $_setString(6, value);
  @$pb.TagNumber(7)
  $core.bool hasStateRoot() => $_has(6);
  @$pb.TagNumber(7)
  void clearStateRoot() => $_clearField(7);

  @$pb.TagNumber(8)
  $pb.PbMap<$core.String, $fixnum.Int64> get kindCounts => $_getMap(7);
}

const $core.bool _omitFieldNames =
    $core.bool.fromEnvironment('protobuf.omit_field_names');
const $core.bool _omitMessageNames =
    $core.bool.fromEnvironment('protobuf.omit_message_names');
