// This is a generated file - do not edit.
//
// Generated from sentinel.proto.

// @dart = 3.3

// ignore_for_file: annotate_overrides, camel_case_types, comment_references
// ignore_for_file: constant_identifier_names
// ignore_for_file: curly_braces_in_flow_control_structures
// ignore_for_file: deprecated_member_use_from_same_package, library_prefixes
// ignore_for_file: non_constant_identifier_names, prefer_relative_imports

import 'dart:async' as $async;
import 'dart:core' as $core;

import 'package:grpc/service_api.dart' as $grpc;
import 'package:protobuf/protobuf.dart' as $pb;

import 'sentinel.pb.dart' as $0;

export 'sentinel.pb.dart';

@$pb.GrpcServiceName('sentinel.v1.Sentinel')
class SentinelClient extends $grpc.Client {
  /// The hostname for this service.
  static const $core.String defaultHost = '';

  /// OAuth scopes needed for the client.
  static const $core.List<$core.String> oauthScopes = [
    '',
  ];

  SentinelClient(super.channel, {super.options, super.interceptors});

  /// Enroll a device. The device info + credential generate the genesis/origin
  /// block; the returned chain is bound to this device for its lifetime.
  $grpc.ResponseFuture<$0.Provenance> provisionDevice(
    $0.DeviceInfo request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$provisionDevice, request, options: options);
  }

  /// The live capture pipeline: the device streams data records, the server
  /// anchors each one and streams back anchoring receipts (pending → finalized).
  /// Bidirectional so finality receipts arrive asynchronously as blocks seal.
  $grpc.ResponseStream<$0.AnchorReceipt> streamData(
    $async.Stream<$0.DataRecord> request, {
    $grpc.CallOptions? options,
  }) {
    return $createStreamingCall(_$streamData, request, options: options);
  }

  /// Prove a previously-anchored item is committed in the chain (integrity check).
  $grpc.ResponseFuture<$0.VerifyReply> verifyRecord(
    $0.VerifyRequest request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$verifyRecord, request, options: options);
  }

  /// Ledger status + Prometheus-style metrics for the device chain.
  $grpc.ResponseFuture<$0.LedgerStatus> getLedger(
    $0.DeviceRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$getLedger, request, options: options);
  }

  /// ── Fleet: a CHAIN TREE — one fleet root anchoring a subchain per device;
  /// a device's on-chain identity is provable from the fleet root header.
  $grpc.ResponseFuture<$0.Fleet> buildFleet(
    $0.FleetSpec request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$buildFleet, request, options: options);
  }

  $grpc.ResponseFuture<$0.Fleet> getFleet(
    $0.FleetRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$getFleet, request, options: options);
  }

  $grpc.ResponseFuture<$0.Fleet> fleetProduce(
    $0.FleetRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$fleetProduce, request, options: options);
  }

  $grpc.ResponseFuture<$0.FleetProof> fleetProveDevice(
    $0.FleetRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$fleetProveDevice, request, options: options);
  }

  // method descriptors

  static final _$provisionDevice =
      $grpc.ClientMethod<$0.DeviceInfo, $0.Provenance>(
          '/sentinel.v1.Sentinel/ProvisionDevice',
          ($0.DeviceInfo value) => value.writeToBuffer(),
          $0.Provenance.fromBuffer);
  static final _$streamData =
      $grpc.ClientMethod<$0.DataRecord, $0.AnchorReceipt>(
          '/sentinel.v1.Sentinel/StreamData',
          ($0.DataRecord value) => value.writeToBuffer(),
          $0.AnchorReceipt.fromBuffer);
  static final _$verifyRecord =
      $grpc.ClientMethod<$0.VerifyRequest, $0.VerifyReply>(
          '/sentinel.v1.Sentinel/VerifyRecord',
          ($0.VerifyRequest value) => value.writeToBuffer(),
          $0.VerifyReply.fromBuffer);
  static final _$getLedger = $grpc.ClientMethod<$0.DeviceRef, $0.LedgerStatus>(
      '/sentinel.v1.Sentinel/GetLedger',
      ($0.DeviceRef value) => value.writeToBuffer(),
      $0.LedgerStatus.fromBuffer);
  static final _$buildFleet = $grpc.ClientMethod<$0.FleetSpec, $0.Fleet>(
      '/sentinel.v1.Sentinel/BuildFleet',
      ($0.FleetSpec value) => value.writeToBuffer(),
      $0.Fleet.fromBuffer);
  static final _$getFleet = $grpc.ClientMethod<$0.FleetRef, $0.Fleet>(
      '/sentinel.v1.Sentinel/GetFleet',
      ($0.FleetRef value) => value.writeToBuffer(),
      $0.Fleet.fromBuffer);
  static final _$fleetProduce = $grpc.ClientMethod<$0.FleetRef, $0.Fleet>(
      '/sentinel.v1.Sentinel/FleetProduce',
      ($0.FleetRef value) => value.writeToBuffer(),
      $0.Fleet.fromBuffer);
  static final _$fleetProveDevice =
      $grpc.ClientMethod<$0.FleetRef, $0.FleetProof>(
          '/sentinel.v1.Sentinel/FleetProveDevice',
          ($0.FleetRef value) => value.writeToBuffer(),
          $0.FleetProof.fromBuffer);
}

