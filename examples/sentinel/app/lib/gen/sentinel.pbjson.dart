// This is a generated file - do not edit.
//
// Generated from sentinel.proto.

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

@$core.Deprecated('Use dataKindDescriptor instead')
const DataKind$json = {
  '1': 'DataKind',
  '2': [
    {'1': 'DATA_UNSPECIFIED', '2': 0},
    {'1': 'PHOTO', '2': 1},
    {'1': 'VIDEO', '2': 2},
    {'1': 'LOCATION', '2': 3},
    {'1': 'SENSOR', '2': 4},
  ],
};

/// Descriptor for `DataKind`. Decode as a `google.protobuf.EnumDescriptorProto`.
final $typed_data.Uint8List dataKindDescriptor = $convert.base64Decode(
    'CghEYXRhS2luZBIUChBEQVRBX1VOU1BFQ0lGSUVEEAASCQoFUEhPVE8QARIJCgVWSURFTxACEg'
    'wKCExPQ0FUSU9OEAMSCgoGU0VOU09SEAQ=');

@$core.Deprecated('Use deviceInfoDescriptor instead')
const DeviceInfo$json = {
  '1': 'DeviceInfo',
  '2': [
    {'1': 'device_id', '3': 1, '4': 1, '5': 9, '10': 'deviceId'},
    {'1': 'model', '3': 2, '4': 1, '5': 9, '10': 'model'},
    {'1': 'platform', '3': 3, '4': 1, '5': 9, '10': 'platform'},
    {'1': 'os_version', '3': 4, '4': 1, '5': 9, '10': 'osVersion'},
    {'1': 'label', '3': 5, '4': 1, '5': 9, '10': 'label'},
    {'1': 'credential', '3': 6, '4': 1, '5': 12, '10': 'credential'},
  ],
};

/// Descriptor for `DeviceInfo`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List deviceInfoDescriptor = $convert.base64Decode(
    'CgpEZXZpY2VJbmZvEhsKCWRldmljZV9pZBgBIAEoCVIIZGV2aWNlSWQSFAoFbW9kZWwYAiABKA'
    'lSBW1vZGVsEhoKCHBsYXRmb3JtGAMgASgJUghwbGF0Zm9ybRIdCgpvc192ZXJzaW9uGAQgASgJ'
    'Uglvc1ZlcnNpb24SFAoFbGFiZWwYBSABKAlSBWxhYmVsEh4KCmNyZWRlbnRpYWwYBiABKAxSCm'
    'NyZWRlbnRpYWw=');

@$core.Deprecated('Use provenanceDescriptor instead')
const Provenance$json = {
  '1': 'Provenance',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {
      '1': 'device_fingerprint',
      '3': 2,
      '4': 1,
      '5': 9,
      '10': 'deviceFingerprint'
    },
    {'1': 'genesis_hash', '3': 3, '4': 1, '5': 9, '10': 'genesisHash'},
    {'1': 'device_address', '3': 4, '4': 1, '5': 9, '10': 'deviceAddress'},
    {'1': 'genesis_height', '3': 5, '4': 1, '5': 4, '10': 'genesisHeight'},
  ],
};

/// Descriptor for `Provenance`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List provenanceDescriptor = $convert.base64Decode(
    'CgpQcm92ZW5hbmNlEhkKCGNoYWluX2lkGAEgASgEUgdjaGFpbklkEi0KEmRldmljZV9maW5nZX'
    'JwcmludBgCIAEoCVIRZGV2aWNlRmluZ2VycHJpbnQSIQoMZ2VuZXNpc19oYXNoGAMgASgJUgtn'
    'ZW5lc2lzSGFzaBIlCg5kZXZpY2VfYWRkcmVzcxgEIAEoCVINZGV2aWNlQWRkcmVzcxIlCg5nZW'
    '5lc2lzX2hlaWdodBgFIAEoBFINZ2VuZXNpc0hlaWdodA==');

