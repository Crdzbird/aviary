part of '../../aviary.dart';

/// The step of a consensus round a timeout belongs to.
/// {@category Networking}
enum ConsensusStep {
  /// Waiting for the proposer's block.
  propose(0),

  /// Waiting for prevotes.
  prevote(1),

  /// Waiting for precommits.
  precommit(2);

  const ConsensusStep(this.code);

  /// The ABI value.
  final int code;

  /// The step for an ABI [code]; unknown codes map to [propose].
  static ConsensusStep fromCode(int code) =>
      values.firstWhere((s) => s.code == code, orElse: () => propose);
}

/// A validator's BLS public key and voting power.
/// {@category Ledger}
typedef ValidatorEntry = ({Uint8List publicKey, int power});

/// A genesis funded account: a 32 byte address and its balance.
/// {@category Ledger}
typedef GenesisAccountEntry = ({Uint8List address, int balance});

/// A tree validator; `secret` is null when this process does not hold it.
/// {@category Subchains and trees}
typedef TreeValidator = ({Uint8List publicKey, Uint8List? secret, int power});

/// A key pair as public and secret bytes.
/// {@category Ledger}
typedef KeyPairBytes = ({Uint8List public, Uint8List secret});

/// Round one of a threshold signature: keep `nonces`, share `commitment`.
/// {@category Custody}
typedef ThresholdCommit = ({Uint8List nonces, Uint8List commitment});

/// A header, its certificate, and the validator set in force at its height.
/// {@category Light clients}
typedef Checkpoint = ({
  Uint8List header,
  Uint8List certificate,
  List<ValidatorEntry> validators,
});

/// A proposer policy: the proposer index for [height] among [validators].
/// {@category Chains}
typedef ProposerPolicy = int Function(int height, int validators);

/// Supplies the encoded transactions a chain includes when it produces.
/// {@category Subchains and trees}
typedef TxSource = List<Uint8List> Function(int chainId, int timestamp);

/// Observes a finalized block on a chain.
/// {@category Subchains and trees}
typedef FinalizedHook = void Function(int chainId, Uint8List block);

/// A gossip message to send to one peer.
/// {@category Networking}
typedef Relay = ({int to, Uint8List message});

/// A consensus timer to arm; when it fires, call [NetNode.onTimeout].
/// {@category Networking}
typedef TimeoutRequest = ({ConsensusStep step, int round});

/// What a [NetNode] wants done after an input: relays to send and timers to
/// arm. The duration of a timer is the caller's choice.
/// {@category Networking}
typedef Outbound = ({List<Relay> relays, List<TimeoutRequest> timeouts});

/// A long term peer identity: a hybrid KEM pair to receive handshakes and a
/// hybrid signature pair to authenticate.
/// {@category Networking}
typedef SecureIdentity = ({KeyPairBytes kem, KeyPairBytes sig});

/// What a record transaction does: write, hand over to a new owner, or remove.
/// {@category Records}
enum RecordOp { put, transfer, delete }

/// A committed application record: owner, write count and current bytes.
/// {@category Records}
typedef RecordEntry = ({Uint8List owner, int version, Uint8List value});

/// A record read out of a verified state proof, with its key.
/// {@category Records}
typedef RecordLeaf = ({
  Uint8List key,
  Uint8List owner,
  int version,
  Uint8List value,
});
