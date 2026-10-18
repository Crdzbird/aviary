import 'dart:io';

import 'package:aviary_dart/aviary.dart';
import 'package:pub_compare/hashing.dart';
import 'package:pub_compare/ledger.dart';
import 'package:pub_compare/measure.dart';
import 'package:pub_compare/post_quantum.dart';
import 'package:pub_compare/signing.dart';

/// Prints the comparison as markdown tables, best of five interleaved
/// rounds per contender.
Future<void> main() async {
  final bc = Aviary.instance;
  print(
    'aviary ${bc.version()} on Dart ${Platform.version.split(' ').first}, '
    '${Platform.operatingSystem} ${Platform.numberOfProcessors} cores\n',
  );
  final sections = <String, Future<Map<String, double>> Function()>{
    'Hashing': () => bestOf(hashing(bc)),
    'Ed25519': () async => bestOf(await signing(bc)),
    'Post quantum signatures': () => bestOf(postQuantum(bc)),
    'A block of 1,000 transfers': () async => ledger(bc),
  };
  for (final s in sections.entries) {
    print('### ${s.key}\n');
    print('| Work | Library | Rate | Per operation |\n|---|---|---|---|');
    (await s.value()).forEach((k, v) => print(row(k, v)));
    print('');
  }
  if (sink == -1) print('');
}
