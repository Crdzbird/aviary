// This is a generated file - do not edit.
//
// Generated from atlas.proto.

// @dart = 3.3

// ignore_for_file: annotate_overrides, camel_case_types, comment_references
// ignore_for_file: constant_identifier_names
// ignore_for_file: curly_braces_in_flow_control_structures
// ignore_for_file: deprecated_member_use_from_same_package, library_prefixes
// ignore_for_file: non_constant_identifier_names, prefer_relative_imports
// ignore_for_file: unused_import

import 'dart:convert' as $convert;
import 'dart:core' as $core;
import 'dart:typed_data' as $typed_data;

@$core.Deprecated('Use eventTypeDescriptor instead')
const EventType$json = {
  '1': 'EventType',
  '2': [
    {'1': 'EVENT_UNSPECIFIED', '2': 0},
    {'1': 'TX_ADMITTED', '2': 1},
    {'1': 'BLOCK_FINALIZED', '2': 2},
    {'1': 'TX_REMOVED', '2': 3},
  ],
};

/// Descriptor for `EventType`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List eventTypeDescriptor = $convert.base64Decode(
    'CglFdmVudFR5cGUSFQoRRVZFTlRfVU5TUEVDSUZJRUQQABIPCgtUWF9BRE1JVFRFRBABEhMKD0'
    'JMT0NLX0ZJTkFMSVpFRBACEg4KClRYX1JFTU9WRUQQAw==');

@$core.Deprecated('Use councilOpDescriptor instead')
const CouncilOp$json = {
  '1': 'CouncilOp',
  '2': [
    {'1': 'COUNCIL_PRODUCE', '2': 0},
    {'1': 'COUNCIL_ADD_VALIDATOR', '2': 1},
    {'1': 'COUNCIL_AUDITOR_SYNC', '2': 2},
  ],
};

/// Descriptor for `CouncilOp`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List councilOpDescriptor = $convert.base64Decode(
    'CglDb3VuY2lsT3ASEwoPQ09VTkNJTF9QUk9EVUNFEAASGQoVQ09VTkNJTF9BRERfVkFMSURBVE'
    '9SEAESGAoUQ09VTkNJTF9BVURJVE9SX1NZTkMQAg==');

@$core.Deprecated('Use emptyDescriptor instead')
const Empty$json = {
  '1': 'Empty',
};

/// Descriptor for `Empty`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List emptyDescriptor =
    $convert.base64Decode('CgVFbXB0eQ==');

@$core.Deprecated('Use createChainRequestDescriptor instead')
const CreateChainRequest$json = {
  '1': 'CreateChainRequest',
  '2': [
    {'1': 'name', '3': 1, '4': 1, '5': 9, '10': 'name'},
    {'1': 'chain_id', '3': 2, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'native_symbol', '3': 3, '4': 1, '5': 9, '10': 'nativeSymbol'},
  ],
};

/// Descriptor for `CreateChainRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List createChainRequestDescriptor = $convert.base64Decode(
    'ChJDcmVhdGVDaGFpblJlcXVlc3QSEgoEbmFtZRgBIAEoCVIEbmFtZRIZCghjaGFpbl9pZBgCIA'
    'EoBFIHY2hhaW5JZBIjCg1uYXRpdmVfc3ltYm9sGAMgASgJUgxuYXRpdmVTeW1ib2w=');

@$core.Deprecated('Use chainDescriptor instead')
const Chain$json = {
  '1': 'Chain',
  '2': [
    {'1': 'name', '3': 1, '4': 1, '5': 9, '10': 'name'},
    {'1': 'chain_id', '3': 2, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'native_symbol', '3': 3, '4': 1, '5': 9, '10': 'nativeSymbol'},
    {'1': 'height', '3': 4, '4': 1, '5': 4, '10': 'height'},
    {'1': 'wallet_count', '3': 5, '4': 1, '5': 13, '10': 'walletCount'},
    {'1': 'total_issued', '3': 6, '4': 1, '5': 4, '10': 'totalIssued'},
  ],
};

