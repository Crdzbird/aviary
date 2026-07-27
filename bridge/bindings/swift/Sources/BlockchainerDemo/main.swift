// Swift demo of the Blockchainer wrapper, including the off-thread preload.
// Run on macOS: `swift run BlockchainerDemo`
import Blockchainer
import Foundation

// Optional: warm the native backend off the main thread. Fire-and-forget — the
// synchronous API below never waits on it.
let warmed = DispatchSemaphore(value: 0)
Blockchainer.preload { warmed.signal() }

print("blockchainer:", Blockchainer.version)
print("crypto backend:", Blockchainer.cryptoBackendVersion)

// A full ledger flow — fully synchronous, no await.
let node = try Blockchainer.makeNode(chainID: 42)
let alice = try node.newAccount()
let bob = try node.newAccount()
try node.credit(address: alice.address, amount: 1_000_000)
try node.submitTransfer(secret: alice.secret, to: bob.address, amount: 250_000, nonce: node.nonce(address: alice.address))
let height = try node.produceBlock(timestamp: 1)
print("height:", height, "bob balance:", node.balance(address: bob.address))

// Light-client proof round-trip.
let proof = try node.proveAccount(address: bob.address)
let ok = Blockchainer.verifyStateProof(stateRoot: try node.stateRoot(), proof: proof)
print("state proof verified:", ok)

// Interop address derivation.
let secp = Data(repeating: 0x11, count: 32)
print("eth:", try Blockchainer.ethAddress(secret32: secp))

// Portable account export — the three tiers, all fail-closed.
let hy = try node.newAccountHybrid()
let env1 = try Blockchainer.sealAccountPassphrase(publicKey: hy.publicKey, secret: hy.secret,
                                                  passphrase: "backup", ops: 3, mem: 1 << 16)
let back1 = try Blockchainer.openAccountPassphrase(envelope: env1, passphrase: "backup")
precondition(back1.secret == hy.secret, "passphrase round-trip")
var wrongPass = false
do { _ = try Blockchainer.openAccountPassphrase(envelope: env1, passphrase: "nope") } catch { wrongPass = true }
precondition(wrongPass, "wrong passphrase must fail")

let dev = try Blockchainer.hybridKemKeypair()
let env2 = try Blockchainer.sealAccountTo(publicKey: hy.publicKey, secret: hy.secret, recipientKemPublic: dev.publicKey)
let back2 = try Blockchainer.openAccountTo(envelope: env2, recipientKemSecret: dev.secret)
precondition(back2.secret == hy.secret, "recipient round-trip")

let env3 = try Blockchainer.sealAccountSignedTo(publicKey: hy.publicKey, secret: hy.secret,
                                                recipientKemPublic: dev.publicKey, senderSigSecret: hy.secret)
let back3 = try Blockchainer.openAccountSignedFrom(envelope: env3, recipientKemSecret: dev.secret,
                                                   senderSigPublic: hy.publicKey)
precondition(back3.secret == hy.secret, "signed round-trip")
let stranger = try Blockchainer.hybridKemKeypair()
var wrongSigner = false
do { _ = try Blockchainer.openAccountSignedFrom(envelope: env3, recipientKemSecret: dev.secret, senderSigPublic: stranger.publicKey) } catch { wrongSigner = true }
precondition(wrongSigner, "wrong expected signer must fail")
print("portable tiers OK (passphrase + recipient + signed, all fail-closed)")

_ = warmed.wait(timeout: .now() + 2) // demo only; real apps never wait on preload
print("OK")