@$pb.GrpcServiceName('sentinel.v1.Sentinel')
abstract class SentinelServiceBase extends $grpc.Service {
  $core.String get $name => 'sentinel.v1.Sentinel';

  SentinelServiceBase() {
    $addMethod($grpc.ServiceMethod<$0.DeviceInfo, $0.Provenance>(
        'ProvisionDevice',
        provisionDevice_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.DeviceInfo.fromBuffer(value),
        ($0.Provenance value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.DataRecord, $0.AnchorReceipt>(
        'StreamData',
        streamData,
        true,
        true,
        ($core.List<$core.int> value) => $0.DataRecord.fromBuffer(value),
        ($0.AnchorReceipt value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.VerifyRequest, $0.VerifyReply>(
        'VerifyRecord',
        verifyRecord_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.VerifyRequest.fromBuffer(value),
        ($0.VerifyReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.DeviceRef, $0.LedgerStatus>(
        'GetLedger',
        getLedger_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.DeviceRef.fromBuffer(value),
        ($0.LedgerStatus value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.FleetSpec, $0.Fleet>(
        'BuildFleet',
        buildFleet_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.FleetSpec.fromBuffer(value),
        ($0.Fleet value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.FleetRef, $0.Fleet>(
        'GetFleet',
        getFleet_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.FleetRef.fromBuffer(value),
        ($0.Fleet value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.FleetRef, $0.Fleet>(
        'FleetProduce',
        fleetProduce_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.FleetRef.fromBuffer(value),
        ($0.Fleet value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.FleetRef, $0.FleetProof>(
        'FleetProveDevice',
        fleetProveDevice_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.FleetRef.fromBuffer(value),
        ($0.FleetProof value) => value.writeToBuffer()));
  }

  $async.Future<$0.Provenance> provisionDevice_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.DeviceInfo> $request) async {
    return provisionDevice($call, await $request);
  }

  $async.Future<$0.Provenance> provisionDevice(
      $grpc.ServiceCall call, $0.DeviceInfo request);

  $async.Stream<$0.AnchorReceipt> streamData(
      $grpc.ServiceCall call, $async.Stream<$0.DataRecord> request);

  $async.Future<$0.VerifyReply> verifyRecord_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.VerifyRequest> $request) async {
    return verifyRecord($call, await $request);
  }

  $async.Future<$0.VerifyReply> verifyRecord(
      $grpc.ServiceCall call, $0.VerifyRequest request);

  $async.Future<$0.LedgerStatus> getLedger_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.DeviceRef> $request) async {
    return getLedger($call, await $request);
  }

  $async.Future<$0.LedgerStatus> getLedger(
      $grpc.ServiceCall call, $0.DeviceRef request);

  $async.Future<$0.Fleet> buildFleet_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.FleetSpec> $request) async {
    return buildFleet($call, await $request);
  }

  $async.Future<$0.Fleet> buildFleet(
      $grpc.ServiceCall call, $0.FleetSpec request);

  $async.Future<$0.Fleet> getFleet_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.FleetRef> $request) async {
    return getFleet($call, await $request);
  }

  $async.Future<$0.Fleet> getFleet($grpc.ServiceCall call, $0.FleetRef request);

  $async.Future<$0.Fleet> fleetProduce_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.FleetRef> $request) async {
    return fleetProduce($call, await $request);
  }

  $async.Future<$0.Fleet> fleetProduce(
      $grpc.ServiceCall call, $0.FleetRef request);

  $async.Future<$0.FleetProof> fleetProveDevice_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.FleetRef> $request) async {
    return fleetProveDevice($call, await $request);
  }

  $async.Future<$0.FleetProof> fleetProveDevice(
      $grpc.ServiceCall call, $0.FleetRef request);
}
