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

import 'package:protobuf/protobuf.dart' as $pb;

class DataKind extends $pb.ProtobufEnum {
  static const DataKind DATA_UNSPECIFIED =
      DataKind._(0, _omitEnumNames ? '' : 'DATA_UNSPECIFIED');
  static const DataKind PHOTO = DataKind._(1, _omitEnumNames ? '' : 'PHOTO');
  static const DataKind VIDEO = DataKind._(2, _omitEnumNames ? '' : 'VIDEO');
  static const DataKind LOCATION =
      DataKind._(3, _omitEnumNames ? '' : 'LOCATION');
  static const DataKind SENSOR = DataKind._(4, _omitEnumNames ? '' : 'SENSOR');

  static const $core.List<DataKind> values = <DataKind>[
    DATA_UNSPECIFIED,
    PHOTO,
    VIDEO,
    LOCATION,
    SENSOR,
  ];

  static final $core.List<DataKind?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 4);
  static DataKind? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const DataKind._(super.value, super.name);
}

const $core.bool _omitEnumNames =
    $core.bool.fromEnvironment('protobuf.omit_enum_names');
