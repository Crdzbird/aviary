// This is a generated file - do not edit.
//
// Generated from interop.proto.

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

import 'interop.pb.dart' as $0;

export 'interop.pb.dart';

@$pb.GrpcServiceName('interop.v1.Interop')
class InteropClient extends $grpc.Client {
  /// The hostname for this service.
  static const $core.String defaultHost = '';

  /// OAuth scopes needed for the client.
  static const $core.List<$core.String> oauthScopes = [
    '',
  ];

  InteropClient(super.channel, {super.options, super.interceptors});

  /// go ↔ flutter: compute canonical bytes for identical inputs (determinism).
  $grpc.ResponseFuture<$0.CanonicalOutput> computeCanonical(
    $0.CanonicalInput request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$computeCanonical, request, options: options);
  }

  /// go → flutter: the server builds the scenario and returns a verifiable proof.
  $grpc.ResponseFuture<$0.ChainResult> buildChain(
    $0.ChainScenario request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$buildChain, request, options: options);
  }

  /// flutter → go: the server verifies a proof the client produced.
  $grpc.ResponseFuture<$0.VerifyResult> verifyProof(
    $0.ProofBundle request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$verifyProof, request, options: options);
  }

  /// ── Chain tree: Go builds a deterministic family (root ← child ← grandchild)
  /// from shared seeds; Flutter builds the SAME tree natively. Roots must match,
  /// and each side verifies the other's cross-chain proof.
  $grpc.ResponseFuture<$0.TreeResult> buildTree(
    $0.TreeScenario request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$buildTree, request, options: options);
  }

  $grpc.ResponseFuture<$0.VerifyResult> verifyTreeProof(
    $0.TreeProofBundle request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$verifyTreeProof, request, options: options);
  }

  // method descriptors

  static final _$computeCanonical =
      $grpc.ClientMethod<$0.CanonicalInput, $0.CanonicalOutput>(
          '/interop.v1.Interop/ComputeCanonical',
          ($0.CanonicalInput value) => value.writeToBuffer(),
          $0.CanonicalOutput.fromBuffer);
  static final _$buildChain =
      $grpc.ClientMethod<$0.ChainScenario, $0.ChainResult>(
          '/interop.v1.Interop/BuildChain',
          ($0.ChainScenario value) => value.writeToBuffer(),
          $0.ChainResult.fromBuffer);
  static final _$verifyProof =
      $grpc.ClientMethod<$0.ProofBundle, $0.VerifyResult>(
          '/interop.v1.Interop/VerifyProof',
          ($0.ProofBundle value) => value.writeToBuffer(),
          $0.VerifyResult.fromBuffer);
  static final _$buildTree = $grpc.ClientMethod<$0.TreeScenario, $0.TreeResult>(
      '/interop.v1.Interop/BuildTree',
      ($0.TreeScenario value) => value.writeToBuffer(),
      $0.TreeResult.fromBuffer);
  static final _$verifyTreeProof =
      $grpc.ClientMethod<$0.TreeProofBundle, $0.VerifyResult>(
          '/interop.v1.Interop/VerifyTreeProof',
          ($0.TreeProofBundle value) => value.writeToBuffer(),
          $0.VerifyResult.fromBuffer);
}

@$pb.GrpcServiceName('interop.v1.Interop')
abstract class InteropServiceBase extends $grpc.Service {
  $core.String get $name => 'interop.v1.Interop';

  InteropServiceBase() {
    $addMethod($grpc.ServiceMethod<$0.CanonicalInput, $0.CanonicalOutput>(
        'ComputeCanonical',
        computeCanonical_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.CanonicalInput.fromBuffer(value),
        ($0.CanonicalOutput value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.ChainScenario, $0.ChainResult>(
        'BuildChain',
        buildChain_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.ChainScenario.fromBuffer(value),
        ($0.ChainResult value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.ProofBundle, $0.VerifyResult>(
        'VerifyProof',
        verifyProof_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.ProofBundle.fromBuffer(value),
        ($0.VerifyResult value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.TreeScenario, $0.TreeResult>(
        'BuildTree',
        buildTree_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.TreeScenario.fromBuffer(value),
        ($0.TreeResult value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.TreeProofBundle, $0.VerifyResult>(
        'VerifyTreeProof',
        verifyTreeProof_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.TreeProofBundle.fromBuffer(value),
        ($0.VerifyResult value) => value.writeToBuffer()));
  }

  $async.Future<$0.CanonicalOutput> computeCanonical_Pre(
      $grpc.ServiceCall $call,
      $async.Future<$0.CanonicalInput> $request) async {
    return computeCanonical($call, await $request);
  }

  $async.Future<$0.CanonicalOutput> computeCanonical(
      $grpc.ServiceCall call, $0.CanonicalInput request);

  $async.Future<$0.ChainResult> buildChain_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.ChainScenario> $request) async {
    return buildChain($call, await $request);
  }

  $async.Future<$0.ChainResult> buildChain(
      $grpc.ServiceCall call, $0.ChainScenario request);

  $async.Future<$0.VerifyResult> verifyProof_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.ProofBundle> $request) async {
    return verifyProof($call, await $request);
  }

  $async.Future<$0.VerifyResult> verifyProof(
      $grpc.ServiceCall call, $0.ProofBundle request);

  $async.Future<$0.TreeResult> buildTree_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.TreeScenario> $request) async {
    return buildTree($call, await $request);
  }

  $async.Future<$0.TreeResult> buildTree(
      $grpc.ServiceCall call, $0.TreeScenario request);

  $async.Future<$0.VerifyResult> verifyTreeProof_Pre($grpc.ServiceCall $call,
      $async.Future<$0.TreeProofBundle> $request) async {
    return verifyTreeProof($call, await $request);
  }

  $async.Future<$0.VerifyResult> verifyTreeProof(
      $grpc.ServiceCall call, $0.TreeProofBundle request);
}