/// Descriptor for `Chain`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List chainDescriptor = $convert.base64Decode(
    'CgVDaGFpbhISCgRuYW1lGAEgASgJUgRuYW1lEhkKCGNoYWluX2lkGAIgASgEUgdjaGFpbklkEi'
    'MKDW5hdGl2ZV9zeW1ib2wYAyABKAlSDG5hdGl2ZVN5bWJvbBIWCgZoZWlnaHQYBCABKARSBmhl'
    'aWdodBIhCgx3YWxsZXRfY291bnQYBSABKA1SC3dhbGxldENvdW50EiEKDHRvdGFsX2lzc3VlZB'
    'gGIAEoBFILdG90YWxJc3N1ZWQ=');

@$core.Deprecated('Use chainListDescriptor instead')
const ChainList$json = {
  '1': 'ChainList',
  '2': [
    {
      '1': 'chains',
      '3': 1,
      '4': 3,
      '5': 11,
      '6': '.atlas.v1.Chain',
      '10': 'chains'
    },
  ],
};

/// Descriptor for `ChainList`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List chainListDescriptor = $convert.base64Decode(
    'CglDaGFpbkxpc3QSJwoGY2hhaW5zGAEgAygLMg8uYXRsYXMudjEuQ2hhaW5SBmNoYWlucw==');

@$core.Deprecated('Use chainRefDescriptor instead')
const ChainRef$json = {
  '1': 'ChainRef',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
  ],
};

/// Descriptor for `ChainRef`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List chainRefDescriptor = $convert
    .base64Decode('CghDaGFpblJlZhIZCghjaGFpbl9pZBgBIAEoBFIHY2hhaW5JZA==');

@$core.Deprecated('Use createWalletRequestDescriptor instead')
const CreateWalletRequest$json = {
  '1': 'CreateWalletRequest',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'label', '3': 2, '4': 1, '5': 9, '10': 'label'},
    {'1': 'institution', '3': 3, '4': 1, '5': 9, '10': 'institution'},
  ],
};

/// Descriptor for `CreateWalletRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List createWalletRequestDescriptor = $convert.base64Decode(
    'ChNDcmVhdGVXYWxsZXRSZXF1ZXN0EhkKCGNoYWluX2lkGAEgASgEUgdjaGFpbklkEhQKBWxhYm'
    'VsGAIgASgJUgVsYWJlbBIgCgtpbnN0aXR1dGlvbhgDIAEoCVILaW5zdGl0dXRpb24=');

@$core.Deprecated('Use walletDescriptor instead')
const Wallet$json = {
  '1': 'Wallet',
  '2': [
    {'1': 'id', '3': 1, '4': 1, '5': 9, '10': 'id'},
    {'1': 'label', '3': 2, '4': 1, '5': 9, '10': 'label'},
    {'1': 'institution', '3': 3, '4': 1, '5': 9, '10': 'institution'},
    {'1': 'address', '3': 4, '4': 1, '5': 9, '10': 'address'},
    {'1': 'balance', '3': 5, '4': 1, '5': 4, '10': 'balance'},
    {'1': 'nonce', '3': 6, '4': 1, '5': 4, '10': 'nonce'},
    {'1': 'eth_address', '3': 7, '4': 1, '5': 9, '10': 'ethAddress'},
    {'1': 'btc_address', '3': 8, '4': 1, '5': 9, '10': 'btcAddress'},
    {'1': 'sol_address', '3': 9, '4': 1, '5': 9, '10': 'solAddress'},
  ],
};

/// Descriptor for `Wallet`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List walletDescriptor = $convert.base64Decode(
    'CgZXYWxsZXQSDgoCaWQYASABKAlSAmlkEhQKBWxhYmVsGAIgASgJUgVsYWJlbBIgCgtpbnN0aX'
    'R1dGlvbhgDIAEoCVILaW5zdGl0dXRpb24SGAoHYWRkcmVzcxgEIAEoCVIHYWRkcmVzcxIYCgdi'
    'YWxhbmNlGAUgASgEUgdiYWxhbmNlEhQKBW5vbmNlGAYgASgEUgVub25jZRIfCgtldGhfYWRkcm'
    'VzcxgHIAEoCVIKZXRoQWRkcmVzcxIfCgtidGNfYWRkcmVzcxgIIAEoCVIKYnRjQWRkcmVzcxIf'
    'Cgtzb2xfYWRkcmVzcxgJIAEoCVIKc29sQWRkcmVzcw==');

@$core.Deprecated('Use walletListDescriptor instead')
const WalletList$json = {
  '1': 'WalletList',
  '2': [
    {
      '1': 'wallets',
      '3': 1,
      '4': 3,
      '5': 11,
      '6': '.atlas.v1.Wallet',
      '10': 'wallets'
    },
  ],
};

