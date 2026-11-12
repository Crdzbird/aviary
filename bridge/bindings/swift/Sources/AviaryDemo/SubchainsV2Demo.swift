import Aviary
import Foundation

private func expect(_ condition: @autoclosure () throws -> Bool, _ message: @autoclosure () -> String) throws {
    let holds = try condition()
    precondition(holds, message())
}

private func hexString(_ d: Data) -> String { d.map { String(format: "%02x", $0) }.joined() }

private func hexData(_ s: String) -> Data {
    var out = Data(); var i = s.startIndex
    while i < s.endIndex { let j = s.index(i, offsetBy: 2); out.append(UInt8(s[i..<j], radix: 16)!); i = j }
    return out
}

private struct SubchainFixture {
    let secret: Data

    func recordGenesis(_ chainID: UInt64, _ v: PortableAccount) throws -> Data {
        let base = try Aviary.buildGenesis(chainID: chainID, stateModel: 2, accounts: [], validators: [.init(publicKey: v.publicKey, power: 1)])
        return try Aviary.commitBlueprint(genesis: base, blueprint: Aviary.Blueprint(consensus: "bft", stateModel: "record"))
    }
    func assemble(_ genesis: Data, _ v: PortableAccount) throws -> Aviary.Chain {
        guard let c = Aviary.assembleChain(genesis: genesis, me: 0, blsPub: v.publicKey, blsSec: v.secret) else {
            fatalError("assemble refused") }
        try c.enableArchive()
        return c
    }
    @discardableResult
    func commit(_ c: Aviary.Chain, _ block: Data) throws -> Data {
        try expect(c.submit(block: block, justification: try c.certify(block)), "block refused")
        return block
    }
    func registration(_ chainID: UInt64, _ genesis: Data, _ v: PortableAccount) throws -> Data {
        try Aviary.ChildRegistration(chainID: chainID, genesisHeader: Aviary.genesisHeader(genesis),
                                     validators: [.init(publicKey: v.publicKey, power: 1)]).encode()
    }
    func sign(_ op: Data, _ parentID: UInt64, _ parentHash: Data) throws -> Data {
        try Aviary.signOp(op, parentChainID: parentID, parentGenesisHash: parentHash, registrarIndex: 0, secret: secret)
    }
    func policy() throws -> Data {
        try Aviary.DynamicSubchainPolicy(threshold: 1, registrars: [.init(scheme: .ed25519, publicKey: secret.suffix(32))]).encode()
    }
}

