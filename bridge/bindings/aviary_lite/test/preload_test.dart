import 'package:aviary_lite/aviary_lite.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('synchronous API works with no preload', () {
    final node = Aviary.instance.createNode(7);
    final a = node.newAccount();
    node.credit(a.address, 1000);
    expect(node.produceBlock(1), 1);
    node.dispose();
  });

  test('preload warms a background isolate', () async {
    expect(await Aviary.preload(), isTrue);
  });
}
