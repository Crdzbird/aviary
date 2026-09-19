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

// CredentialGate Option B (pseudonymous — per-epoch Sybil resistance).
let pseudoPolicy = try Blockchainer.CredentialPolicy(chainID: 7, issuerPublic: issuer.publicKey,
                                                     header: Data("blockchainer.credential.v2".utf8),
                                                     minTier: 2, mode: .pseudonymous)
func issuePseudo(_ secret: Data) throws -> Data {
    let mc = try pseudoPolicy.membershipCommit(memberSecret: secret)
    let commitment = Blockchainer.extractCommitment(mc)
    let bi = try pseudoPolicy.blindIssue(issuerSecret: issuer.secret, commitment: commitment,
                                         tier: 3, notAfter: 4_102_444_800, issuerEpoch: 1)
    return try Blockchainer.finalizeMembership(membershipCommit: mc, blindIssuance: bi)
}
let pseudoCred = try issuePseudo(Data(repeating: 0x77, count: 32))
let pseudoGate = Blockchainer.makeCredentialGate(policy: pseudoPolicy)
func chd(_ b: UInt8) -> Data { Data(repeating: b, count: 32) }
let presP = try pseudoPolicy.presentPseudonymous(credential: pseudoCred, epoch: 5, scope: "gossip", challenge: chd(1))
let nymA = try pseudoGate.admitPseudonymous(epoch: 5, scope: "gossip", challenge: chd(1), presentation: presP, now: credNow)
precondition(nymA != nil && nymA!.count == 48, "admit returns a 48-byte pseudonym")
let presP2 = try pseudoPolicy.presentPseudonymous(credential: pseudoCred, epoch: 5, scope: "gossip", challenge: chd(2))
let nym2 = try pseudoGate.admitPseudonymous(epoch: 5, scope: "gossip", challenge: chd(2), presentation: presP2, now: credNow)
precondition(nym2 == nymA, "same member+epoch+scope → same pseudonym")
precondition(pseudoGate.noteAdmission(epoch: 5, scope: "gossip", nym: nymA!), "first admission fresh")
precondition(!pseudoGate.noteAdmission(epoch: 5, scope: "gossip", nym: nym2!), "Sybil cap flags the duplicate")
let presE = try pseudoPolicy.presentPseudonymous(credential: pseudoCred, epoch: 6, scope: "gossip", challenge: chd(1))
let nymE = try pseudoGate.admitPseudonymous(epoch: 6, scope: "gossip", challenge: chd(1), presentation: presE, now: credNow)
precondition(nymE != nil && nymE != nymA, "different epoch → unlinkable pseudonym")
print("credential-gate pseudonymous OK")

// Composable chains: declare a mixture, commit it into genesis, assemble, commit a block.
let engIds = try Blockchainer.engineIds()
precondition(engIds.contains("pos") && engIds.contains("pow"), "registry discovery")
let val = try Blockchainer.newBLSValidator(keyMaterial: Data(repeating: 0x60, count: 32))
let gen = try Blockchainer.buildGenesis(chainID: 7,
    accounts: [.init(address: Data(repeating: 0x11, count: 32), balance: 1000)],
    validators: [.init(publicKey: val.publicKey, power: 1)])
