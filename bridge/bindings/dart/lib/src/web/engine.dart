part of '../aviary_web.dart';

/// One instantiated WebAssembly engine: calls into its exports and moves
/// bytes in and out of its memory. Every buffer it allocates is freed
/// before the call that allocated it returns.
final class _Engine {
  _Engine(this._module);

  final JSObject _module;

  /// A fresh view of the engine's memory; growth replaces the old one.
  Uint8List get _heap => (_module['HEAPU8']! as JSUint8Array).toDart;

  /// Calls the export [name] with integer [args]; 0 for a void result.
  int call(String name, List<int> args) {
    final r = _module.callMethodVarArgs<JSAny?>('_$name'.toJS, [
      for (final a in args) a.toJS,
    ]);
    if (r.isA<JSNumber>()) return (r! as JSNumber).toDartInt;
    return 0;
  }

  int _alloc(int size) {
    final p = call('malloc', [size == 0 ? 1 : size]);
    if (p == 0) throw const AviaryException('out of memory');
    return p;
  }

  /// Copies [inputs] into engine memory, runs [body] with their addresses
  /// and frees them again.
  T scoped<T>(List<Uint8List> inputs, T Function(List<int> at) body) {
    final at = <int>[];
    try {
      for (final input in inputs) {
        final p = _alloc(input.length);
        at.add(p);
        _heap.setAll(p, input);
      }
      return body(at);
    } finally {
      for (final p in at) {
        call('free', [p]);
      }
    }
  }

  /// Calls the export [name], which returns an AviaryBufferResult, and
  /// returns a copy of the buffer or throws the engine's error.
  Uint8List take(String name, List<int> args, String op) {
    final slot = _alloc(12);
    try {
      call(name, [slot, ...args]);
      final heap = _heap;
      final data = _word(heap, slot);
      final error = _word(heap, slot + 8);
      if (data == 0) throw _error(error, op);
      final out = Uint8List.fromList(
        heap.sublist(data, data + _word(heap, slot + 4)),
      );
      call('aviary_buffer_free', [slot]);
      return out;
    } finally {
      call('free', [slot]);
    }
  }

  AviaryException _error(int error, String op) {
    if (error == 0) return AviaryException('$op failed');
    final message = _string(error);
    call('aviary_str_free', [error]);
    return AviaryException(message);
  }

  /// The static string the export [name] returns.
  String cString(String name) => _string(call(name, const []));

  String _string(int at) {
    final heap = _heap;
    var end = at;
    while (heap[end] != 0) {
      end++;
    }
    return utf8.decode(heap.sublist(at, end));
  }
}
