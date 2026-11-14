/// The Aviary ledger in a browser: the stateless part of the API over the
/// WebAssembly engine, which is what `package:aviary_dart/aviary.dart`
/// exports when compiled to JavaScript or WebAssembly. Hashing, header
/// hashes, and proof and certificate checks run the same C++ core as every
/// other binding, so they return byte identical results.
library;

import 'dart:convert';
import 'dart:js_interop';
import 'dart:js_interop_unsafe';
import 'dart:typed_data';

import 'common.dart';

export 'common.dart' hide childRecordOf;

part 'web/aviary.dart';
part 'web/canonical.dart';
part 'web/child_proofs.dart';
part 'web/dynamic_subchains.dart';
part 'web/engine.dart';
part 'web/engine_load.dart';
part 'web/proofs.dart';
part 'web/subchain_proofs.dart';
part 'web/words.dart';
