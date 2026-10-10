part of '../../../aviary.dart';

/// An event kind this version of the binding does not model. [json] holds
/// the fields as the engine sent them.
abstract final class UnknownEvent extends ChainEvent {
  /// Wraps an unmodelled event.
  const factory UnknownEvent(String type, Map<String, Object?> json) =
      _UnknownEvent;

  /// The raw fields.
  Map<String, Object?> get json;
}