@$core.Deprecated('Use dataRecordDescriptor instead')
const DataRecord$json = {
  '1': 'DataRecord',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {
      '1': 'kind',
      '3': 2,
      '4': 1,
      '5': 14,
      '6': '.sentinel.v1.DataKind',
      '10': 'kind'
    },
    {'1': 'payload', '3': 3, '4': 1, '5': 12, '10': 'payload'},
    {'1': 'mime', '3': 4, '4': 1, '5': 9, '10': 'mime'},
    {'1': 'lat', '3': 5, '4': 1, '5': 1, '10': 'lat'},
    {'1': 'lon', '3': 6, '4': 1, '5': 1, '10': 'lon'},
    {'1': 'accuracy_m', '3': 7, '4': 1, '5': 1, '10': 'accuracyM'},
    {
      '1': 'sensors',
      '3': 8,
      '4': 3,
      '5': 11,
      '6': '.sentinel.v1.DataRecord.SensorsEntry',
      '10': 'sensors'
    },
    {
      '1': 'captured_unix_millis',
      '3': 9,
      '4': 1,
      '5': 3,
      '10': 'capturedUnixMillis'
    },
    {'1': 'note', '3': 10, '4': 1, '5': 9, '10': 'note'},
  ],
  '3': [DataRecord_SensorsEntry$json],
};

@$core.Deprecated('Use dataRecordDescriptor instead')
const DataRecord_SensorsEntry$json = {
  '1': 'SensorsEntry',
  '2': [
    {'1': 'key', '3': 1, '4': 1, '5': 9, '10': 'key'},
    {'1': 'value', '3': 2, '4': 1, '5': 1, '10': 'value'},
  ],
  '7': {'7': true},
};

/// Descriptor for `DataRecord`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List dataRecordDescriptor = $convert.base64Decode(
    'CgpEYXRhUmVjb3JkEhkKCGNoYWluX2lkGAEgASgEUgdjaGFpbklkEikKBGtpbmQYAiABKA4yFS'
    '5zZW50aW5lbC52MS5EYXRhS2luZFIEa2luZBIYCgdwYXlsb2FkGAMgASgMUgdwYXlsb2FkEhIK'
    'BG1pbWUYBCABKAlSBG1pbWUSEAoDbGF0GAUgASgBUgNsYXQSEAoDbG9uGAYgASgBUgNsb24SHQ'
    'oKYWNjdXJhY3lfbRgHIAEoAVIJYWNjdXJhY3lNEj4KB3NlbnNvcnMYCCADKAsyJC5zZW50aW5l'
    'bC52MS5EYXRhUmVjb3JkLlNlbnNvcnNFbnRyeVIHc2Vuc29ycxIwChRjYXB0dXJlZF91bml4X2'
    '1pbGxpcxgJIAEoA1ISY2FwdHVyZWRVbml4TWlsbGlzEhIKBG5vdGUYCiABKAlSBG5vdGUaOgoM'
    'U2Vuc29yc0VudHJ5EhAKA2tleRgBIAEoCVIDa2V5EhQKBXZhbHVlGAIgASgBUgV2YWx1ZToCOA'
    'E=');

@$core.Deprecated('Use anchorReceiptDescriptor instead')
const AnchorReceipt$json = {
  '1': 'AnchorReceipt',
  '2': [
    {'1': 'sequence', '3': 1, '4': 1, '5': 4, '10': 'sequence'},
    {
      '1': 'kind',
      '3': 2,
      '4': 1,
      '5': 14,
      '6': '.sentinel.v1.DataKind',
      '10': 'kind'
    },
    {'1': 'content_hash', '3': 3, '4': 1, '5': 9, '10': 'contentHash'},
    {'1': 'txid', '3': 4, '4': 1, '5': 9, '10': 'txid'},
    {'1': 'finalized', '3': 5, '4': 1, '5': 8, '10': 'finalized'},
    {'1': 'height', '3': 6, '4': 1, '5': 4, '10': 'height'},
    {'1': 'summary', '3': 7, '4': 1, '5': 9, '10': 'summary'},
    {
      '1': 'anchored_unix_millis',
      '3': 8,
      '4': 1,
      '5': 3,
      '10': 'anchoredUnixMillis'
    },
  ],
};

