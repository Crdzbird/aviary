part of '../../aviary.dart';

final Map<String, NativeFinalizer> _finalizers = {};

NativeFinalizer _finalizerFor(DynamicLibrary lib, String symbol) =>
    _finalizers['${lib.handle.address}:$symbol'] ??= NativeFinalizer(
      lib.lookup<NativeFunction<Void Function(Pointer<Void>)>>(symbol),
    );

/// Owns one native handle. The handle is freed by [free] or [dispose] on the
/// public classes, and otherwise when the Dart object is collected, so a
/// forgotten release no longer leaks native memory. Releasing twice is a
/// no-op.
abstract class _NativeHandle implements Finalizable {
  _NativeHandle(this._bc, this._h, this._freeSymbol) {
    _finalizerFor(_bc.lib, _freeSymbol).attach(this, _h, detach: this);
  }

  final Aviary _bc;
  final Pointer<Void> _h;
  final String _freeSymbol;
  bool _released = false;

  /// Whether the native handle has already been released.
  bool get isReleased => _released;

  void _release() {
    if (_released) return;
    _released = true;
    _finalizerFor(_bc.lib, _freeSymbol).detach(this);
    _bc.lib.lookupFunction<_HandleFreeC, _HandleFreeD>(_freeSymbol)(_h);
  }
}
