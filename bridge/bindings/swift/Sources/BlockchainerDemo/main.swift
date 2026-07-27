// Swift demo of the Blockchainer wrapper, including the off-thread preload.
// Run on macOS: `swift run BlockchainerDemo`
import Blockchainer
import Foundation
import CryptoKit

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

// Threshold (FROST 2-of-3): the aggregate is a standard Ed25519 signature.
let shares = try Blockchainer.createThresholdAccount(n: 3, t: 2)
precondition(shares.count == 3, "3 share cards")
let tGroupPK = shares[0].groupPublicKey
let tMsg = Data("threshold-signed transfer".utf8)
let (n0, com0) = try Blockchainer.commit(card: shares[0].bytes)
let (n2, com2) = try Blockchainer.commit(card: shares[2].bytes)
let ceremony = try Blockchainer.makeCeremony(groupPK: tGroupPK, message: tMsg, commitments: [com2, com0])
let sh0 = try ceremony.sign(card: shares[0].bytes, nonces: n0)
let sh2 = try ceremony.sign(card: shares[2].bytes, nonces: n2)
precondition(ceremony.verifyShare(id: shares[0].index, publicShare: shares[0].publicShare, sigShare: sh0), "honest share attributed")
var badShare = sh2; badShare[badShare.startIndex + 5] ^= 1
precondition(!ceremony.verifyShare(id: shares[2].index, publicShare: shares[2].publicShare, sigShare: badShare), "corrupt share detected")
let tSig = try ceremony.aggregate(ids: [shares[0].index, shares[2].index], sigShares: [sh0, sh2])
precondition(tSig.count == 64, "aggregate is a 64-byte Ed25519 signature")
let edPub = try Curve25519.Signing.PublicKey(rawRepresentation: tGroupPK)
precondition(edPub.isValidSignature(tSig, for: tMsg), "aggregate verifies as Ed25519 under the group key")
let tEnv = try Blockchainer.sealShare(card: shares[1].bytes, passphrase: "share-pass", ops: 3, mem: 1 << 16)
let tBack = try Blockchainer.unsealShare(envelope: tEnv, passphrase: "share-pass")
precondition(tBack == shares[1].bytes, "sealed share round-trip")
print("threshold OK (2-of-3 -> Ed25519, attribution, sealed)")

// AssuranceProfile tiers + SealedMempool (anti-MEV).
let hardProfile = try Blockchainer.describeAssurance(level: 2)
precondition(hardProfile.ratcheted && hardProfile.vrf && hardProfile.thresholdCustody && !hardProfile.sealedMempool, "hardened tiers")
let fortProfile = try Blockchainer.describeAssurance(level: 3)
precondition(fortProfile.sealedMempool, "fortress seals the mempool")
let proposerKem = try Blockchainer.hybridKemKeypair()
let sealTx = try Blockchainer.encodeTx(chainID: 7, fee: 300, body: Data([1]), author: Data(count: 32), signature: Data(count: 64))
let pool = Blockchainer.makeSealedMempool()
let sealedBlob = try Blockchainer.sealTx(txEncoded: sealTx, proposerKemPublic: proposerKem.publicKey)
precondition(pool.add(sealedBlob), "add")
let revealed = try pool.reveal(proposerKemSecret: proposerKem.secret, max: 10)
precondition(revealed.count == 1, "proposer reveals its sealed tx")
let strangerKem = try Blockchainer.hybridKemKeypair()
let strangerReveal = try pool.reveal(proposerKemSecret: strangerKem.secret, max: 10)
precondition(strangerReveal.isEmpty, "a non-proposer opens nothing")
print("assurance + sealed-mempool OK")

// CredentialGate (anonymous membership admission, Option A — unlinkable).
let issuer = try Blockchainer.newBBSIssuer(keyMaterial: Data(repeating: 0x11, count: 32))
precondition(issuer.publicKey.count == 96 && issuer.secret.count == 32, "BBS issuer key sizes")
let credPolicy = try Blockchainer.CredentialPolicy(chainID: 7, issuerPublic: issuer.publicKey,
                                                   header: Data("blockchainer.credential.v1".utf8), minTier: 2)
let credPolicyHash = try credPolicy.hash()
precondition(credPolicyHash.count == 32, "policy hash is 32 bytes")
let memberSecret = Data(repeating: 0xAB, count: 32)
let credNow: UInt64 = 1_700_000_000
let cred = try credPolicy.issue(issuerSecret: issuer.secret, memberSecret: memberSecret,
                                tier: 3, notAfter: 4_102_444_800, issuerEpoch: 1)
let credGate = Blockchainer.makeCredentialGate(policy: credPolicy)
let credChallenge = Data(repeating: 0x01, count: 32)
let pres = try credPolicy.present(credential: cred, epoch: 5, scope: "submit", challenge: credChallenge)
precondition(credGate.admit(epoch: 5, scope: "submit", challenge: credChallenge, presentation: pres, now: credNow),
             "valid presentation admitted")
let pres2 = try credPolicy.present(credential: cred, epoch: 5, scope: "submit", challenge: credChallenge)
precondition(pres != pres2, "two presentations of one credential are unlinkable")
precondition(!credGate.admit(epoch: 6, scope: "submit", challenge: credChallenge, presentation: pres, now: credNow),
             "wrong epoch rejected")
let lowCred = try credPolicy.issue(issuerSecret: issuer.secret, memberSecret: memberSecret,
                                   tier: 1, notAfter: 4_102_444_800, issuerEpoch: 1)
let lowPres = try credPolicy.present(credential: lowCred, epoch: 5, scope: "submit", challenge: credChallenge)
precondition(!credGate.admit(epoch: 5, scope: "submit", challenge: credChallenge, presentation: lowPres, now: credNow),
             "below-min-tier rejected")
precondition(!credGate.admit(epoch: 5, scope: "submit", challenge: credChallenge,
                             presentation: Data("garbage".utf8), now: credNow), "malformed rejected")
print("credential-gate OK")

_ = warmed.wait(timeout: .now() + 2) // demo only; real apps never wait on preload
print("OK")