/// Descriptor for `AnchorReceipt`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List anchorReceiptDescriptor = $convert.base64Decode(
    'Cg1BbmNob3JSZWNlaXB0EhoKCHNlcXVlbmNlGAEgASgEUghzZXF1ZW5jZRIpCgRraW5kGAIgAS'
    'gOMhUuc2VudGluZWwudjEuRGF0YUtpbmRSBGtpbmQSIQoMY29udGVudF9oYXNoGAMgASgJUgtj'
    'b250ZW50SGFzaBISCgR0eGlkGAQgASgJUgR0eGlkEhwKCWZpbmFsaXplZBgFIAEoCFIJZmluYW'
    'xpemVkEhYKBmhlaWdodBgGIAEoBFIGaGVpZ2h0EhgKB3N1bW1hcnkYByABKAlSB3N1bW1hcnkS'
    'MAoUYW5jaG9yZWRfdW5peF9taWxsaXMYCCABKANSEmFuY2hvcmVkVW5peE1pbGxpcw==');

@$core.Deprecated('Use verifyRequestDescriptor instead')
const VerifyRequest$json = {
  '1': 'VerifyRequest',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'content_hash', '3': 2, '4': 1, '5': 9, '10': 'contentHash'},
  ],
};

/// Descriptor for `VerifyRequest`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List verifyRequestDescriptor = $convert.base64Decode(
    'Cg1WZXJpZnlSZXF1ZXN0EhkKCGNoYWluX2lkGAEgASgEUgdjaGFpbklkEiEKDGNvbnRlbnRfaG'
    'FzaBgCIAEoCVILY29udGVudEhhc2g=');

@$core.Deprecated('Use verifyReplyDescriptor instead')
const VerifyReply$json = {
  '1': 'VerifyReply',
  '2': [
    {'1': 'anchored', '3': 1, '4': 1, '5': 8, '10': 'anchored'},
    {'1': 'verified', '3': 2, '4': 1, '5': 8, '10': 'verified'},
    {'1': 'height', '3': 3, '4': 1, '5': 4, '10': 'height'},
    {'1': 'state_root', '3': 4, '4': 1, '5': 9, '10': 'stateRoot'},
    {'1': 'proof_bytes', '3': 5, '4': 1, '5': 13, '10': 'proofBytes'},
  ],
};

/// Descriptor for `VerifyReply`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List verifyReplyDescriptor = $convert.base64Decode(
    'CgtWZXJpZnlSZXBseRIaCghhbmNob3JlZBgBIAEoCFIIYW5jaG9yZWQSGgoIdmVyaWZpZWQYAi'
    'ABKAhSCHZlcmlmaWVkEhYKBmhlaWdodBgDIAEoBFIGaGVpZ2h0Eh0KCnN0YXRlX3Jvb3QYBCAB'
    'KAlSCXN0YXRlUm9vdBIfCgtwcm9vZl9ieXRlcxgFIAEoDVIKcHJvb2ZCeXRlcw==');

@$core.Deprecated('Use deviceRefDescriptor instead')
const DeviceRef$json = {
  '1': 'DeviceRef',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
  ],
};

/// Descriptor for `DeviceRef`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List deviceRefDescriptor = $convert
    .base64Decode('CglEZXZpY2VSZWYSGQoIY2hhaW5faWQYASABKARSB2NoYWluSWQ=');