/// Descriptor for `WalletList`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List walletListDescriptor = $convert.base64Decode(
    'CgpXYWxsZXRMaXN0EioKB3dhbGxldHMYASADKAsyEC5hdGxhcy52MS5XYWxsZXRSB3dhbGxldH'
    'M=');

@$core.Deprecated('Use walletRefDescriptor instead')
const WalletRef$json = {
  '1': 'WalletRef',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'wallet_id', '3': 2, '4': 1, '5': 9, '10': 'walletId'},
  ],
};

/// Descriptor for `WalletRef`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List walletRefDescriptor = $convert.base64Decode(
    'CglXYWxsZXRSZWYSGQoIY2hhaW5faWQYASABKARSB2NoYWluSWQSGwoJd2FsbGV0X2lkGAIgAS'
    'gJUgh3YWxsZXRJZA==');

@$core.Deprecated('Use issueRequestDescriptor instead')
const IssueRequest$json = {
  '1': 'IssueRequest',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'wallet_id', '3': 2, '4': 1, '5': 9, '10': 'walletId'},
    {'1': 'amount', '3': 3, '4': 1, '5': 4, '10': 'amount'},
  ],
};

/// Descriptor for `IssueRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List issueRequestDescriptor = $convert.base64Decode(
    'CgxJc3N1ZVJlcXVlc3QSGQoIY2hhaW5faWQYASABKARSB2NoYWluSWQSGwoJd2FsbGV0X2lkGA'
    'IgASgJUgh3YWxsZXRJZBIWCgZhbW91bnQYAyABKARSBmFtb3VudA==');

@$core.Deprecated('Use settleRequestDescriptor instead')
const SettleRequest$json = {
  '1': 'SettleRequest',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'from_wallet_id', '3': 2, '4': 1, '5': 9, '10': 'fromWalletId'},
    {'1': 'to_wallet_id', '3': 3, '4': 1, '5': 9, '10': 'toWalletId'},
    {'1': 'amount', '3': 4, '4': 1, '5': 4, '10': 'amount'},
    {'1': 'memo', '3': 5, '4': 1, '5': 9, '10': 'memo'},
  ],
};

/// Descriptor for `SettleRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List settleRequestDescriptor = $convert.base64Decode(
    'Cg1TZXR0bGVSZXF1ZXN0EhkKCGNoYWluX2lkGAEgASgEUgdjaGFpbklkEiQKDmZyb21fd2FsbG'
    'V0X2lkGAIgASgJUgxmcm9tV2FsbGV0SWQSIAoMdG9fd2FsbGV0X2lkGAMgASgJUgp0b1dhbGxl'
    'dElkEhYKBmFtb3VudBgEIAEoBFIGYW1vdW50EhIKBG1lbW8YBSABKAlSBG1lbW8=');

@$core.Deprecated('Use settlementReceiptDescriptor instead')
const SettlementReceipt$json = {
  '1': 'SettlementReceipt',
  '2': [
    {'1': 'txid', '3': 1, '4': 1, '5': 9, '10': 'txid'},
    {'1': 'chain_id', '3': 2, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'from_address', '3': 3, '4': 1, '5': 9, '10': 'fromAddress'},
    {'1': 'to_address', '3': 4, '4': 1, '5': 9, '10': 'toAddress'},
    {'1': 'amount', '3': 5, '4': 1, '5': 4, '10': 'amount'},
    {'1': 'finalized', '3': 6, '4': 1, '5': 8, '10': 'finalized'},
  ],
};

/// Descriptor for `SettlementReceipt`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List settlementReceiptDescriptor = $convert.base64Decode(
    'ChFTZXR0bGVtZW50UmVjZWlwdBISCgR0eGlkGAEgASgJUgR0eGlkEhkKCGNoYWluX2lkGAIgAS'
    'gEUgdjaGFpbklkEiEKDGZyb21fYWRkcmVzcxgDIAEoCVILZnJvbUFkZHJlc3MSHQoKdG9fYWRk'
    'cmVzcxgEIAEoCVIJdG9BZGRyZXNzEhYKBmFtb3VudBgFIAEoBFIGYW1vdW50EhwKCWZpbmFsaX'
    'plZBgGIAEoCFIJZmluYWxpemVk');

