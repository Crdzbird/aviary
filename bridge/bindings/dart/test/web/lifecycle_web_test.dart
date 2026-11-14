@TestOn('browser')
library;

import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/web_engine.dart';

void main() {
  test('the engine is not there before it loads', () {
    expect(Aviary.isInitialized, isFalse);
    expect(Aviary.loadedFrom, isNull);
    expect(() => Aviary.instance, throwsA(isA<AviaryException>()));
    expect(Aviary.load, throwsA(isA<AviaryException>()));
    expect(() => Aviary.open('x'), throwsA(isA<AviaryException>()));
  });

  test('a failed load reports false and is retried', () async {
    pointEngineAt(Uri.base.resolve('missing/aviary.mjs').toString());
    expect(await Aviary.preload(), isFalse);
    await expectLater(Aviary.ensureInitialized(), throwsA(anything));
    expect(Aviary.isInitialized, isFalse);
    pointEngineAt(servedEngineUrl);
    expect(await Aviary.preload(), isTrue);
  });

  test('loading is idempotent and every entry gives the one engine', () async {
    await Future.wait([Aviary.ensureInitialized(), Aviary.init()]);
    final bc = Aviary.instance;
    expect(identical(Aviary.load(), bc), isTrue);
    expect(identical(Aviary.open('ignored'), bc), isTrue);
    expect(bc.hash(Uint8List(0)), hasLength(32));
  });
}
