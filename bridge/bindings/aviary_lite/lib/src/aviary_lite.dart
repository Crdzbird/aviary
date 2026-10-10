import 'package:aviary_dart/aviary.dart';

/// Entry point of the self contained Flutter package. The engine ships
/// inside the plugin, so there is nothing to configure.
final class AviaryLite {
  const AviaryLite._();

  /// Warms the bundled engine on a background isolate and prepares
  /// [instance]. Call it once before the first ledger call, typically in
  /// main after the widgets binding is initialized.
  static Future<void> ensureInitialized() => Aviary.ensureInitialized();

  /// The process wide engine, loaded synchronously on first access.
  static Aviary get instance => Aviary.instance;

  /// Whether [instance] is ready without any load work.
  static bool get isInitialized => Aviary.isInitialized;
}
