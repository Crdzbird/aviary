import 'dart:typed_data';

/// Keeps results observable so the compiler cannot drop the measured work.
int sink = 0;

/// Folds the first byte of [bytes] into [sink].
void consume(List<int> bytes) => sink ^= bytes.isEmpty ? 0 : bytes[0];

/// Calls per second of [body] over [window], after a [warmup].
double rate(
  void Function(int i) body, {
  Duration warmup = const Duration(milliseconds: 100),
  Duration window = const Duration(milliseconds: 300),
}) {
  var i = 0;
  final w = Stopwatch()..start();
  while (w.elapsed < warmup) {
    body(i++);
  }
  final sw = Stopwatch()..start();
  var n = 0;
  while (sw.elapsed < window) {
    body(i++);
    n++;
  }
  return n / (sw.elapsedMicroseconds / 1e6);
}

/// [rate] for an asynchronous [body]; every call is awaited.
Future<double> rateAsync(
  Future<void> Function(int i) body, {
  Duration warmup = const Duration(milliseconds: 100),
  Duration window = const Duration(milliseconds: 300),
}) async {
  var i = 0;
  final w = Stopwatch()..start();
  while (w.elapsed < warmup) {
    await body(i++);
  }
  final sw = Stopwatch()..start();
  var n = 0;
  while (sw.elapsed < window) {
    await body(i++);
    n++;
  }
  return n / (sw.elapsedMicroseconds / 1e6);
}

/// The best rate of each contender over [rounds] interleaved rounds, so a
/// burst of load elsewhere on the machine costs one round, not the result.
Future<Map<String, double>> bestOf(
  Map<String, Future<double> Function()> runs, {
  int rounds = 5,
}) async {
  final best = {for (final k in runs.keys) k: 0.0};
  for (var r = 0; r < rounds; r++) {
    for (final e in runs.entries) {
      final v = await e.value();
      if (v > best[e.key]!) best[e.key] = v;
    }
  }
  return best;
}

/// Seconds taken by [body].
double seconds(void Function() body) {
  final sw = Stopwatch()..start();
  body();
  return sw.elapsedMicroseconds / 1e6;
}

/// A markdown row for a [bestOf] key of the form `what|library`.
String row(String key, double perSecond) {
  final [what, library] = key.split('|');
  final us = 1e6 / perSecond;
  final per = us >= 1000
      ? '${(us / 1000).toStringAsFixed(2)} ms'
      : '${us.toStringAsFixed(us < 10 ? 2 : 1)} us';
  return '| $what | $library | ${_group(perSecond)} /s | $per |';
}

/// [n] deterministic pseudo random bytes from [seed].
Uint8List bytesOf(int n, int seed) {
  final out = Uint8List(n);
  var s = seed * 2654435761 + 1;
  for (var i = 0; i < n; i++) {
    s = (s * 1103515245 + 12345) & 0x7FFFFFFF;
    out[i] = s >> 16;
  }
  return out;
}

String _group(double v) => v.round().toString().replaceAllMapped(
  RegExp(r'\B(?=(\d{3})+(?!\d))'),
  (_) => ',',
);
