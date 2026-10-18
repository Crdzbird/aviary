part of '../../aviary.dart';

/// Multi chain address derivation and EVM calldata helpers.
/// {@category Interop}
extension AviaryInterop on Aviary {
  String _addr(String fn, Uint8List secret) => _scoped(
    [
      secret,
    ],
    (p) => _takeStr(lib.fn<_BufOfPtrC>(fn).asFunction<_BufOfPtrD>()(p[0]), fn),
  );

  /// The Ethereum address of a 32 byte secp256k1 secret.
  String ethAddress(Uint8List secret32) =>
      _addr('aviary_eth_address', secret32);

  /// The Bitcoin address of a 32 byte secp256k1 secret.
  String btcAddress(Uint8List secret32) =>
      _addr('aviary_btc_address', secret32);

  /// The Solana address of a 64 byte Ed25519 secret.
  String solAddress(Uint8List edSecret64) =>
      _addr('aviary_sol_address', edSecret64);

  /// ABI calldata for an ERC-20 transfer to a 20 byte address.
  Uint8List ethErc20TransferCalldata(Uint8List to20, int amount) => _scoped(
    [to20],
    (p) => _take(
      lib.fn<_Erc20C>('aviary_eth_erc20_transfer').asFunction<_Erc20D>()(
        p[0],
        amount,
      ),
      'ethErc20TransferCalldata',
    ),
  );

  /// The 4 byte ABI selector of a function signature.
  Uint8List ethSelector(String signature) => _withUtf8(
    signature,
    (s) => _take(
      lib.fn<_SelectorC>('aviary_eth_selector').asFunction<_SelectorD>()(s),
      'ethSelector',
    ),
  );

  /// Signs an EVM legacy transaction with a 32 byte secp256k1 key. [to] null
  /// creates a contract; [data] null is a plain value transfer.
  Uint8List ethSignLegacy({
    required int nonce,
    required int gasPrice,
    required int gasLimit,
    required int chainId,
    required Uint8List secret32,
    Uint8List? to,
    int value = 0,
    Uint8List? data,
  }) {
    final hasTo = to != null && to.length == 20 ? 1 : 0;
    return _scoped(
      [to ?? Uint8List(0), data ?? Uint8List(0), secret32],
      (p) => _take(
        lib
            .fn<_SignLegacyC>('aviary_eth_sign_legacy')
            .asFunction<_SignLegacyD>()(
          nonce,
          gasPrice,
          gasLimit,
          p[0],
          hasTo,
          value,
          p[1],
          data?.length ?? 0,
          chainId,
          p[2],
        ),
        'ethSignLegacy',
      ),
    );
  }
}
