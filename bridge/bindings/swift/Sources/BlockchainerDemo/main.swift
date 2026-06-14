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

_ = warmed.wait(timeout: .now() + 2) // demo only; real apps never wait on preload
print("OK")
