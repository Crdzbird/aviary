// This is a generated file - do not edit.
//
// Generated from atlas.proto.

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

import 'atlas.pb.dart' as $0;

export 'atlas.pb.dart';

@$pb.GrpcServiceName('atlas.v1.Atlas')
class AtlasClient extends $grpc.Client {
  /// The hostname for this service.
  static const $core.String defaultHost = '';

  /// OAuth scopes needed for the client.
  static const $core.List<$core.String> oauthScopes = [
    '',
  ];

  AtlasClient(super.channel, {super.options, super.interceptors});

  /// Chains (settlement rails).
  $grpc.ResponseFuture<$0.Chain> createChain(
    $0.CreateChainRequest request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$createChain, request, options: options);
  }

  $grpc.ResponseFuture<$0.ChainList> listChains(
    $0.Empty request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$listChains, request, options: options);
  }

  /// Wallets (institutional custody accounts).
  $grpc.ResponseFuture<$0.Wallet> createWallet(
    $0.CreateWalletRequest request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$createWallet, request, options: options);
  }

  $grpc.ResponseFuture<$0.WalletList> listWallets(
    $0.ChainRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$listWallets, request, options: options);
  }

  $grpc.ResponseFuture<$0.Wallet> getWallet(
    $0.WalletRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$getWallet, request, options: options);
  }

  $grpc.ResponseFuture<$0.Wallet> issueFunds(
    $0.IssueRequest request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$issueFunds, request, options: options);
  }

  /// Settlement + finality.
  $grpc.ResponseFuture<$0.SettlementReceipt> settle(
    $0.SettleRequest request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$settle, request, options: options);
  }

  $grpc.ResponseFuture<$0.Block> produceBlock(
    $0.ChainRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$produceBlock, request, options: options);
  }

  /// Auditability: a verifiable light-client proof of a wallet's balance.
  $grpc.ResponseFuture<$0.BalanceProof> getBalanceProof(
    $0.WalletRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$getBalanceProof, request, options: options);
  }

  /// Smart contracts (EVM): build + sign a real ERC-20 transfer to broadcast.
  $grpc.ResponseFuture<$0.SignedEvmTx> buildErc20Transfer(
    $0.Erc20Request request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$buildErc20Transfer, request, options: options);
  }

  $grpc.ResponseFuture<$0.SelectorReply> functionSelector(
    $0.SelectorRequest request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$functionSelector, request, options: options);
  }

  $grpc.ResponseFuture<$0.Metrics> getMetrics(
    $0.ChainRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$getMetrics, request, options: options);
  }

  /// Live ledger feed (server-streaming): admissions, finalizations, removals.
  $grpc.ResponseStream<$0.Event> streamEvents(
    $0.ChainRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createStreamingCall(
        _$streamEvents, $async.Stream.fromIterable([request]),
        options: options);
  }

  /// ── Governance: a validator council (coordinated rotation) + an auditor
  /// light client that syncs with the OVERLAP rule (bisection through rotations).
  $grpc.ResponseFuture<$0.Council> getCouncil(
    $0.ChainRef request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$getCouncil, request, options: options);
  }

  $grpc.ResponseFuture<$0.Council> councilAction(
    $0.CouncilRequest request, {
    $grpc.CallOptions? options,
  }) {
    return $createUnaryCall(_$councilAction, request, options: options);
  }

  // method descriptors

  static final _$createChain =
      $grpc.ClientMethod<$0.CreateChainRequest, $0.Chain>(
          '/atlas.v1.Atlas/CreateChain',
          ($0.CreateChainRequest value) => value.writeToBuffer(),
          $0.Chain.fromBuffer);
  static final _$listChains = $grpc.ClientMethod<$0.Empty, $0.ChainList>(
      '/atlas.v1.Atlas/ListChains',
      ($0.Empty value) => value.writeToBuffer(),
      $0.ChainList.fromBuffer);
  static final _$createWallet =
      $grpc.ClientMethod<$0.CreateWalletRequest, $0.Wallet>(
          '/atlas.v1.Atlas/CreateWallet',
          ($0.CreateWalletRequest value) => value.writeToBuffer(),
          $0.Wallet.fromBuffer);
  static final _$listWallets = $grpc.ClientMethod<$0.ChainRef, $0.WalletList>(
      '/atlas.v1.Atlas/ListWallets',
      ($0.ChainRef value) => value.writeToBuffer(),
      $0.WalletList.fromBuffer);
  static final _$getWallet = $grpc.ClientMethod<$0.WalletRef, $0.Wallet>(
      '/atlas.v1.Atlas/GetWallet',
      ($0.WalletRef value) => value.writeToBuffer(),
      $0.Wallet.fromBuffer);
  static final _$issueFunds = $grpc.ClientMethod<$0.IssueRequest, $0.Wallet>(
      '/atlas.v1.Atlas/IssueFunds',
      ($0.IssueRequest value) => value.writeToBuffer(),
      $0.Wallet.fromBuffer);
  static final _$settle =
      $grpc.ClientMethod<$0.SettleRequest, $0.SettlementReceipt>(
          '/atlas.v1.Atlas/Settle',
          ($0.SettleRequest value) => value.writeToBuffer(),
          $0.SettlementReceipt.fromBuffer);
  static final _$produceBlock = $grpc.ClientMethod<$0.ChainRef, $0.Block>(
      '/atlas.v1.Atlas/ProduceBlock',
      ($0.ChainRef value) => value.writeToBuffer(),
      $0.Block.fromBuffer);
  static final _$getBalanceProof =
      $grpc.ClientMethod<$0.WalletRef, $0.BalanceProof>(
          '/atlas.v1.Atlas/GetBalanceProof',
          ($0.WalletRef value) => value.writeToBuffer(),
          $0.BalanceProof.fromBuffer);
  static final _$buildErc20Transfer =
      $grpc.ClientMethod<$0.Erc20Request, $0.SignedEvmTx>(
          '/atlas.v1.Atlas/BuildErc20Transfer',
          ($0.Erc20Request value) => value.writeToBuffer(),
          $0.SignedEvmTx.fromBuffer);
  static final _$functionSelector =
      $grpc.ClientMethod<$0.SelectorRequest, $0.SelectorReply>(
          '/atlas.v1.Atlas/FunctionSelector',
          ($0.SelectorRequest value) => value.writeToBuffer(),
          $0.SelectorReply.fromBuffer);
  static final _$getMetrics = $grpc.ClientMethod<$0.ChainRef, $0.Metrics>(
      '/atlas.v1.Atlas/GetMetrics',
      ($0.ChainRef value) => value.writeToBuffer(),
      $0.Metrics.fromBuffer);
  static final _$streamEvents = $grpc.ClientMethod<$0.ChainRef, $0.Event>(
      '/atlas.v1.Atlas/StreamEvents',
      ($0.ChainRef value) => value.writeToBuffer(),
      $0.Event.fromBuffer);
  static final _$getCouncil = $grpc.ClientMethod<$0.ChainRef, $0.Council>(
      '/atlas.v1.Atlas/GetCouncil',
      ($0.ChainRef value) => value.writeToBuffer(),
      $0.Council.fromBuffer);
  static final _$councilAction =
      $grpc.ClientMethod<$0.CouncilRequest, $0.Council>(
          '/atlas.v1.Atlas/CouncilAction',
          ($0.CouncilRequest value) => value.writeToBuffer(),
          $0.Council.fromBuffer);
}

