import 'package:flutter/material.dart';
import 'package:aviary/aviary.dart';

import 'home_page.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Aviary.ensureInitialized();
  runApp(const ExampleApp());
}

/// The example application: four tabs, one per area of the library.
final class ExampleApp extends StatelessWidget {
  const ExampleApp({super.key});

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'aviary example',
    theme: ThemeData(colorSchemeSeed: Colors.indigo),
    home: const HomePage(),
  );
}