let posBp = try Blockchainer.Blueprint(consensus: "pos", stateModel: "account", proposer: "round-robin")
let mixedGen = try Blockchainer.commitBlueprint(genesis: gen, blueprint: posBp)
let genH0 = try Blockchainer.genesisHash(gen), genH1 = try Blockchainer.genesisHash(mixedGen)
precondition(genH0 != genH1, "the mixture changes the genesis hash")
let posChain = Blockchainer.assembleChain(genesis: mixedGen, me: 0, blsPub: val.publicKey, blsSec: val.secret)
precondition(posChain != nil, "PoS chain assembles")
let posBlk = try posChain!.propose(timestamp: 2)
let posJust = try posChain!.seal(posBlk)
precondition(posChain!.submit(block: posBlk, justification: posJust) && posChain!.height == 1, "PoS block committed")
let powBp = try Blockchainer.Blueprint(consensus: "pow", stateModel: "account", params: Data(repeating: 0xFF, count: 32))
let powGen = try Blockchainer.commitBlueprint(genesis: gen, blueprint: powBp)
let powChain = Blockchainer.assembleChain(genesis: powGen, me: 0, blsPub: val.publicKey, blsSec: val.secret)!
let cand = try powChain.propose(timestamp: 2)
let solved = try powChain.mine(cand)
precondition(powChain.submit(block: solved) && powChain.height == 1, "PoW block committed")
let badBp = try Blockchainer.Blueprint(consensus: "no-such-engine", stateModel: "account")
let badGen = try Blockchainer.commitBlueprint(genesis: gen, blueprint: badBp)
precondition(Blockchainer.assembleChain(genesis: badGen, me: 0, blsPub: val.publicKey, blsSec: val.secret) == nil, "unknown engine rejected")
print("chain-blueprint OK")

// Foreign components: a Swift-implemented engine + proposer join the mixture.
final class SwiftHashEngine: Blockchainer.ForeignEngine {
    var justified = 0, verified = 0
    static func tag(_ b: Data) -> Data { Data("swift-just:".utf8) + b.prefix(16) }
    func justify(block: Data) -> Data? { justified += 1; return Self.tag(block) }
    func verify(block: Data, justification: Data) -> Bool { verified += 1; return justification == Self.tag(block) }
}
let swiftEngine = SwiftHashEngine()
precondition(Blockchainer.registerEngine(id: "swift-engine", swiftEngine), "register Swift engine")
precondition(Blockchainer.registerProposer(id: "swift-first") { _, _ in 0 }, "register Swift proposer")
let feBp = try Blockchainer.Blueprint(consensus: "swift-engine", stateModel: "account")
let feGen = try Blockchainer.commitBlueprint(genesis: gen, blueprint: feBp)
let feChain = Blockchainer.assembleChain(genesis: feGen, me: 0, blsPub: val.publicKey, blsSec: val.secret)!
let feBlk = try feChain.propose(timestamp: 2)
let feJust = try feChain.seal(feBlk)   // → Swift justify
precondition(feJust.starts(with: Data("swift-just:".utf8)), "seal via Swift justify")
var feBad = feJust; feBad[feBad.count - 1] ^= 1
precondition(!feChain.submit(block: feBlk, justification: feBad) && feChain.height == 0, "tampered justification refused, ledger untouched")
precondition(feChain.submit(block: feBlk, justification: feJust) && feChain.height == 1, "Swift-justified block committed")
precondition(swiftEngine.justified >= 1 && swiftEngine.verified >= 2, "callbacks crossed the boundary")
let fpBp = try Blockchainer.Blueprint(consensus: "pos", stateModel: "account", proposer: "swift-first")
let fpChain = Blockchainer.assembleChain(genesis: try Blockchainer.commitBlueprint(genesis: gen, blueprint: fpBp), me: 0, blsPub: val.publicKey, blsSec: val.secret)!
let fpBlk = try fpChain.propose(timestamp: 2)
let fpJust = try fpChain.seal(fpBlk)
precondition(fpChain.submit(block: fpBlk, justification: fpJust) && fpChain.height == 1, "PoS with a Swift proposer committed")
print("foreign-components OK")

