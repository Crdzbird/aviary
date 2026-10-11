part of '../../aviary.dart';

/// Events and metrics of an assembled chain: every [Chain.submit] reports a
/// [BlockFinalized] or a [BlockRejected], and the metrics count them.
extension ChainObservability on Chain {
  /// Events buffered since the last call. Empty while [eventStream] is live.
  List<ChainEvent> events() => _chainEventsFromJsonArray(
    utf8.decode(_buf('aviary_chain_drain_events')),
  );

  /// Every event as it happens, on the microtask after the call that caused
  /// it. Listening redirects events from the buffer to this broadcast stream
  /// until the chain is freed, which closes it.
  Stream<ChainEvent> get eventStream =>
      (_nodeSinks[_h.address] ??= _NodeEventSink.attach(
        _bc,
        _h,
        'aviary_chain_set_event_hook',
      )).controller.stream;

  /// A Prometheus format metrics snapshot: chain height, blocks finalized
  /// and rejected, transactions applied.
  String metrics() => utf8.decode(_buf('aviary_chain_metrics'));

  /// [metrics] as values keyed by sample name.
  Map<String, double> metricValues() => _parseMetrics(metrics());
}

/// Events and metrics of a networked node: a [BlockFinalized] per committed
/// height, and counters for gossip received, relayed and dropped, timeouts
/// fired and peers banned.
extension NetNodeObservability on NetNode {
  /// Events buffered since the last call. Empty while [eventStream] is live.
  List<ChainEvent> events() => _chainEventsFromJsonArray(
    utf8.decode(_buf('aviary_net_node_drain_events')),
  );

  /// Every event as it happens, on the microtask after the input that caused
  /// it. Listening redirects events from the buffer to this broadcast stream
  /// until the node is freed, which closes it.
  Stream<ChainEvent> get eventStream =>
      (_nodeSinks[_h.address] ??= _NodeEventSink.attach(
        _bc,
        _h,
        'aviary_net_node_set_event_hook',
      )).controller.stream;

  /// A Prometheus format metrics snapshot.
  String metrics() => utf8.decode(_buf('aviary_net_node_metrics'));

  /// [metrics] as values keyed by sample name.
  Map<String, double> metricValues() => _parseMetrics(metrics());
}
