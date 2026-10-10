import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

/// A deterministic in memory mesh: relays are queued, timeouts fire only
/// when the queue is empty, earliest round first.
final class Mesh {
  Mesh(this.nodes);

  final List<NetNode> nodes;
  final List<(int from, int to, Uint8List payload)> queue = [];
  final List<(int round, int step, int node)> timeouts = [];

  void apply(int node, Outbound out) {
    for (final r in out.relays) {
      queue.add((node, r.to, r.message));
    }
    for (final t in out.timeouts) {
      timeouts.add((t.round, t.step.code, node));
    }
  }

  void run(bool Function() done, {int budget = 20000}) {
    for (var step = 0; step < budget; step++) {
      if (done()) return;
      if (queue.isNotEmpty) {
        final (from, to, payload) = queue.removeAt(0);
        apply(to, nodes[to].onGossip(from, payload));
        continue;
      }
      if (timeouts.isEmpty) return;
      timeouts.sort((a, b) => a.$1 != b.$1 ? a.$1 - b.$1 : a.$2 - b.$2);
      final (round, step, node) = timeouts.removeAt(0);
      final request = (step: ConsensusStep.fromCode(step), round: round);
      apply(node, nodes[node].onTimeout(request));
    }
  }
}
