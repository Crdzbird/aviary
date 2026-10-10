part of '../../../aviary.dart';

/// One event published by a node's event bus. Switch over the subtypes
/// ([BlockFinalized], [BlockRejected], [TxAdmitted], [TxRemoved],
/// [UnknownEvent]) exhaustively; [UnknownEvent] keeps a newer engine's event
/// kinds readable.
sealed class ChainEvent {
  const ChainEvent();

  /// The event kind as the engine names it, such as block_finalized.
  String get type;

  /// Decodes one JSON object from the engine's event stream.
  factory ChainEvent.fromJson(Map<String, Object?> json) =>
      _chainEventFromJson(json);
}
