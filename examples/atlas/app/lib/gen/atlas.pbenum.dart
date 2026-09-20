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

import 'package:protobuf/protobuf.dart' as $pb;

class EventType extends $pb.ProtobufEnum {
  static const EventType EVENT_UNSPECIFIED =
      EventType._(0, _omitEnumNames ? '' : 'EVENT_UNSPECIFIED');
  static const EventType TX_ADMITTED =
      EventType._(1, _omitEnumNames ? '' : 'TX_ADMITTED');
  static const EventType BLOCK_FINALIZED =
      EventType._(2, _omitEnumNames ? '' : 'BLOCK_FINALIZED');
  static const EventType TX_REMOVED =
      EventType._(3, _omitEnumNames ? '' : 'TX_REMOVED');

  static const $core.List<EventType> values = <EventType>[
    EVENT_UNSPECIFIED,
    TX_ADMITTED,
    BLOCK_FINALIZED,
    TX_REMOVED,
  ];

  static final $core.List<EventType?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 3);
  static EventType? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const EventType._(super.value, super.name);
}

class CouncilOp extends $pb.ProtobufEnum {
  static const CouncilOp COUNCIL_PRODUCE =
      CouncilOp._(0, _omitEnumNames ? '' : 'COUNCIL_PRODUCE');
  static const CouncilOp COUNCIL_ADD_VALIDATOR =
      CouncilOp._(1, _omitEnumNames ? '' : 'COUNCIL_ADD_VALIDATOR');
  static const CouncilOp COUNCIL_AUDITOR_SYNC =
      CouncilOp._(2, _omitEnumNames ? '' : 'COUNCIL_AUDITOR_SYNC');

  static const $core.List<CouncilOp> values = <CouncilOp>[
    COUNCIL_PRODUCE,
    COUNCIL_ADD_VALIDATOR,
    COUNCIL_AUDITOR_SYNC,
  ];

  static final $core.List<CouncilOp?> _byValue =
      $pb.ProtobufEnum.$_initByValueList(values, 2);
  static CouncilOp? valueOf($core.int value) =>
      value < 0 || value >= _byValue.length ? null : _byValue[value];

  const CouncilOp._(super.value, super.name);
}

const $core.bool _omitEnumNames =
    $core.bool.fromEnvironment('protobuf.omit_enum_names');