@$pb.GrpcServiceName('atlas.v1.Atlas')
abstract class AtlasServiceBase extends $grpc.Service {
  $core.String get $name => 'atlas.v1.Atlas';

  AtlasServiceBase() {
    $addMethod($grpc.ServiceMethod<$0.CreateChainRequest, $0.Chain>(
        'CreateChain',
        createChain_Pre,
        false,
        false,
        ($core.List<$core.int> value) =>
            $0.CreateChainRequest.fromBuffer(value),
        ($0.Chain value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.Empty, $0.ChainList>(
        'ListChains',
        listChains_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.Empty.fromBuffer(value),
        ($0.ChainList value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.CreateWalletRequest, $0.Wallet>(
        'CreateWallet',
        createWallet_Pre,
        false,
        false,
        ($core.List<$core.int> value) =>
            $0.CreateWalletRequest.fromBuffer(value),
        ($0.Wallet value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.ChainRef, $0.WalletList>(
        'ListWallets',
        listWallets_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.ChainRef.fromBuffer(value),
        ($0.WalletList value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.WalletRef, $0.Wallet>(
        'GetWallet',
        getWallet_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.WalletRef.fromBuffer(value),
        ($0.Wallet value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.IssueRequest, $0.Wallet>(
        'IssueFunds',
        issueFunds_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.IssueRequest.fromBuffer(value),
        ($0.Wallet value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.SettleRequest, $0.SettlementReceipt>(
        'Settle',
        settle_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.SettleRequest.fromBuffer(value),
        ($0.SettlementReceipt value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.ChainRef, $0.Block>(
        'ProduceBlock',
        produceBlock_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.ChainRef.fromBuffer(value),
        ($0.Block value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.WalletRef, $0.BalanceProof>(
        'GetBalanceProof',
        getBalanceProof_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.WalletRef.fromBuffer(value),
        ($0.BalanceProof value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.Erc20Request, $0.SignedEvmTx>(
        'BuildErc20Transfer',
        buildErc20Transfer_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.Erc20Request.fromBuffer(value),
        ($0.SignedEvmTx value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.SelectorRequest, $0.SelectorReply>(
        'FunctionSelector',
        functionSelector_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.SelectorRequest.fromBuffer(value),
        ($0.SelectorReply value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.ChainRef, $0.Metrics>(
        'GetMetrics',
        getMetrics_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.ChainRef.fromBuffer(value),
        ($0.Metrics value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.ChainRef, $0.Event>(
        'StreamEvents',
        streamEvents_Pre,
        false,
        true,
        ($core.List<$core.int> value) => $0.ChainRef.fromBuffer(value),
        ($0.Event value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.ChainRef, $0.Council>(
        'GetCouncil',
        getCouncil_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.ChainRef.fromBuffer(value),
        ($0.Council value) => value.writeToBuffer()));
    $addMethod($grpc.ServiceMethod<$0.CouncilRequest, $0.Council>(
        'CouncilAction',
        councilAction_Pre,
        false,
        false,
        ($core.List<$core.int> value) => $0.CouncilRequest.fromBuffer(value),
        ($0.Council value) => value.writeToBuffer()));
  }

  $async.Future<$0.Chain> createChain_Pre($grpc.ServiceCall $call,
      $async.Future<$0.CreateChainRequest> $request) async {
    return createChain($call, await $request);
  }

  $async.Future<$0.Chain> createChain(
      $grpc.ServiceCall call, $0.CreateChainRequest request);

  $async.Future<$0.ChainList> listChains_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.Empty> $request) async {
    return listChains($call, await $request);
  }

  $async.Future<$0.ChainList> listChains(
      $grpc.ServiceCall call, $0.Empty request);

  $async.Future<$0.Wallet> createWallet_Pre($grpc.ServiceCall $call,
      $async.Future<$0.CreateWalletRequest> $request) async {
    return createWallet($call, await $request);
  }

  $async.Future<$0.Wallet> createWallet(
      $grpc.ServiceCall call, $0.CreateWalletRequest request);

  $async.Future<$0.WalletList> listWallets_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.ChainRef> $request) async {
    return listWallets($call, await $request);
  }

  $async.Future<$0.WalletList> listWallets(
      $grpc.ServiceCall call, $0.ChainRef request);

  $async.Future<$0.Wallet> getWallet_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.WalletRef> $request) async {
    return getWallet($call, await $request);
  }

  $async.Future<$0.Wallet> getWallet(
      $grpc.ServiceCall call, $0.WalletRef request);

  $async.Future<$0.Wallet> issueFunds_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.IssueRequest> $request) async {
    return issueFunds($call, await $request);
  }

  $async.Future<$0.Wallet> issueFunds(
      $grpc.ServiceCall call, $0.IssueRequest request);

  $async.Future<$0.SettlementReceipt> settle_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.SettleRequest> $request) async {
    return settle($call, await $request);
  }

  $async.Future<$0.SettlementReceipt> settle(
      $grpc.ServiceCall call, $0.SettleRequest request);

  $async.Future<$0.Block> produceBlock_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.ChainRef> $request) async {
    return produceBlock($call, await $request);
  }

  $async.Future<$0.Block> produceBlock(
      $grpc.ServiceCall call, $0.ChainRef request);

  $async.Future<$0.BalanceProof> getBalanceProof_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.WalletRef> $request) async {
    return getBalanceProof($call, await $request);
  }

  $async.Future<$0.BalanceProof> getBalanceProof(
      $grpc.ServiceCall call, $0.WalletRef request);

  $async.Future<$0.SignedEvmTx> buildErc20Transfer_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.Erc20Request> $request) async {
    return buildErc20Transfer($call, await $request);
  }

  $async.Future<$0.SignedEvmTx> buildErc20Transfer(
      $grpc.ServiceCall call, $0.Erc20Request request);

  $async.Future<$0.SelectorReply> functionSelector_Pre($grpc.ServiceCall $call,
      $async.Future<$0.SelectorRequest> $request) async {
    return functionSelector($call, await $request);
  }

  $async.Future<$0.SelectorReply> functionSelector(
      $grpc.ServiceCall call, $0.SelectorRequest request);

  $async.Future<$0.Metrics> getMetrics_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.ChainRef> $request) async {
    return getMetrics($call, await $request);
  }

  $async.Future<$0.Metrics> getMetrics(
      $grpc.ServiceCall call, $0.ChainRef request);

  $async.Stream<$0.Event> streamEvents_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.ChainRef> $request) async* {
    yield* streamEvents($call, await $request);
  }

  $async.Stream<$0.Event> streamEvents(
      $grpc.ServiceCall call, $0.ChainRef request);

  $async.Future<$0.Council> getCouncil_Pre(
      $grpc.ServiceCall $call, $async.Future<$0.ChainRef> $request) async {
    return getCouncil($call, await $request);
  }

  $async.Future<$0.Council> getCouncil(
      $grpc.ServiceCall call, $0.ChainRef request);

  $async.Future<$0.Council> councilAction_Pre($grpc.ServiceCall $call,
      $async.Future<$0.CouncilRequest> $request) async {
    return councilAction($call, await $request);
  }

  $async.Future<$0.Council> councilAction(
      $grpc.ServiceCall call, $0.CouncilRequest request);
}
