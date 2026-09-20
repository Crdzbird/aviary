// This is a generated file - do not edit.
//
// Generated from interop.proto.

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

@$core.Deprecated('Use canonicalInputDescriptor instead')
const CanonicalInput$json = {
  '1': 'CanonicalInput',
  '2': [
    {'1': 'message', '3': 1, '4': 1, '5': 12, '10': 'message'},
    {'1': 'tx_version', '3': 2, '4': 1, '5': 13, '10': 'txVersion'},
    {'1': 'chain_id', '3': 3, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'tx_body', '3': 4, '4': 1, '5': 12, '10': 'txBody'},
    {'1': 'tx_author', '3': 5, '4': 1, '5': 12, '10': 'txAuthor'},
    {'1': 'tx_signature', '3': 6, '4': 1, '5': 12, '10': 'txSignature'},
    {'1': 'hdr_version', '3': 7, '4': 1, '5': 13, '10': 'hdrVersion'},
    {'1': 'height', '3': 8, '4': 1, '5': 4, '10': 'height'},
    {'1': 'parent_hash', '3': 9, '4': 1, '5': 12, '10': 'parentHash'},
    {'1': 'timestamp', '3': 10, '4': 1, '5': 4, '10': 'timestamp'},
    {'1': 'tx_root', '3': 11, '4': 1, '5': 12, '10': 'txRoot'},
    {'1': 'state_root', '3': 12, '4': 1, '5': 12, '10': 'stateRoot'},
    {'1': 'consensus', '3': 13, '4': 1, '5': 12, '10': 'consensus'},
  ],
};

/// Descriptor for `CanonicalInput`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List canonicalInputDescriptor = $convert.base64Decode(
    'Cg5DYW5vbmljYWxJbnB1dBIYCgdtZXNzYWdlGAEgASgMUgdtZXNzYWdlEh0KCnR4X3ZlcnNpb2'
    '4YAiABKA1SCXR4VmVyc2lvbhIZCghjaGFpbl9pZBgDIAEoBFIHY2hhaW5JZBIXCgd0eF9ib2R5'
    'GAQgASgMUgZ0eEJvZHkSGwoJdHhfYXV0aG9yGAUgASgMUgh0eEF1dGhvchIhCgx0eF9zaWduYX'
    'R1cmUYBiABKAxSC3R4U2lnbmF0dXJlEh8KC2hkcl92ZXJzaW9uGAcgASgNUgpoZHJWZXJzaW9u'
    'EhYKBmhlaWdodBgIIAEoBFIGaGVpZ2h0Eh8KC3BhcmVudF9oYXNoGAkgASgMUgpwYXJlbnRIYX'
    'NoEhwKCXRpbWVzdGFtcBgKIAEoBFIJdGltZXN0YW1wEhcKB3R4X3Jvb3QYCyABKAxSBnR4Um9v'
    'dBIdCgpzdGF0ZV9yb290GAwgASgMUglzdGF0ZVJvb3QSHAoJY29uc2Vuc3VzGA0gASgMUgljb2'
    '5zZW5zdXM=');

@$core.Deprecated('Use canonicalOutputDescriptor instead')
const CanonicalOutput$json = {
  '1': 'CanonicalOutput',
  '2': [
    {'1': 'hash', '3': 1, '4': 1, '5': 12, '10': 'hash'},
    {'1': 'txid', '3': 2, '4': 1, '5': 12, '10': 'txid'},
    {'1': 'header_hash', '3': 3, '4': 1, '5': 12, '10': 'headerHash'},
  ],
};

/// Descriptor for `CanonicalOutput`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List canonicalOutputDescriptor = $convert.base64Decode(
    'Cg9DYW5vbmljYWxPdXRwdXQSEgoEaGFzaBgBIAEoDFIEaGFzaBISCgR0eGlkGAIgASgMUgR0eG'
    'lkEh8KC2hlYWRlcl9oYXNoGAMgASgMUgpoZWFkZXJIYXNo');

