part of '../../aviary.dart';

/// A deterministic in-memory mesh of [NetNode]s for tests, simulations and
/// single process networks: relays are delivered in emission order, a timeout
/// fires only when none is pending, so the same inputs always take one path.
final class PeerMesh {
  /// Wraps [nodes]; a node's index is its mesh id and must match its `me`.
  PeerMesh(this.nodes);

  /// The nodes, indexed by mesh id.
  final List<NetNode> nodes;
  final List<(int from, int to, Uint8List payload)> _queue = [];

  /// Armed timeouts packed as round << 24 | step << 16 | node, so a plain
  /// sort orders them by round, then step, then node.
  final List<int> _timeouts = [];

  /// Makes [a] and [b] neighbours in both directions.
  void link(int a, int b) {
    nodes[a].addPeer(b);
    nodes[b].addPeer(a);
  }

  /// Links every pair of nodes.
  void linkAll() {
    for (var a = 0; a < nodes.length; a++) {
      for (var b = a + 1; b < nodes.length; b++) {
        link(a, b);
      }
    }
  }

  /// Queues what [node] emitted.
  void apply(int node, Outbound out) {
    for (final r in out.relays) {
      _queue.add((node, r.to, r.message));
    }
    for (final t in out.timeouts) {
      _timeouts.add(t.round << 24 | t.step.code << 16 | node);
    }
  }

  /// Starts every node.
  void start() => _each((n) => n.start());

  /// Rolls every node into the next height. Throws if one has not committed.
  void advance() => _each((n) => n.advance());

  /// Delivers the next relay; false when none is pending.
  bool deliver() {
    if (_queue.isEmpty) return false;
    final (from, to, payload) = _queue.removeAt(0);
    apply(to, nodes[to].onGossip(from, payload));
    return true;
  }

  /// Delivers one relay, or fires the earliest timeout when none is pending.
  /// Returns false when nothing was outstanding.
  bool step() {
    if (deliver()) return true;
    if (_timeouts.isEmpty) return false;
    _timeouts.sort();
    final key = _timeouts.removeAt(0);
    final node = key & 0xFFFF;
    final step = ConsensusStep.fromCode(key >> 16 & 0xFF);
    apply(node, nodes[node].onTimeout((step: step, round: key >> 24)));
    return true;
  }

  /// Steps until [until] holds or [budget] steps passed; returns [until].
  bool run(bool Function() until, {int budget = 20000}) {
    for (var i = 0; i < budget && !until(); i++) {
      if (!step()) break;
    }
    return until();
  }

  /// Runs until every node in [validators] (all by default) has committed,
  /// then delivers the remaining relays so observers catch up; armed timeouts
  /// are left alone. Returns whether the commit happened within [budget].
  bool settle({Iterable<int>? validators, int budget = 20000}) {
    final v = validators ?? List.generate(nodes.length, (i) => i);
    final ok = run(() => v.every((i) => nodes[i].committed), budget: budget);
    while (deliver()) {}
    return ok;
  }

  /// Frees every node.
  void free() {
    for (final n in nodes) {
      n.free();
    }
  }

  void _each(Outbound Function(NetNode) f) {
    for (var i = 0; i < nodes.length; i++) {
      apply(i, f(nodes[i]));
    }
  }
}