// Subchains: a BFT child anchored by a parent; a balance on the child proven from the parent header.
do {
    let cv = try Blockchainer.newBLSValidator(keyMaterial: Data(repeating: 0x71, count: 32))
    let pv = try Blockchainer.newBLSValidator(keyMaterial: Data(repeating: 0x72, count: 32))
    let alice = Data(repeating: 0xA1, count: 32)
    let bft = try Blockchainer.Blueprint(consensus: "bft", stateModel: "account")
    let childGen = try Blockchainer.commitBlueprint(genesis: try Blockchainer.buildGenesis(chainID: 42, accounts: [.init(address: alice, balance: 1000)], validators: [.init(publicKey: cv.publicKey, power: 1)]), blueprint: bft)
    let childHeader = try Blockchainer.genesisHeader(childGen)
    var parentGen = try Blockchainer.commitBlueprint(genesis: try Blockchainer.buildGenesis(chainID: 1, accounts: [], validators: [.init(publicKey: pv.publicKey, power: 1)]), blueprint: bft)
    let policy = try Blockchainer.encodeSubchainPolicy(maxChildren: 1, maxHeadersPerAnchor: 4, children: [.init(chainID: 42, genesisHeader: childHeader, validators: [.init(publicKey: cv.publicKey, power: 1)])])
    let plainHash = try Blockchainer.genesisHash(parentGen)
    parentGen = try Blockchainer.commitSubchainPolicy(genesis: parentGen, policy: policy)
    let parentHash = try Blockchainer.genesisHash(parentGen)
    precondition(plainHash != parentHash, "policy changes the parent genesis hash")
    let ownChild = try Blockchainer.encodeSubchainPolicy(maxChildren: 1, children: [.init(chainID: 1, genesisHeader: childHeader, validators: [.init(publicKey: cv.publicKey, power: 1)])])
    let refused = (try? Blockchainer.commitSubchainPolicy(genesis: parentGen, policy: ownChild)) == nil
    precondition(refused, "own-chain child refused")
    let bound = try Blockchainer.commitParentBinding(genesis: childGen, parentChainID: 1, parentGenesisHash: parentHash)
    let boundHash = try Blockchainer.genesisHash(bound), childHash = try Blockchainer.genesisHash(childGen), boundHeader = try Blockchainer.genesisHeader(bound)
    precondition(boundHash != childHash && boundHeader == childHeader, "binding changes the hash, not the header")
    let child = Blockchainer.assembleChain(genesis: bound, me: 0, blsPub: cv.publicKey, blsSec: cv.secret)!
    let parent = Blockchainer.assembleChain(genesis: parentGen, me: 0, blsPub: pv.publicKey, blsSec: pv.secret)!
    var certified: [Data] = []
    for ts: UInt64 in 2...3 {
        let blk = try child.propose(timestamp: ts); let cert = try child.certify(blk)
        precondition(child.submit(block: blk, justification: cert), "child submit")
        certified.append(try Blockchainer.certifiedHeader(header: try Blockchainer.blockHeader(blk), certificate: cert))
    }
    let tip = try parent.childTip(42)
    precondition(child.height == 2 && !tip.isEmpty, "child state")
    precondition((try? parent.childTip(99)) == nil && (try? child.childTip(42)) == nil, "unknown child / non-parent refused")
    let anchor = try Blockchainer.encodeAnchor(childChainID: 42, certifiedHeaders: certified)
    let pblk = try parent.proposeAnchored(timestamp: 2, anchors: [anchor])
    let pcert = try parent.certify(pblk)
    precondition(parent.submit(block: pblk, justification: pcert) && parent.height == 1, "parent anchored")
    var tampered = certified[1]; tampered[tampered.count - 1] ^= 1
    let badAnchor = try Blockchainer.encodeAnchor(childChainID: 42, certifiedHeaders: [tampered])
    precondition((try? parent.proposeAnchored(timestamp: 3, anchors: [badAnchor])) == nil, "tampered anchor refused")
    precondition((try? parent.proposeAnchored(timestamp: 3, anchors: [anchor])) == nil, "stale anchor refused")
    let root = try Blockchainer.headerSubchainRoot(try Blockchainer.blockHeader(pblk))
    let proof = try Blockchainer.encodeSubchainProof(hops: [try parent.subchainHop(42)], stateProof: try child.proveAccount(alice))
    precondition(Blockchainer.verifySubchainProof(parentSubchainRoot: root, proof: proof), "cross-chain proof verifies")
    var bad = proof; bad[bad.count - 1] ^= 1
    let rootless = try Blockchainer.encodeSubchainProof(hops: [], stateProof: try child.proveAccount(alice))
    precondition(!Blockchainer.verifySubchainProof(parentSubchainRoot: Data(repeating: 1, count: 32), proof: proof) && !Blockchainer.verifySubchainProof(parentSubchainRoot: root, proof: bad) && !Blockchainer.verifySubchainProof(parentSubchainRoot: root, proof: rootless), "tampered / rootless proofs fail")
    let blk3 = try child.propose(timestamp: 4); let cert3 = try child.certify(blk3); _ = child.submit(block: blk3, justification: cert3)
    let a2 = try Blockchainer.encodeAnchor(childChainID: 42, certifiedHeaders: [try Blockchainer.certifiedHeader(header: try Blockchainer.blockHeader(blk3), certificate: cert3)])
    let pblk2 = try parent.proposeAnchored(timestamp: 3, anchors: [a2])
    let pcert2 = try parent.certify(pblk2)
    precondition(parent.submit(block: pblk2, justification: pcert2), "parent block 2")
    let root2 = try Blockchainer.headerSubchainRoot(try Blockchainer.blockHeader(pblk2))
    precondition(!Blockchainer.verifySubchainProof(parentSubchainRoot: root2, proof: proof) && Blockchainer.verifySubchainProof(parentSubchainRoot: root, proof: proof), "old proof stale at new root, valid at old")
    let fresh = try Blockchainer.encodeSubchainProof(hops: [try parent.subchainHop(42)], stateProof: try child.proveAccount(alice))
    precondition(Blockchainer.verifySubchainProof(parentSubchainRoot: root2, proof: fresh), "fresh proof at new root")
}
print("subchains OK")

