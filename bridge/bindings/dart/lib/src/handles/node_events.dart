part of '../../aviary.dart';

/// Typed chain events, pulled or pushed.
extension NodeEvents on Node {
  /// Chain events buffered since the last call, decoded. Returns nothing
  /// while [eventStream] is live, because events then go to the stream.
  List<ChainEvent> events() => _chainEventsFromJsonArray(drainEvents());

  /// Every chain event as it happens. The engine hands each event over
  /// synchronously inside the node call that caused it; listeners run on the
  /// next microtask, after that call has returned, so a listener may call the
  /// node again. Listening redirects events from the buffer behind [events]
  /// to this broadcast stream until the node is disposed, which closes it.
  Stream<ChainEvent> get eventStream =>
      (_nodeSinks[_h.address] ??= _NodeEventSink.attach(_bc, _h))
          .controller
          .stream;
}