@$core.Deprecated('Use blockDescriptor instead')
const Block$json = {
  '1': 'Block',
  '2': [
    {'1': 'height', '3': 1, '4': 1, '5': 4, '10': 'height'},
    {'1': 'tx_count', '3': 2, '4': 1, '5': 4, '10': 'txCount'},
    {'1': 'state_root', '3': 3, '4': 1, '5': 9, '10': 'stateRoot'},
  ],
};

/// Descriptor for `Block`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List blockDescriptor = $convert.base64Decode(
    'CgVCbG9jaxIWCgZoZWlnaHQYASABKARSBmhlaWdodBIZCgh0eF9jb3VudBgCIAEoBFIHdHhDb3'
    'VudBIdCgpzdGF0ZV9yb290GAMgASgJUglzdGF0ZVJvb3Q=');

@$core.Deprecated('Use balanceProofDescriptor instead')
const BalanceProof$json = {
  '1': 'BalanceProof',
  '2': [
    {'1': 'balance', '3': 1, '4': 1, '5': 4, '10': 'balance'},
    {'1': 'state_root', '3': 2, '4': 1, '5': 9, '10': 'stateRoot'},
    {'1': 'verified', '3': 3, '4': 1, '5': 8, '10': 'verified'},
    {'1': 'proof_bytes', '3': 4, '4': 1, '5': 13, '10': 'proofBytes'},
  ],
};

/// Descriptor for `BalanceProof`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List balanceProofDescriptor = $convert.base64Decode(
    'CgxCYWxhbmNlUHJvb2YSGAoHYmFsYW5jZRgBIAEoBFIHYmFsYW5jZRIdCgpzdGF0ZV9yb290GA'
    'IgASgJUglzdGF0ZVJvb3QSGgoIdmVyaWZpZWQYAyABKAhSCHZlcmlmaWVkEh8KC3Byb29mX2J5'
    'dGVzGAQgASgNUgpwcm9vZkJ5dGVz');

@$core.Deprecated('Use erc20RequestDescriptor instead')
const Erc20Request$json = {
  '1': 'Erc20Request',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'wallet_id', '3': 2, '4': 1, '5': 9, '10': 'walletId'},
    {'1': 'token_address', '3': 3, '4': 1, '5': 9, '10': 'tokenAddress'},
    {'1': 'to_address', '3': 4, '4': 1, '5': 9, '10': 'toAddress'},
    {'1': 'amount', '3': 5, '4': 1, '5': 4, '10': 'amount'},
    {'1': 'nonce', '3': 6, '4': 1, '5': 4, '10': 'nonce'},
    {'1': 'gas_price', '3': 7, '4': 1, '5': 4, '10': 'gasPrice'},
    {'1': 'gas_limit', '3': 8, '4': 1, '5': 4, '10': 'gasLimit'},
    {'1': 'evm_chain_id', '3': 9, '4': 1, '5': 4, '10': 'evmChainId'},
  ],
};

/// Descriptor for `Erc20Request`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List erc20RequestDescriptor = $convert.base64Decode(
    'CgxFcmMyMFJlcXVlc3QSGQoIY2hhaW5faWQYASABKARSB2NoYWluSWQSGwoJd2FsbGV0X2lkGA'
    'IgASgJUgh3YWxsZXRJZBIjCg10b2tlbl9hZGRyZXNzGAMgASgJUgx0b2tlbkFkZHJlc3MSHQoK'
    'dG9fYWRkcmVzcxgEIAEoCVIJdG9BZGRyZXNzEhYKBmFtb3VudBgFIAEoBFIGYW1vdW50EhQKBW'
    '5vbmNlGAYgASgEUgVub25jZRIbCglnYXNfcHJpY2UYByABKARSCGdhc1ByaWNlEhsKCWdhc19s'
    'aW1pdBgIIAEoBFIIZ2FzTGltaXQSIAoMZXZtX2NoYWluX2lkGAkgASgEUgpldm1DaGFpbklk');

@$core.Deprecated('Use signedEvmTxDescriptor instead')
const SignedEvmTx$json = {
  '1': 'SignedEvmTx',
  '2': [
    {'1': 'raw_tx_hex', '3': 1, '4': 1, '5': 9, '10': 'rawTxHex'},
    {'1': 'calldata_hex', '3': 2, '4': 1, '5': 9, '10': 'calldataHex'},
    {'1': 'from_address', '3': 3, '4': 1, '5': 9, '10': 'fromAddress'},
  ],
};