@$core.Deprecated('Use ledgerStatusDescriptor instead')
const LedgerStatus$json = {
  '1': 'LedgerStatus',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'device_address', '3': 2, '4': 1, '5': 9, '10': 'deviceAddress'},
    {
      '1': 'device_fingerprint',
      '3': 3,
      '4': 1,
      '5': 9,
      '10': 'deviceFingerprint'
    },
    {'1': 'height', '3': 4, '4': 1, '5': 4, '10': 'height'},
    {'1': 'records', '3': 5, '4': 1, '5': 4, '10': 'records'},
    {'1': 'mempool', '3': 6, '4': 1, '5': 4, '10': 'mempool'},
    {'1': 'state_root', '3': 7, '4': 1, '5': 9, '10': 'stateRoot'},
    {
      '1': 'kind_counts',
      '3': 8,
      '4': 3,
      '5': 11,
      '6': '.sentinel.v1.LedgerStatus.KindCountsEntry',
      '10': 'kindCounts'
    },
  ],
  '3': [LedgerStatus_KindCountsEntry$json],
};

@$core.Deprecated('Use ledgerStatusDescriptor instead')
const LedgerStatus_KindCountsEntry$json = {
  '1': 'KindCountsEntry',
  '2': [
    {'1': 'key', '3': 1, '4': 1, '5': 9, '10': 'key'},
    {'1': 'value', '3': 2, '4': 1, '5': 4, '10': 'value'},
  ],
  '7': {'7': true},
};

/// Descriptor for `LedgerStatus`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List ledgerStatusDescriptor = $convert.base64Decode(
    'CgxMZWRnZXJTdGF0dXMSGQoIY2hhaW5faWQYASABKARSB2NoYWluSWQSJQoOZGV2aWNlX2FkZH'
    'Jlc3MYAiABKAlSDWRldmljZUFkZHJlc3MSLQoSZGV2aWNlX2ZpbmdlcnByaW50GAMgASgJUhFk'
    'ZXZpY2VGaW5nZXJwcmludBIWCgZoZWlnaHQYBCABKARSBmhlaWdodBIYCgdyZWNvcmRzGAUgAS'
    'gEUgdyZWNvcmRzEhgKB21lbXBvb2wYBiABKARSB21lbXBvb2wSHQoKc3RhdGVfcm9vdBgHIAEo'
    'CVIJc3RhdGVSb290EkoKC2tpbmRfY291bnRzGAggAygLMikuc2VudGluZWwudjEuTGVkZ2VyU3'
    'RhdHVzLktpbmRDb3VudHNFbnRyeVIKa2luZENvdW50cxo9Cg9LaW5kQ291bnRzRW50cnkSEAoD'
    'a2V5GAEgASgJUgNrZXkSFAoFdmFsdWUYAiABKARSBXZhbHVlOgI4AQ==');

@$core.Deprecated('Use fleetSpecDescriptor instead')
const FleetSpec$json = {
  '1': 'FleetSpec',
  '2': [
    {'1': 'device_labels', '3': 1, '4': 3, '5': 9, '10': 'deviceLabels'},
  ],
};

/// Descriptor for `FleetSpec`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List fleetSpecDescriptor = $convert.base64Decode(
    'CglGbGVldFNwZWMSIwoNZGV2aWNlX2xhYmVscxgBIAMoCVIMZGV2aWNlTGFiZWxz');

@$core.Deprecated('Use fleetRefDescriptor instead')
const FleetRef$json = {
  '1': 'FleetRef',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
  ],
};

/// Descriptor for `FleetRef`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List fleetRefDescriptor = $convert
    .base64Decode('CghGbGVldFJlZhIZCghjaGFpbl9pZBgBIAEoBFIHY2hhaW5JZA==');

