import 'dart:typed_data';

/// Hashes whose 64 bit fields need more than 32 bits, made by the native
/// engine. The browser passes such fields as two halves; these pin that
/// they arrive whole. The VM test pinned_web_vectors_test.dart recomputes
/// them natively.
const wideHeaderHash =
    'b7e181dd4ec31e76125a7ead84e3282832a45711f360f00f1ee0405bcab76a36';

/// The txid of [wideTxid] with a chain id just above 2 to the 52.
const wideTxidHash =
    '35c5ebd4991ac221cc915cba567a86114a77337b81140d481ea3d3991cc65f73';

/// The header [wideHeaderHash] hashes: zero roots and no consensus data.
Uint8List wideHeader(
  Uint8List Function(
    int version,
    int chainId,
    int height,
    Uint8List parentHash,
    int timestamp,
    Uint8List txRoot,
    Uint8List stateRoot,
    Uint8List consensus,
  )
  headerHash,
) => headerHash(
  1,
  1099511627783,
  8589934634,
  Uint8List(32),
  1731580000000,
  Uint8List(32),
  Uint8List(32),
  Uint8List(0),
);

/// The transaction [wideTxidHash] identifies.
Uint8List wideTxid(
  Uint8List Function(
    int version,
    int chainId,
    Uint8List body,
    Uint8List author,
    Uint8List signature,
  )
  txid,
) => txid(1, 4503599627370505, Uint8List(3), Uint8List(32), Uint8List(64));