// Validator rotation: grow, certify under each set, rotate out, PoS refuses.
do {
    let me = try Blockchainer.newBLSValidator(keyMaterial: Data(repeating: 0x81, count: 32))
    let other = try Blockchainer.newBLSValidator(keyMaterial: Data(repeating: 0x82, count: 32))
    let bft = try Blockchainer.Blueprint(consensus: "bft", stateModel: "account")
    let gen = try Blockchainer.commitBlueprint(genesis: try Blockchainer.buildGenesis(chainID: 5, accounts: [], validators: [.init(publicKey: me.publicKey, power: 1)]), blueprint: bft)
    let chain = Blockchainer.assembleChain(genesis: gen, me: 0, blsPub: me.publicKey, blsSec: me.secret)!
    let initial = try chain.validators()
    precondition(initial.count == 1 && initial[0].publicKey == me.publicKey, "initial set")
    try chain.scheduleValidators([.init(publicKey: me.publicKey, power: 3), .init(publicKey: other.publicKey, power: 1)])
    let b1 = try chain.propose(timestamp: 2); let c1 = try chain.certify(b1)
    precondition(chain.submit(block: b1, justification: c1), "rotation block")
    let vs = try chain.validators()
    precondition(vs.count == 2 && vs[0].power == 3 && vs[1].publicKey == other.publicKey, "set after rotation")
    let b2 = try chain.propose(timestamp: 3); let c2 = try chain.certify(b2)
    precondition(chain.submit(block: b2, justification: c2) && chain.height == 2, "block under the new set")
    precondition((try? chain.scheduleValidators([])) == nil && (try? chain.scheduleValidators([.init(publicKey: me.publicKey, power: 0)])) == nil, "malformed sets refused")
    try chain.scheduleValidators([.init(publicKey: other.publicKey, power: 1)])
    let b3 = try chain.propose(timestamp: 4); let c3 = try chain.certify(b3)
    precondition(chain.submit(block: b3, justification: c3) && chain.height == 3, "leaving block")
    let b4 = try chain.propose(timestamp: 5)
    precondition((try? chain.certify(b4)) == nil, "a node that left cannot certify")
    let posGen = try Blockchainer.commitBlueprint(genesis: try Blockchainer.buildGenesis(chainID: 6, accounts: [], validators: [.init(publicKey: me.publicKey, power: 1)]), blueprint: try Blockchainer.Blueprint(consensus: "pos", stateModel: "account", proposer: "round-robin"))
    let pc = Blockchainer.assembleChain(genesis: posGen, me: 0, blsPub: me.publicKey, blsSec: me.secret)!
    precondition((try? pc.scheduleValidators([.init(publicKey: me.publicKey, power: 1)])) == nil, "PoS refuses")
}
print("validator-rotation OK")

