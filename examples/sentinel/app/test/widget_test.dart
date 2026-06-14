// Smoke test: the app renders a loading state before reaching the server.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:sentinel_app/main.dart';

void main() {
  testWidgets('shows a progress indicator while connecting', (tester) async {
    await tester.pumpWidget(const SentinelApp());
    expect(find.byType(CircularProgressIndicator), findsOneWidget);
  });
}
