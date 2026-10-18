import 'package:flutter_test/flutter_test.dart';
import 'package:aviary/aviary.dart';
import 'package:aviary_example/main.dart';

Future<void> _open(WidgetTester tester, String tab) async {
  await tester.tap(find.text(tab));
  await tester.pumpAndSettle();
}

Future<void> _press(WidgetTester tester, String label) async {
  await tester.tap(find.text(label));
  await tester.pumpAndSettle();
}

void main() {
  setUpAll(Aviary.ensureInitialized);

  testWidgets('every tab works', (tester) async {
    await tester.pumpWidget(const ExampleApp());
    await _press(tester, 'Run');
    expect(find.textContaining('balance proof verifies: true'), findsOneWidget);

    await _open(tester, 'Events');
    await _press(tester, 'Send 10');
    await _press(tester, 'Commit block');
    expect(find.textContaining('block 1 finalized'), findsOneWidget);
    expect(find.textContaining('Bob holds 10'), findsOneWidget);

    await _open(tester, 'Records');
    await _press(tester, 'Register');
    await _press(tester, 'Service');
    await _press(tester, 'Hand to lab');
    expect(find.textContaining('owned by the lab'), findsOneWidget);
    await _press(tester, 'Verify');
    expect(find.textContaining('state root: true'), findsOneWidget);

    await _open(tester, 'Network');
    await _press(tester, 'Start consensus');
    expect(find.text('Validator 3 at height 1'), findsOneWidget);
    await _press(tester, 'Next height');
    expect(find.text('Validator 0 at height 2'), findsOneWidget);
    expect(find.text('Every validator holds the same tip.'), findsOneWidget);
  });
}