// Light-client overlap rule: power moves A → B; a client trusting only A bisects through the rotation.
do {
    let va = try Blockchainer.newBLSValidator(keyMaterial: Data(repeating: 0x91, count: 32))
    let vb = try Blockchainer.newBLSValidator(keyMaterial: Data(repeating: 0x92, count: 32))
    let bft = try Blockchainer.Blueprint(consensus: "bft", stateModel: "account")
    let genesisSet = [Blockchainer.GenesisValidator(publicKey: va.publicKey, power: 1)]
    let lgen = try Blockchainer.commitBlueprint(genesis: try Blockchainer.buildGenesis(chainID: 8, accounts: [], validators: genesisSet), blueprint: bft)
    let gh = try Blockchainer.genesisHeader(lgen)
    let A = Blockchainer.assembleChain(genesis: lgen, me: 0, blsPub: va.publicKey, blsSec: va.secret)!
    let B = Blockchainer.assembleChain(genesis: lgen, me: 0, blsPub: vb.publicKey, blsSec: vb.secret)!
    var cps: [UInt64: Blockchainer.Checkpoint] = [:]
    try A.scheduleValidators([.init(publicKey: va.publicKey, power: 1), .init(publicKey: vb.publicKey, power: 3)])
    let b1 = try A.propose(timestamp: 2); let c1 = try A.certify(b1)
    precondition(A.submit(block: b1, justification: c1) && B.submit(block: b1, justification: c1), "rotation block")
    cps[1] = .init(header: try Blockchainer.blockHeader(b1), certificate: c1, validators: genesisSet)
    let newSet = try B.validators()
    for ts: UInt64 in 3...4 {
        let blk = try B.propose(timestamp: ts); let cert = try B.certify(blk)
        precondition(B.submit(block: blk, justification: cert) && A.submit(block: blk, justification: cert), "B block")
        cps[ts - 1] = .init(header: try Blockchainer.blockHeader(blk), certificate: cert, validators: newSet)
    }
    let lc = Blockchainer.newLightClient(chainID: 8, validators: genesisSet, trustedHeader: gh)!
    precondition(lc.acceptSkipping(cps[3]!) == .insufficientOverlap, "no-overlap skip reports thin overlap")
    precondition(lc.acceptSkipping(.init(header: cps[3]!.header, certificate: cps[3]!.certificate, validators: genesisSet)) == .refused, "wrong set refused")
    var badCert = cps[3]!.certificate; badCert[badCert.count - 1] ^= 1
    precondition(lc.acceptSkipping(.init(header: cps[3]!.header, certificate: badCert, validators: newSet)) == .refused && lc.height == 0, "tampered cert refused")
    var fetched = 0
    try lc.syncTo(cps[3]!, targetHeight: 3) { hh in fetched += 1; return cps[hh] }
    let lcSet = try lc.validators()
    precondition(lc.height == 3 && fetched > 0 && lcSet.count == 2 && lcSet[1].power == 3, "bisection sync")
    let b4 = try B.propose(timestamp: 5); let c4 = try B.certify(b4); _ = B.submit(block: b4, justification: c4)
    let h4 = try Blockchainer.blockHeader(b4)
    precondition(lc.accept(header: h4, certificate: c4) && lc.height == 4, "sequential accept after a sync")
    let lc2 = Blockchainer.newLightClient(chainID: 8, validators: genesisSet, trustedHeader: gh)!
    lc2.setMaxSkip(1)
    precondition(lc2.acceptSkipping(cps[2]!) == .refused, "max_skip refuses a 2-block jump")
    lc2.setMaxSkip(0)
    precondition((try? lc2.syncTo(cps[3]!, targetHeight: 3) { _ in nil }) == nil, "unavailable checkpoint fails")
    precondition(Blockchainer.newLightClient(chainID: 8, validators: [], trustedHeader: gh) == nil, "empty set refused")
}
print("light-client-skip OK")

_ = warmed.wait(timeout: .now() + 2) // demo only; real apps never wait on preload
print("OK")