@$core.Deprecated('Use chainScenarioDescriptor instead')
const ChainScenario$json = {
  '1': 'ChainScenario',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'alice_secret', '3': 2, '4': 1, '5': 12, '10': 'aliceSecret'},
    {'1': 'bob_secret', '3': 3, '4': 1, '5': 12, '10': 'bobSecret'},
    {'1': 'alice_credit', '3': 4, '4': 1, '5': 4, '10': 'aliceCredit'},
    {'1': 'transfer_amount', '3': 5, '4': 1, '5': 4, '10': 'transferAmount'},
    {'1': 'timestamp', '3': 6, '4': 1, '5': 4, '10': 'timestamp'},
  ],
};

/// Descriptor for `ChainScenario`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List chainScenarioDescriptor = $convert.base64Decode(
    'Cg1DaGFpblNjZW5hcmlvEhkKCGNoYWluX2lkGAEgASgEUgdjaGFpbklkEiEKDGFsaWNlX3NlY3'
    'JldBgCIAEoDFILYWxpY2VTZWNyZXQSHQoKYm9iX3NlY3JldBgDIAEoDFIJYm9iU2VjcmV0EiEK'
    'DGFsaWNlX2NyZWRpdBgEIAEoBFILYWxpY2VDcmVkaXQSJwoPdHJhbnNmZXJfYW1vdW50GAUgAS'
    'gEUg50cmFuc2ZlckFtb3VudBIcCgl0aW1lc3RhbXAYBiABKARSCXRpbWVzdGFtcA==');

@$core.Deprecated('Use chainResultDescriptor instead')
const ChainResult$json = {
  '1': 'ChainResult',
  '2': [
    {'1': 'height', '3': 1, '4': 1, '5': 4, '10': 'height'},
    {'1': 'state_root', '3': 2, '4': 1, '5': 12, '10': 'stateRoot'},
    {'1': 'alice_address', '3': 3, '4': 1, '5': 12, '10': 'aliceAddress'},
    {'1': 'bob_address', '3': 4, '4': 1, '5': 12, '10': 'bobAddress'},
    {'1': 'alice_balance', '3': 5, '4': 1, '5': 4, '10': 'aliceBalance'},
    {'1': 'bob_balance', '3': 6, '4': 1, '5': 4, '10': 'bobBalance'},
    {'1': 'proof', '3': 7, '4': 1, '5': 12, '10': 'proof'},
  ],
};

/// Descriptor for `ChainResult`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List chainResultDescriptor = $convert.base64Decode(
    'CgtDaGFpblJlc3VsdBIWCgZoZWlnaHQYASABKARSBmhlaWdodBIdCgpzdGF0ZV9yb290GAIgAS'
    'gMUglzdGF0ZVJvb3QSIwoNYWxpY2VfYWRkcmVzcxgDIAEoDFIMYWxpY2VBZGRyZXNzEh8KC2Jv'
    'Yl9hZGRyZXNzGAQgASgMUgpib2JBZGRyZXNzEiMKDWFsaWNlX2JhbGFuY2UYBSABKARSDGFsaW'
    'NlQmFsYW5jZRIfCgtib2JfYmFsYW5jZRgGIAEoBFIKYm9iQmFsYW5jZRIUCgVwcm9vZhgHIAEo'
    'DFIFcHJvb2Y=');

@$core.Deprecated('Use proofBundleDescriptor instead')
const ProofBundle$json = {
  '1': 'ProofBundle',
  '2': [
    {'1': 'state_root', '3': 1, '4': 1, '5': 12, '10': 'stateRoot'},
    {'1': 'proof', '3': 2, '4': 1, '5': 12, '10': 'proof'},
  ],
};