private func loadVectors() -> [String: String]? {
    let url = URL(fileURLWithPath: #filePath).resolvingSymlinksInPath().deletingLastPathComponent()
        .appendingPathComponent("../../../../../vectors/subchain_v2_vectors.txt").standardized
    guard let text = try? String(contentsOf: url, encoding: .utf8) else { return nil }
    var out: [String: String] = [:]
    for line in text.split(separator: "\n") {
        let fields = line.split(separator: " ", omittingEmptySubsequences: true)
        if fields.isEmpty || fields[0].hasPrefix("#") { continue }
        precondition(fields.count == 2, "malformed vector line")
        out[String(fields[0])] = String(fields[1])
    }
    return out
}

/// Replays vectors/subchain_v2_vectors.txt, the bytes Go and Dart pin; false outside a repository checkout.
func runSubchainVectors() throws -> Bool {
    guard let want = loadVectors() else { return false }
    var seen = 0
    func check(_ name: String, _ got: Data) {
        seen += 1
        precondition(want[name] == hexString(got), "vector \(name): got \(hexString(got))")
    }
    let secret = hexData(want["registrar_secret"]!)
    let f = SubchainFixture(secret: secret)
    let server = try Aviary.newBLSValidator(keyMaterial: Data(repeating: 0x01, count: 32))
    let user = try Aviary.newBLSValidator(keyMaterial: Data(repeating: 0x10, count: 32))
    check("registrar_secret", secret)

    let policy = try f.policy()
    check("policy", policy)
    let mainGenesis = try Aviary.commitSubchainPolicy(genesis: f.recordGenesis(1, server), policy: policy)
    let mainHash = try Aviary.genesisHash(mainGenesis)
    check("main_genesis_hash", mainHash)
    check("main_genesis_header", try Aviary.genesisHeader(mainGenesis))
    let userGenesis = try Aviary.commitParentBinding(genesis: f.recordGenesis(1001, user), parentChainID: 1, parentGenesisHash: mainHash)
    let registration = try f.registration(1001, userGenesis, user)
    check("registration", registration)
    let opRegister = try Aviary.opRegister(authorityEpoch: 0, registration: registration)
    check("op_register", opRegister)
    check("op_register_digest", try Aviary.opDigest(opRegister, parentChainID: 1, parentGenesisHash: mainHash))
    let opRegisterSigned = try f.sign(opRegister, 1, mainHash)
    check("op_register_signed", opRegisterSigned)

    let mainChain = try f.assemble(mainGenesis, server), userChain = try f.assemble(userGenesis, user)
    let header1 = try Aviary.blockHeader(f.commit(mainChain, mainChain.proposeSubchain(timestamp: 10, ops: [opRegisterSigned])))
    check("header_1", header1)
    check("header_1_hash", try Aviary.encodedHeaderHash(header1))
    check("subchain_root_1", try Aviary.headerSubchainRoot(header1))
    check("child_record_1", try mainChain.childRecord(1001).encoded)

    let key = Data(repeating: 0x42, count: 32)
    try userChain.addTx(Aviary.signRecordTx(chainID: 1001, nonce: 0, key: key, op: .put, value: Data("door opened".utf8), secret: secret))
    try f.commit(userChain, userChain.propose(timestamp: 20))
    try f.commit(userChain, userChain.propose(timestamp: 21))
    let anchor = try Aviary.pendingAnchor(parent: mainChain, child: userChain)!
    check("anchor", anchor)
    let header2 = try Aviary.blockHeader(f.commit(mainChain, mainChain.proposeSubchain(timestamp: 11, anchors: [anchor])))
    check("header_2_hash", try Aviary.encodedHeaderHash(header2))
    let root2 = try Aviary.headerSubchainRoot(header2)
    check("subchain_root_2", root2)
    let record2 = try mainChain.childRecord(1001)
    check("child_record_2", record2.encoded)
    let proof2 = try mainChain.childProof(1001)
    check("child_proof_2", proof2)
    let proven = try Aviary.verifyChildProof(subchainRoot: root2, proof: proof2)
    check("child_proof_2_verified", proven.encoded)
    try expect(proven.tipHeight == 2 && proven.status == .open && proven.tipHash == (try Aviary.encodedHeaderHash(userChain.tip())), "proven record")
    let recordProof = try userChain.proveRecord(key: key)
    try expect(Aviary.verifyStateProof(stateRoot: proven.tipStateRoot, proof: recordProof), "record proof under the proven tip")
    let path = try Aviary.encodeSubchainProofV2(hops: [mainChain.subchainHopV2(1001)], stateProof: recordProof)
    check("path_proof_2", path)
    try expect(path.count > 0 && Aviary.verifySubchainProofV2(parentSubchainRoot: root2, proof: path)
        && !Aviary.verifySubchainProofV2(parentSubchainRoot: try Aviary.headerSubchainRoot(header1), proof: path), "path proof")
    check("main_snapshot_hash_2", try Aviary.hash(mainChain.snapshot()))
    check("user_snapshot_hash_2", try Aviary.hash(userChain.snapshot()))

    let opClose = try Aviary.opClose(authorityEpoch: 0, chainID: 1001, finalHeight: record2.tipHeight, finalTipHash: record2.tipHash)
    check("op_close", opClose)
    check("op_close_digest", try Aviary.opDigest(opClose, parentChainID: 1, parentGenesisHash: mainHash))
    let header3 = try Aviary.blockHeader(f.commit(mainChain, mainChain.proposeSubchain(timestamp: 12, ops: [f.sign(opClose, 1, mainHash)])))
    check("header_3_hash", try Aviary.encodedHeaderHash(header3))
    check("subchain_root_3", try Aviary.headerSubchainRoot(header3))
    let record3 = try mainChain.childRecord(1001)
    check("child_record_3", record3.encoded)
    try expect(record3.status == .closed && record3.closedHeight == 3, "closed record")
    check("main_snapshot_hash_3", try Aviary.hash(mainChain.snapshot()))

    let wideGenesis = try Aviary.commitSubchainPolicy(genesis: f.recordGenesis(2, server), policy: policy)
    let wideHash = try Aviary.genesisHash(wideGenesis)
    let wide = try f.assemble(wideGenesis, server)
    let ops = try (UInt64(2000)..<2011).map { id in
        try f.sign(Aviary.opRegister(authorityEpoch: 0, registration: f.registration(id, f.recordGenesis(id, user), user)), 2, wideHash)
    }
    let wideRoot = try Aviary.headerSubchainRoot(Aviary.blockHeader(f.commit(wide, wide.proposeSubchain(timestamp: 10, ops: ops))))
    check("wide_root", wideRoot)
    let wideProof = try wide.childProof(2007)
    check("wide_proof", wideProof)
    let wideRecord = try Aviary.verifyChildProof(subchainRoot: wideRoot, proof: wideProof)
    check("wide_proof_verified", wideRecord.encoded)
    try expect(wideRecord.chainID == 2007 && wideRecord.index == 7, "wide record")
    try expect(seen == want.count, "replayed \(seen) vectors, the file pins \(want.count)")
    return true
}

/// Rejections, registrar rotation, chain restore that continues byte for byte, and the archive.
func runDynamicSubchainRestart() throws {
    let node = try Aviary.makeNode(chainID: 0x5501)
    let secret = try node.newAccount().secret, stranger = try node.newAccount().secret
    let f = SubchainFixture(secret: secret)
    let server = try Aviary.newBLSValidator(keyMaterial: Data(repeating: 0x02, count: 32))
    let user = try Aviary.newBLSValidator(keyMaterial: Data(repeating: 0x12, count: 32))
    let mainGenesis = try Aviary.commitSubchainPolicy(genesis: f.recordGenesis(7, server), policy: f.policy())
    let mainHash = try Aviary.genesisHash(mainGenesis)
    let userGenesis = try Aviary.commitParentBinding(genesis: f.recordGenesis(7001, user), parentChainID: 7, parentGenesisHash: mainHash)
    let mainChain = try f.assemble(mainGenesis, server), userChain = try f.assemble(userGenesis, user)
    try expect(mainChain.registryCounts()?.records == 0 && userChain.registryCounts() == nil, "registry counts")

    func refusal(_ ops: [Data]) -> String {
        do { _ = try mainChain.proposeSubchain(timestamp: 10, ops: ops); return "" } catch { return String(describing: error) }
    }
    let op = try Aviary.opRegister(authorityEpoch: 0, registration: f.registration(7001, userGenesis, user))
    try expect(refusal([op]).contains("not enough approvals"), "unsigned operation")
    let forged = try Aviary.signOp(op, parentChainID: 7, parentGenesisHash: mainHash, registrarIndex: 0, secret: stranger)
    try expect(refusal([forged]).contains("bad registrar signature"), "forged operation")
    var otherParent = mainHash; otherParent[otherParent.startIndex] ^= 1
    try expect(!refusal([try Aviary.signOp(op, parentChainID: 7, parentGenesisHash: otherParent, registrarIndex: 0, secret: secret)]).isEmpty, "replay across parents")
    try expect(try Aviary.opDigest(op, parentChainID: 7, parentGenesisHash: mainHash).count == 32, "digest")
    let signed = try f.sign(op, 7, mainHash)
    try expect((try? Aviary.signOp(signed, parentChainID: 7, parentGenesisHash: mainHash, registrarIndex: 0, secret: secret)) == nil, "a registrar approves once")
    try f.commit(mainChain, mainChain.proposeSubchain(timestamp: 10, ops: [signed]))
    try f.commit(userChain, userChain.propose(timestamp: 20))
    try f.commit(mainChain, mainChain.proposeSubchain(timestamp: 11, anchors: [Aviary.pendingAnchor(parent: mainChain, child: userChain)!]))
    try expect(try Aviary.pendingAnchor(parent: mainChain, child: userChain) == nil, "nothing pending after anchoring")

    let mainSnapshot = try mainChain.snapshot(), userSnapshot = try userChain.snapshot()
    try expect(Aviary.chainSnapshotError(genesis: mainGenesis, snapshot: mainSnapshot).isEmpty, "a good snapshot has no error")
    let mainBack = try Aviary.restoreChain(genesis: mainGenesis, me: 0, blsPub: server.publicKey, blsSec: server.secret, snapshot: mainSnapshot)
    let userBack = try Aviary.restoreChain(genesis: userGenesis, me: 0, blsPub: user.publicKey, blsSec: user.secret, snapshot: userSnapshot)
    try expect(try mainBack.snapshot() == mainSnapshot && mainBack.tip() == mainChain.tip(), "restored chain differs")
    for u in [userChain, userBack] { try f.commit(u, u.propose(timestamp: 21)) }
    let straight = try f.commit(mainChain, mainChain.proposeSubchain(timestamp: 12, anchors: [Aviary.pendingAnchor(parent: mainChain, child: userChain)!]))
    let resumed = try f.commit(mainBack, mainBack.proposeSubchain(timestamp: 12, anchors: [Aviary.pendingAnchor(parent: mainBack, child: userBack)!]))
    try expect(straight == resumed && (try mainBack.snapshot()) == (try mainChain.snapshot()), "a restored chain continues byte for byte")
    try expect(mainBack.archiveRange().map { $0.first == 1 && $0.last == 3 } == true, "archive range")
    try expect(try mainBack.archivedBlock(3) == straight && !(try mainBack.archivedCertificate(3)).isEmpty, "archived block")
    try expect(mainBack.pruneArchive(below: 3) == 2 && mainBack.archiveRange().map { $0.first } == 3, "prune")

    var trailing = mainSnapshot; trailing.append(0)
    try expect((try? Aviary.restoreChain(genesis: mainGenesis, me: 0, blsPub: server.publicKey, blsSec: server.secret, snapshot: trailing)) == nil, "trailing bytes refused")
    do {
        _ = try Aviary.restoreChain(genesis: userGenesis, me: 0, blsPub: server.publicKey, blsSec: server.secret, snapshot: mainSnapshot)
        preconditionFailure("another genesis must be refused")
    } catch { try expect(String(describing: error).contains("another genesis"), "another genesis: \(error)") }
    try expect(try mainBack.childRecord(7001).tipHeight == 2, "record after restore")
    let root = try Aviary.headerSubchainRoot(mainBack.tip())
    var proof = try mainBack.childProof(7001)
    _ = try Aviary.verifyChildProof(subchainRoot: root, proof: proof)
    proof[proof.index(before: proof.endIndex)] ^= 1
    try expect((try? Aviary.verifyChildProof(subchainRoot: root, proof: proof)) == nil, "a tampered proof is refused")
    try expect((try? mainBack.childRecord(9)) == nil, "unknown child")

    let newKey = try node.newAccount().secret
    let rotate = try Aviary.opSetRegistrars(authorityEpoch: 0, threshold: 1, registrars: [.init(scheme: .ed25519, publicKey: newKey.suffix(32))])
    try f.commit(mainChain, mainChain.proposeSubchain(timestamp: 13, ops: [f.sign(rotate, 7, mainHash)]))
    try expect(mainChain.registryCounts()?.authorityEpoch == 1, "registrar rotation bumps the epoch")
    let tipRecord = try mainChain.childRecord(7001)
    let staleClose = try Aviary.opClose(authorityEpoch: 0, chainID: 7001, finalHeight: tipRecord.tipHeight, finalTipHash: tipRecord.tipHash)
    try expect(!refusal([try f.sign(staleClose, 7, mainHash)]).isEmpty, "an operation for the old epoch is refused")
    let close = try Aviary.opClose(authorityEpoch: 1, chainID: 7001, finalHeight: tipRecord.tipHeight, finalTipHash: tipRecord.tipHash)
    try f.commit(mainChain, mainChain.proposeSubchain(timestamp: 14, ops: [SubchainFixture(secret: newKey).sign(close, 7, mainHash)]))
    let closed = try mainChain.childRecord(7001)
    try expect(closed.status == .closed && mainChain.registryCounts()?.open == 0, "the new registrar closes the child")
}

/// A chain tree saved and restored continues byte for byte.
func runChainTreeSnapshot() throws {
    let rootKey = try Aviary.newBLSValidator(keyMaterial: Data(repeating: 0x03, count: 32))
    let childKey = try Aviary.newBLSValidator(keyMaterial: Data(repeating: 0x13, count: 32))
    let spec = Aviary.TreeNode(chainID: 1, blueprint: try Aviary.Blueprint(consensus: "pos", stateModel: "account", proposer: "round-robin"),
                               validators: [.init(publicKey: rootKey.publicKey, secret: rootKey.secret)],
                               children: [.init(chainID: 42, validators: [.init(publicKey: childKey.publicKey, secret: childKey.secret)])])
    let straight = try Aviary.buildChainTree(spec)
    var first: Aviary.ChainTree? = try Aviary.buildChainTree(spec)
    func round(_ t: Aviary.ChainTree, _ ts: UInt64) throws -> Data { try t.produce(42, timestamp: ts) + t.produce(1, timestamp: ts) }
    try expect(try round(straight, 10) == round(first!, 10), "two trees from one spec agree")
    let snapshot = try first!.snapshot()
    first = nil
    let second = try Aviary.restoreChainTree(spec, snapshot: snapshot)
    try expect(try second.snapshot() == snapshot && round(straight, 11) == round(second, 11), "a restored tree continues byte for byte")
    try expect((try? Aviary.restoreChainTree(spec, snapshot: snapshot + Data([1]))) == nil, "trailing bytes refused")
}
