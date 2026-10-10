import 'package:blockchainer/blockchainer.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('synchronous API works with no preload', () {
    final node = Blockchainer.instance.createNode(7);
    final a = node.newAccount();
    node.credit(a.address, 1000);
    expect(node.produceBlock(1), 1);
    node.dispose();
  });

  test('preload warms a background isolate', () async {
    expect(await Blockchainer.preload(), isTrue);
  });
}
