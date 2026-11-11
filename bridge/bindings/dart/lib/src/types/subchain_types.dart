part of '../../aviary.dart';

/// A registrar: the scheme and public key allowed to authorise registry
/// operations on a parent with a dynamic policy.
/// {@category Subchains and trees}
typedef RegistrarEntry = ({SignatureScheme scheme, Uint8List publicKey});

/// The sizes of a dynamic registry and its authority epoch, which grows by
/// one each time the registrar set is replaced.
/// {@category Subchains and trees}
typedef RegistryCounts = ({int records, int open, int authorityEpoch});

/// The first and last archived block heights of a chain.
/// {@category Chains}
typedef ArchiveRange = ({int first, int last});