@$core.Deprecated('Use fleetNodeDescriptor instead')
const FleetNode$json = {
  '1': 'FleetNode',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'label', '3': 2, '4': 1, '5': 9, '10': 'label'},
    {'1': 'height', '3': 3, '4': 1, '5': 4, '10': 'height'},
    {'1': 'pending_anchors', '3': 4, '4': 1, '5': 13, '10': 'pendingAnchors'},
    {'1': 'identity_address', '3': 5, '4': 1, '5': 9, '10': 'identityAddress'},
    {'1': 'root', '3': 6, '4': 1, '5': 8, '10': 'root'},
  ],
};

/// Descriptor for `FleetNode`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List fleetNodeDescriptor = $convert.base64Decode(
    'CglGbGVldE5vZGUSGQoIY2hhaW5faWQYASABKARSB2NoYWluSWQSFAoFbGFiZWwYAiABKAlSBW'
    'xhYmVsEhYKBmhlaWdodBgDIAEoBFIGaGVpZ2h0EicKD3BlbmRpbmdfYW5jaG9ycxgEIAEoDVIO'
    'cGVuZGluZ0FuY2hvcnMSKQoQaWRlbnRpdHlfYWRkcmVzcxgFIAEoCVIPaWRlbnRpdHlBZGRyZX'
    'NzEhIKBHJvb3QYBiABKAhSBHJvb3Q=');

@$core.Deprecated('Use fleetDescriptor instead')
const Fleet$json = {
  '1': 'Fleet',
  '2': [
    {'1': 'built', '3': 1, '4': 1, '5': 8, '10': 'built'},
    {
      '1': 'nodes',
      '3': 2,
      '4': 3,
      '5': 11,
      '6': '.sentinel.v1.FleetNode',
      '10': 'nodes'
    },
    {'1': 'log', '3': 3, '4': 3, '5': 9, '10': 'log'},
    {
      '1': 'root_subchain_root',
      '3': 4,
      '4': 1,
      '5': 9,
      '10': 'rootSubchainRoot'
    },
  ],
};

/// Descriptor for `Fleet`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List fleetDescriptor = $convert.base64Decode(
    'CgVGbGVldBIUCgVidWlsdBgBIAEoCFIFYnVpbHQSLAoFbm9kZXMYAiADKAsyFi5zZW50aW5lbC'
    '52MS5GbGVldE5vZGVSBW5vZGVzEhAKA2xvZxgDIAMoCVIDbG9nEiwKEnJvb3Rfc3ViY2hhaW5f'
    'cm9vdBgEIAEoCVIQcm9vdFN1YmNoYWluUm9vdA==');

@$core.Deprecated('Use fleetProofDescriptor instead')
const FleetProof$json = {
  '1': 'FleetProof',
  '2': [
    {'1': 'chain_id', '3': 1, '4': 1, '5': 4, '10': 'chainId'},
    {'1': 'verified', '3': 2, '4': 1, '5': 8, '10': 'verified'},
    {'1': 'proof_bytes', '3': 3, '4': 1, '5': 13, '10': 'proofBytes'},
    {
      '1': 'root_subchain_root',
      '3': 4,
      '4': 1,
      '5': 9,
      '10': 'rootSubchainRoot'
    },
    {'1': 'identity_address', '3': 5, '4': 1, '5': 9, '10': 'identityAddress'},
  ],
};

/// Descriptor for `FleetProof`. Decode as a `google.protobuf.DescriptorProto`.
final $typed_data.Uint8List fleetProofDescriptor = $convert.base64Decode(
    'CgpGbGVldFByb29mEhkKCGNoYWluX2lkGAEgASgEUgdjaGFpbklkEhoKCHZlcmlmaWVkGAIgAS'
    'gIUgh2ZXJpZmllZBIfCgtwcm9vZl9ieXRlcxgDIAEoDVIKcHJvb2ZCeXRlcxIsChJyb290X3N1'
    'YmNoYWluX3Jvb3QYBCABKAlSEHJvb3RTdWJjaGFpblJvb3QSKQoQaWRlbnRpdHlfYWRkcmVzcx'
    'gFIAEoCVIPaWRlbnRpdHlBZGRyZXNz');
