import 'package:flutter/material.dart';
import 'package:blockchainer_lite/blockchainer_lite.dart';

import 'ledger_page.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Blockchainer.ensureInitialized();
  runApp(const ExampleApp());
}

/// The example application.
final class ExampleApp extends StatelessWidget {
  const ExampleApp({super.key});

  @override
  Widget build(BuildContext context) =>
      const MaterialApp(title: 'blockchainer_lite example', home: LedgerPage());
}
