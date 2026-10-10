import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

Future<void> main() async {
  final bc = Aviary.instance;
  releaseTwice(bc);
  typedStatus(bc);
  scopes(bc);
  await workerIsolate(bc);
  print('aviary dart lifecycle smoke: OK');
}

void releaseTwice(Aviary bc) {
  final node = bc.createNode(1);
  expect(!node.isReleased, 'fresh node is live');
  node.dispose();
  node.dispose();
  expect(node.isReleased, 'second dispose is a no-op');
  final store = bc.createKeyStore('pw');
  store.dispose();
  store.dispose();
  for (var i = 0; i < 200; i++) {
    bc.createNode(2).newAccount();
  }
}

void typedStatus(Aviary bc) {
  final node = bc.createNode(3);
  final a = node.fundAccount(10);
  node.commit();
  var status = AviaryStatus.ok;
  try {
    node.credit(a.address, 1);
  } on AviaryException catch (e) {
    status = e.status;
    expect(e.toString().contains('genesisSealed'), 'status in toString');
  }
  expect(status == AviaryStatus.genesisSealed, 'credit after block is typed');
  expect(AviaryStatus.fromCode(99) == AviaryStatus.failure, 'unknown maps');
  expect(
    const AviaryException('x').status == AviaryStatus.failure,
    'default status',
  );
  node.dispose();
}

void scopes(Aviary bc) {
  late Node leaked;
  final height = bc.withNode(4, (node) {
    leaked = node;
    node.fundAccount(5);
    return node.commit();
  });
  expect(height == 1 && leaked.isReleased, 'withNode frees after body');
  final v = bc.newValidator().treeValidator(1);
  final ids = bc.withTree(
    TreeNode(
      chainId: 1,
      validators: [v],
    ).withChild(TreeNode(chainId: 2, validators: [v])),
    (tree) => tree.chainIds(),
  );
  expect(ids.length == 2, 'withTree returns the body value');
}

Future<void> workerIsolate(Aviary bc) async {
  final shares = bc.createThresholdAccount(2, 2);
  final sealed = await bc.sealShareAsync(shares[0].bytes, 'pw');
  final opened = await bc.unsealShareAsync(sealed, 'pw');
  expect(opened.equalsBytes(shares[0].bytes), 'share round trips off isolate');
  final node = bc.createNode(5);
  final a = node.newAccount();
  final env = await bc.sealAccountPassphraseAsync(a.publicKey, a.secret, 'pw');
  final back = await bc.openAccountPassphraseAsync(env, 'pw');
  expect(back.secret.equalsBytes(a.secret), 'account round trips off isolate');
  var wrong = false;
  try {
    await bc.unsealShareAsync(sealed, 'nope');
  } on AviaryException catch (_) {
    wrong = true;
  }
  expect(wrong, 'wrong passphrase surfaces as AviaryException');
  node.dispose();
}