/// Descriptor for `ProofBundle`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List proofBundleDescriptor = $convert.base64Decode(
    'CgtQcm9vZkJ1bmRsZRIdCgpzdGF0ZV9yb290GAEgASgMUglzdGF0ZVJvb3QSFAoFcHJvb2YYAi'
    'ABKAxSBXByb29m');

@$core.Deprecated('Use verifyResultDescriptor instead')
const VerifyResult$json = {
  '1': 'VerifyResult',
  '2': [
    {'1': 'ok', '3': 1, '4': 1, '5': 8, '10': 'ok'},
  ],
};

/// Descriptor for `VerifyResult`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List verifyResultDescriptor =
    $convert.base64Decode('CgxWZXJpZnlSZXN1bHQSDgoCb2sYASABKAhSAm9r');

@$core.Deprecated('Use treeScenarioDescriptor instead')
const TreeScenario$json = {
  '1': 'TreeScenario',
  '2': [
    {'1': 'seed', '3': 1, '4': 1, '5': 12, '10': 'seed'},
    {'1': 'alice_address', '3': 2, '4': 1, '5': 12, '10': 'aliceAddress'},
    {'1': 'alice_credit', '3': 3, '4': 1, '5': 4, '10': 'aliceCredit'},
  ],
};

/// Descriptor for `TreeScenario`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List treeScenarioDescriptor = $convert.base64Decode(
    'CgxUcmVlU2NlbmFyaW8SEgoEc2VlZBgBIAEoDFIEc2VlZBIjCg1hbGljZV9hZGRyZXNzGAIgAS'
    'gMUgxhbGljZUFkZHJlc3MSIQoMYWxpY2VfY3JlZGl0GAMgASgEUgthbGljZUNyZWRpdA==');

@$core.Deprecated('Use treeResultDescriptor instead')
const TreeResult$json = {
  '1': 'TreeResult',
  '2': [
    {'1': 'chain_ids', '3': 1, '4': 3, '5': 4, '10': 'chainIds'},
    {
      '1': 'root_subchain_root',
      '3': 2,
      '4': 1,
      '5': 12,
      '10': 'rootSubchainRoot'
    },
    {'1': 'root_tip_header', '3': 3, '4': 1, '5': 12, '10': 'rootTipHeader'},
    {'1': 'proof', '3': 4, '4': 1, '5': 12, '10': 'proof'},
    {'1': 'root_height', '3': 5, '4': 1, '5': 4, '10': 'rootHeight'},
  ],
};

/// Descriptor for `TreeResult`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List treeResultDescriptor = $convert.base64Decode(
    'CgpUcmVlUmVzdWx0EhsKCWNoYWluX2lkcxgBIAMoBFIIY2hhaW5JZHMSLAoScm9vdF9zdWJjaG'
    'Fpbl9yb290GAIgASgMUhByb290U3ViY2hhaW5Sb290EiYKD3Jvb3RfdGlwX2hlYWRlchgDIAEo'
    'DFINcm9vdFRpcEhlYWRlchIUCgVwcm9vZhgEIAEoDFIFcHJvb2YSHwoLcm9vdF9oZWlnaHQYBS'
    'ABKARSCnJvb3RIZWlnaHQ=');

@$core.Deprecated('Use treeProofBundleDescriptor instead')
const TreeProofBundle$json = {
  '1': 'TreeProofBundle',
  '2': [
    {
      '1': 'root_subchain_root',
      '3': 1,
      '4': 1,
      '5': 12,
      '10': 'rootSubchainRoot'
    },
    {'1': 'proof', '3': 2, '4': 1, '5': 12, '10': 'proof'},
  ],
};

/// Descriptor for `TreeProofBundle`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List treeProofBundleDescriptor = $convert.base64Decode(
    'Cg9UcmVlUHJvb2ZCdW5kbGUSLAoScm9vdF9zdWJjaGFpbl9yb290GAEgASgMUhByb290U3ViY2'
    'hhaW5Sb290EhQKBXByb29mGAIgASgMUgVwcm9vZg==');
