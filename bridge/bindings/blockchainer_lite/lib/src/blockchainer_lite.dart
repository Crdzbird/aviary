import 'package:blockchainer_dart/blockchainer.dart';

/// Entry point of the self contained Flutter package. The engine ships
/// inside the plugin, so there is nothing to configure.
final class BlockchainerLite {
  const BlockchainerLite._();

  /// Warms the bundled engine on a background isolate and prepares
  /// [instance]. Call it once before the first ledger call, typically in
  /// main after the widgets binding is initialized.
  static Future<void> ensureInitialized() => Blockchainer.ensureInitialized();

  /// The process wide engine, loaded synchronously on first access.
  static Blockchainer get instance => Blockchainer.instance;

  /// Whether [instance] is ready without any load work.
  static bool get isInitialized => Blockchainer.isInitialized;
}
