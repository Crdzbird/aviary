part of '../../blockchainer.dart';

/// Multi chain address derivation and EVM calldata helpers.
extension BlockchainerInterop on Blockchainer {
  String _addr(String fn, Uint8List secret) => _scoped(
        [secret],
        (p) =>
            _takeStr(lib.lookupFunction<_BufOfPtrC, _BufOfPtrD>(fn)(p[0]), fn),
      );

  /// The Ethereum address of a 32 byte secp256k1 secret.
  String ethAddress(Uint8List secret32) =>
      _addr('blockchainer_eth_address', secret32);

  /// The Bitcoin address of a 32 byte secp256k1 secret.
  String btcAddress(Uint8List secret32) =>
      _addr('blockchainer_btc_address', secret32);

  /// The Solana address of a 64 byte Ed25519 secret.
  String solAddress(Uint8List edSecret64) =>
      _addr('blockchainer_sol_address', edSecret64);

  /// ABI calldata for an ERC-20 transfer to a 20 byte address.
  Uint8List ethErc20TransferCalldata(Uint8List to20, int amount) => _scoped(
        [to20],
        (p) => _take(
          lib.lookupFunction<_Erc20C, _Erc20D>(
              'blockchainer_eth_erc20_transfer')(p[0], amount),
          'ethErc20TransferCalldata',
        ),
      );

  /// The 4 byte ABI selector of a function signature.
  Uint8List ethSelector(String signature) => _withUtf8(
        signature,
        (s) => _take(
          lib.lookupFunction<_SelectorC, _SelectorD>(
              'blockchainer_eth_selector')(s),
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
        lib.lookupFunction<_SignLegacyC, _SignLegacyD>(
                'blockchainer_eth_sign_legacy')(nonce, gasPrice, gasLimit, p[0],
            hasTo, value, p[1], data?.length ?? 0, chainId, p[2]),
        'ethSignLegacy',
      ),
    );
  }
}