/// Descriptor for `SignedEvmTx`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List signedEvmTxDescriptor = $convert.base64Decode(
    'CgtTaWduZWRFdm1UeBIcCgpyYXdfdHhfaGV4GAEgASgJUghyYXdUeEhleBIhCgxjYWxsZGF0YV'
    '9oZXgYAiABKAlSC2NhbGxkYXRhSGV4EiEKDGZyb21fYWRkcmVzcxgDIAEoCVILZnJvbUFkZHJl'
    'c3M=');

@$core.Deprecated('Use selectorRequestDescriptor instead')
const SelectorRequest$json = {
  '1': 'SelectorRequest',
  '2': [
    {'1': 'signature', '3': 1, '4': 1, '5': 9, '10': 'signature'},
  ],
};

/// Descriptor for `SelectorRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List selectorRequestDescriptor = $convert.base64Decode(
    'Cg9TZWxlY3RvclJlcXVlc3QSHAoJc2lnbmF0dXJlGAEgASgJUglzaWduYXR1cmU=');

@$core.Deprecated('Use selectorReplyDescriptor instead')
const SelectorReply$json = {
  '1': 'SelectorReply',
  '2': [
    {'1': 'selector_hex', '3': 1, '4': 1, '5': 9, '10': 'selectorHex'},
  ],
};

/// Descriptor for `SelectorReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List selectorReplyDescriptor = $convert.base64Decode(
    'Cg1TZWxlY3RvclJlcGx5EiEKDHNlbGVjdG9yX2hleBgBIAEoCVILc2VsZWN0b3JIZXg=');

@$core.Deprecated('Use metricsDescriptor instead')
const Metrics$json = {
  '1': 'Metrics',
  '2': [
    {'1': 'height', '3': 1, '4': 1, '5': 4, '10': 'height'},
    {'1': 'blocks', '3': 2, '4': 1, '5': 4, '10': 'blocks'},
    {'1': 'transactions', '3': 3, '4': 1, '5': 4, '10': 'transactions'},
    {'1': 'mempool', '3': 4, '4': 1, '5': 4, '10': 'mempool'},
  ],
};

/// Descriptor for `Metrics`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List metricsDescriptor = $convert.base64Decode(
    'CgdNZXRyaWNzEhYKBmhlaWdodBgBIAEoBFIGaGVpZ2h0EhYKBmJsb2NrcxgCIAEoBFIGYmxvY2'
    'tzEiIKDHRyYW5zYWN0aW9ucxgDIAEoBFIMdHJhbnNhY3Rpb25zEhgKB21lbXBvb2wYBCABKARS'
    'B21lbXBvb2w=');

@$core.Deprecated('Use eventDescriptor instead')
const Event$json = {
  '1': 'Event',
  '2': [
    {
      '1': 'type',
      '3': 1,
      '4': 1,
      '5': 14,
      '6': '.atlas.v1.EventType',
      '10': 'type'
    },
    {'1': 'chain_id', '3': 2, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'height', '3': 3, '4': 1, '5': 4, '10': 'height'},
    {'1': 'tx_count', '3': 4, '4': 1, '5': 4, '10': 'txCount'},
    {'1': 'txid', '3': 5, '4': 1, '5': 9, '10': 'txid'},
    {'1': 'mempool_size', '3': 6, '4': 1, '5': 4, '10': 'mempoolSize'},
    {'1': 'state_root', '3': 7, '4': 1, '5': 9, '10': 'stateRoot'},
    {'1': 'unix_millis', '3': 8, '4': 1, '5': 3, '10': 'unixMillis'},
  ],
};

/// Descriptor for `Event`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List eventDescriptor = $convert.base64Decode(
    'CgVFdmVudBInCgR0eXBlGAEgASgOMhMuYXRsYXMudjEuRXZlbnRUeXBlUgR0eXBlEhkKCGNoYW'
    'luX2lkGAIgASgEUgdjaGFpbklkEhYKBmhlaWdodBgDIAEoBFIGaGVpZ2h0EhkKCHR4X2NvdW50'
    'GAQgASgEUgd0eENvdW50EhIKBHR4aWQYBSABKAlSBHR4aWQSIQoMbWVtcG9vbF9zaXplGAYgAS'
    'gEUgttZW1wb29sU2l6ZRIdCgpzdGF0ZV9yb290GAcgASgJUglzdGF0ZVJvb3QSHwoLdW5peF9t'
    'aWxsaXMYCCABKANSCnVuaXhNaWxsaXM=');

