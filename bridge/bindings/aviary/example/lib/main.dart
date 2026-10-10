import 'package:flutter/material.dart';
import 'package:aviary/aviary.dart';

import 'ledger_page.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Aviary.ensureInitialized();
  runApp(const ExampleApp());
}

/// The example application.
final class ExampleApp extends StatelessWidget {
  const ExampleApp({super.key});

  @override
  Widget build(BuildContext context) =>
      const MaterialApp(title: 'aviary example', home: LedgerPage());
}
