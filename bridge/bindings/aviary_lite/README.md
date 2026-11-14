# aviary_lite

Aviary is a ledger you embed in your own app or service. A chain runs
in process, finalizes every block with BFT consensus, and lets anyone who
holds only its 32 byte state root verify what it contains. Money is one use;
the same engine keeps tamper evident application data. This package is
Aviary for Flutter, self contained: the CipherBird crypto engine is merged
into the native library, so there is no other dependency to add.

## What you get

- **Application records.** A signed key value store with owners, versions
  and values up to 64 KiB, each provable on its own. Use it for audit logs,
  asset and device registries, document notarization or shared app state.
- **Transfers with instant finality.** Accounts with balances and nonces,
  or UTXOs with multisig, finalized by Tendermint style rounds with BLS
  commit certificates. A finalized block never reverts.
- **Proofs and light clients.** Prove a balance or a record against a state
  root. A light client follows a chain from its genesis validators, across
  validator rotations, without running a full node.
- **Networking you control.** A consensus node with no sockets and no
  clock, so it runs over whatever transport the app picks, with Noise and
  SecureSession encrypted links, signed peer records and credential gated
  admission.
- **Composable chains.** Choose the consensus engine (BFT, PoS, VRF PoS,
  PoW), the state model and the proposer policy at genesis, plug in an
  engine written in Dart, anchor subchains to a parent and drive whole
  chain trees.
- **Key custody.** A passphrase sealed key store that signs inside the
  engine, portable encrypted accounts, threshold accounts and hybrid post
  quantum signatures (Ed25519 plus ML-DSA-65).
- **Privacy.** Sealed mempools that hide transactions until a block
  includes them, and BBS membership credentials that admit a member without
  revealing who it is.
- **Interop.** The same keys derive Ethereum, Bitcoin and Solana addresses,
  and the engine builds and signs real transactions for each.
- **Operations.** Typed event streams, metrics, and snapshots that resume a
  chain at once or replay it for an audit.

The same engine backs the Go, Node.js, JVM and Swift bindings, and every
binding computes byte identical state.

Prefer `aviary` when the app also uses the `cipherbird` package: that
variant shares one engine with it instead of carrying a second copy.

## Install

```yaml
dependencies:
  aviary_lite: ^1.1.0
```

Android, iOS, macOS and Linux desktop are supported. The native library is
bundled per platform and loaded automatically (on Linux through
`aviary_dart`'s build hook, so a Linux app needs no setup); call `Aviary.ensureInitialized()`
once at start to warm it off the UI isolate.

## Use

```dart
import 'package:aviary_lite/aviary_lite.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await AviaryLite.ensureInitialized();
  runApp(const MyApp());
}
```

Then anywhere in the app:

```dart
final node = AviaryLite.instance.createNode(7);
final alice = node.fundAccount(1000);
final bob = node.fundAccount(0);
node.sendTo(alice, bob, 250);
node.commit();
final ok = node.verifyAccountProof(node.proveAccount(bob.address));
```

`AviaryLite` is a thin front for `Aviary`; the whole `aviary_dart`
API is re-exported, including its shortcut extensions (`fundAccount`,
`sendTo`, `commit`, `hex`, `assembleSingleValidator`, `signWithShares`,
`produceRound`). See that package for the full surface.

## Example

The example app has four tabs: a ledger round trip with a balance proof, a
live event stream with metrics, an equipment record that is registered,
serviced, handed over and proven, and four validators reaching consensus
over a `PeerMesh`. Run it with `flutter run` from `example/`.
