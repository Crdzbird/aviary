part of '../../../aviary.dart';

final class _UnknownEvent extends ChainEvent implements UnknownEvent {
  const _UnknownEvent(this.type, this.json);

  @override
  final String type;

  @override
  final Map<String, Object?> json;

  @override
  String toString() => 'UnknownEvent($type)';
}
