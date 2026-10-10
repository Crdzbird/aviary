import 'package:aviary_dart/aviary.dart';

/// Entry point of the Flutter package that shares its crypto engine with the
/// cipherbird package. One native engine serves both libraries.
final class AviaryShared {
  const AviaryShared._();

  /// Warms the engine on a background isolate and prepares [instance]. Call
  /// it once before the first ledger call, typically in main after the
  /// widgets binding is initialized.
  static Future<void> ensureInitialized() => Aviary.ensureInitialized();

  /// The process wide engine, loaded synchronously on first access.
  static Aviary get instance => Aviary.instance;

  /// Whether [instance] is ready without any load work.
  static bool get isInitialized => Aviary.isInitialized;
}
