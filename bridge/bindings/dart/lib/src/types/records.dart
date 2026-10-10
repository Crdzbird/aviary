part of '../../blockchainer.dart';

/// A validator's BLS public key and voting power.
typedef ValidatorEntry = ({Uint8List publicKey, int power});

/// A genesis funded account: a 32 byte address and its balance.
typedef GenesisAccountEntry = ({Uint8List address, int balance});

/// A tree validator; [secret] is null when this process does not hold it.
typedef TreeValidator = ({Uint8List publicKey, Uint8List? secret, int power});

/// A key pair as public and secret bytes.
typedef KeyPairBytes = ({Uint8List public, Uint8List secret});

/// Round one of a threshold signature: keep [nonces], share [commitment].
typedef ThresholdCommit = ({Uint8List nonces, Uint8List commitment});

/// A header, its certificate, and the validator set in force at its height.
typedef Checkpoint = ({
  Uint8List header,
  Uint8List certificate,
  List<ValidatorEntry> validators,
});

/// A proposer policy: the proposer index for [height] among [validators].
typedef ProposerPolicy = int Function(int height, int validators);

/// Supplies the encoded transactions a chain includes when it produces.
typedef TxSource = List<Uint8List> Function(int chainId, int timestamp);

/// Observes a finalized block on a chain.
typedef FinalizedHook = void Function(int chainId, Uint8List block);
