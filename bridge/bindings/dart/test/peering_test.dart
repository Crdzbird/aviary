import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('peer records and address books', () {
    final bc = Aviary.instance;
    final alice = bc.generateSecureIdentity();
    final bob = bc.generateSecureIdentity();
    final ra = bc.signPeerRecord(1, '10.0.0.1', 7001, alice);
    final rb = bc.signPeerRecord(2, '10.0.0.2', 7002, bob);
    check(bc.verifyPeerRecord(ra) && bc.verifyPeerRecord(rb), 'records verify');
    final info = bc.peerRecordInfo(rb);
    check(
      info.id == 2 && info.host == '10.0.0.2' && info.port == 7002,
      'fields read back',
    );
    check(info.sigPublic.equalsBytes(bob.sig.public), 'signing key carried');
    final forged = Uint8List.fromList(rb)..[rb.length - 1] ^= 1;
    check(!bc.verifyPeerRecord(forged), 'forged record refused');
    final mine = bc.newAddressBook()..add(ra);
    final theirs = bc.newAddressBook()
      ..add(rb)
      ..add(ra);
    check(mine.merge(theirs.encode()) == 1, 'learns the one new peer');
    check(mine.length == 2 && mine.contains(2), 'both known');
    check(mine.record(2)!.equalsBytes(rb), 'record by id');
    check(mine.record(9) == null, 'unknown id is null');
    check(mine.encode().equalsBytes(theirs.encode()), 'canonical books');
    check(mine.records().length == 2, 'all records');
    final evil = bc.newAddressBook()..add(forged);
    final fresh = bc.newAddressBook();
    check(fresh.merge(evil.encode()) == 0, 'merge verifies signatures');
    check(failsAnyhow(() => fresh.merge(Uint8List(3))), 'malformed book');
    check(failsAnyhow(() => bc.newAddressBook(Uint8List(3))), 'bad decode');
    final back = bc.newAddressBook(theirs.encode());
    check(back.length == 2, 'decoded book');
    for (final b in [mine, theirs, evil, fresh, back]) {
      b.free();
    }
    print('aviary dart peering smoke: OK');
  });
}