@$core.Deprecated('Use councilValidatorDescriptor instead')
const CouncilValidator$json = {
  '1': 'CouncilValidator',
  '2': [
    {'1': 'public_key', '3': 1, '4': 1, '5': 9, '10': 'publicKey'},
    {'1': 'power', '3': 2, '4': 1, '5': 4, '10': 'power'},
    {'1': 'operator', '3': 3, '4': 1, '5': 8, '10': 'operator'},
  ],
};

/// Descriptor for `CouncilValidator`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List councilValidatorDescriptor = $convert.base64Decode(
    'ChBDb3VuY2lsVmFsaWRhdG9yEh0KCnB1YmxpY19rZXkYASABKAlSCXB1YmxpY0tleRIUCgVwb3'
    'dlchgCIAEoBFIFcG93ZXISGgoIb3BlcmF0b3IYAyABKAhSCG9wZXJhdG9y');

@$core.Deprecated('Use councilStepDescriptor instead')
const CouncilStep$json = {
  '1': 'CouncilStep',
  '2': [
    {'1': 'label', '3': 1, '4': 1, '5': 9, '10': 'label'},
    {'1': 'ok', '3': 2, '4': 1, '5': 8, '10': 'ok'},
    {'1': 'detail', '3': 3, '4': 1, '5': 9, '10': 'detail'},
  ],
};

/// Descriptor for `CouncilStep`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List councilStepDescriptor = $convert.base64Decode(
    'CgtDb3VuY2lsU3RlcBIUCgVsYWJlbBgBIAEoCVIFbGFiZWwSDgoCb2sYAiABKAhSAm9rEhYKBm'
    'RldGFpbBgDIAEoCVIGZGV0YWls');

@$core.Deprecated('Use councilDescriptor instead')
const Council$json = {
  '1': 'Council',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'height', '3': 2, '4': 1, '5': 4, '10': 'height'},
    {
      '1': 'validators',
      '3': 3,
      '4': 3,
      '5': 11,
      '6': '.atlas.v1.CouncilValidator',
      '10': 'validators'
    },
    {'1': 'auditor_height', '3': 4, '4': 1, '5': 4, '10': 'auditorHeight'},
    {'1': 'rotations', '3': 5, '4': 1, '5': 13, '10': 'rotations'},
    {
      '1': 'log',
      '3': 6,
      '4': 3,
      '5': 11,
      '6': '.atlas.v1.CouncilStep',
      '10': 'log'
    },
  ],
};

/// Descriptor for `Council`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List councilDescriptor = $convert.base64Decode(
    'CgdDb3VuY2lsEhkKCGNoYWluX2lkGAEgASgEUgdjaGFpbklkEhYKBmhlaWdodBgCIAEoBFIGaG'
    'VpZ2h0EjoKCnZhbGlkYXRvcnMYAyADKAsyGi5hdGxhcy52MS5Db3VuY2lsVmFsaWRhdG9yUgp2'
    'YWxpZGF0b3JzEiUKDmF1ZGl0b3JfaGVpZ2h0GAQgASgEUg1hdWRpdG9ySGVpZ2h0EhwKCXJvdG'
    'F0aW9ucxgFIAEoDVIJcm90YXRpb25zEicKA2xvZxgGIAMoCzIVLmF0bGFzLnYxLkNvdW5jaWxT'
    'dGVwUgNsb2c=');

@$core.Deprecated('Use councilRequestDescriptor instead')
const CouncilRequest$json = {
  '1': 'CouncilRequest',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {
      '1': 'op',
      '3': 2,
      '4': 1,
      '5': 14,
      '6': '.atlas.v1.CouncilOp',
      '10': 'op'
    },
  ],
};

/// Descriptor for `CouncilRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List councilRequestDescriptor = $convert.base64Decode(
    'Cg5Db3VuY2lsUmVxdWVzdBIZCghjaGFpbl9pZBgBIAEoBFIHY2hhaW5JZBIjCgJvcBgCIAEoDj'
    'ITLmF0bGFzLnYxLkNvdW5jaWxPcFICb3A=');
