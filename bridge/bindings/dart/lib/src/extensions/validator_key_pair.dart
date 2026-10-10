part of '../../aviary.dart';

/// Turning a validator key pair into the entries the genesis, rotation and
/// tree APIs take.
extension ValidatorKeyPair on KeyPairBytes {
  /// A validator set entry with [power].
  ValidatorEntry entry(int power) => (publicKey: public, power: power);

  /// A tree validator with [power]. Set [holdsSecret] false to describe a
  /// validator this process does not run.
  TreeValidator treeValidator(int power, {bool holdsSecret = true}) => (
        publicKey: public,
        secret: holdsSecret ? secret : null,
        power: power,
      );
}
