import AviaryC
import Foundation

extension Aviary {
    /// The signature scheme of a registrar key: the transaction schemes.
    public enum SignatureScheme: UInt8 { case ed25519 = 1, hybrid = 2, slhDSA = 4 }
    /// Whether a parent still anchors a child. A closed child keeps its final tip provable.
    public enum ChildStatus: UInt8 { case open = 1, closed = 2 }
    /// The kind tag in the first byte of a version 2 proof hop.
    public enum HopKind: UInt8 { case flat = 1, record = 2 }

    /// A key allowed to authorise registry operations on a parent with a dynamic policy.
    public struct Registrar {
        public var scheme: SignatureScheme; public var publicKey: Data
        public init(scheme: SignatureScheme, publicKey: Data) { self.scheme = scheme; self.publicKey = publicKey }
        public func encode() throws -> Data {
            ensureReady()
            return try consume(publicKey.withC { p, l in aviary_registrar_encode(scheme.rawValue, p, l) }, "registrar_encode")
        }
    }

    static func optionalList(_ items: [Data]) -> Data { items.isEmpty ? Data() : blobList(items) }

    static func registrarList(_ registrars: [Registrar]) throws -> Data {
        optionalList(try registrars.map { try $0.encode() })
    }

    /// A parent's version 2 policy. `maxChildren` bounds OPEN children; 0 is unlimited for
    /// every bound. No registrars with threshold 0 is a registry sealed at genesis. Children
    /// registered at genesis must be ascending and unique by chain id.
    public struct DynamicSubchainPolicy {
        public var maxDepth: UInt8; public var maxChildren: UInt32
        public var maxAnchorsPerBlock: UInt16; public var maxHeadersPerAnchor: UInt16; public var maxOpsPerBlock: UInt16
        public var threshold: UInt8; public var registrars: [Registrar]; public var children: [ChildRegistration]
        public init(maxDepth: UInt8 = 1, maxChildren: UInt32 = 0, maxAnchorsPerBlock: UInt16 = 0, maxHeadersPerAnchor: UInt16 = 0,
                    maxOpsPerBlock: UInt16 = 0, threshold: UInt8, registrars: [Registrar], children: [ChildRegistration] = []) {
            self.maxDepth = maxDepth; self.maxChildren = maxChildren; self.maxAnchorsPerBlock = maxAnchorsPerBlock
            self.maxHeadersPerAnchor = maxHeadersPerAnchor; self.maxOpsPerBlock = maxOpsPerBlock
            self.threshold = threshold; self.registrars = registrars; self.children = children
        }
        /// The canonical encoding; commit it with `Aviary.commitSubchainPolicy`.
        public func encode() throws -> Data {
            ensureReady()
            let regs = try Aviary.registrarList(registrars)
            let kids = Aviary.optionalList(try children.map { try $0.encode() })
            return try consume(regs.withC { rp, rl in kids.withC { kp, kl in
                aviary_dynamic_subchain_policy_encode(maxDepth, maxChildren, maxAnchorsPerBlock, maxHeadersPerAnchor,
                                                      maxOpsPerBlock, threshold, rp, rl, kp, kl) } }, "dynamic_subchain_policy_encode")
        }
    }

    /// The canonical 32 byte hash of an encoded header of any version.
    public static func encodedHeaderHash(_ header: Data) throws -> Data {
        ensureReady(); return try consume(header.withC { hp, hl in aviary_encoded_header_hash(hp, hl) }, "encoded_header_hash")
    }

    static func requireHash(_ hash: Data, _ what: String) throws {
        if hash.count != 32 { throw AviaryError(message: "operation: \(what) must be 32 bytes") }
    }

    /// An UNSIGNED operation registering a child (an encoded `ChildRegistration`), valid in
    /// `authorityEpoch` up to parent height `validUntil` (0 = no expiry).
    public static func opRegister(authorityEpoch: UInt32, validUntil: UInt64 = 0, registration: Data) throws -> Data {
        ensureReady()
        return try consume(registration.withC { rp, rl in aviary_subchain_op_register(authorityEpoch, validUntil, rp, rl) }, "subchain_op_register")
    }
    /// An UNSIGNED operation closing a child at its recorded tip (height and 32 byte header hash).
    public static func opClose(authorityEpoch: UInt32, validUntil: UInt64 = 0, chainID: UInt64, finalHeight: UInt64, finalTipHash: Data) throws -> Data {
        ensureReady(); try requireHash(finalTipHash, "final tip hash")
        return try consume(finalTipHash.withC { tp, _ in aviary_subchain_op_close(authorityEpoch, validUntil, chainID, finalHeight, tp) }, "subchain_op_close")
    }
    /// An UNSIGNED operation replacing the registrar set. Applying it increments the authority
    /// epoch, which voids every operation signed for the old one.
    public static func opSetRegistrars(authorityEpoch: UInt32, validUntil: UInt64 = 0, threshold: UInt8, registrars: [Registrar]) throws -> Data {
        ensureReady()
        let regs = try registrarList(registrars)
        return try consume(regs.withC { rp, rl in aviary_subchain_op_set_registrars(authorityEpoch, validUntil, threshold, rp, rl) }, "subchain_op_set_registrars")
    }
    /// The 32 byte digest registrars sign for an operation on the parent named by its chain id and genesis hash.
    public static func opDigest(_ op: Data, parentChainID: UInt64, parentGenesisHash: Data) throws -> Data {
        ensureReady(); try requireHash(parentGenesisHash, "parent genesis hash")
        return try consume(op.withC { xp, xl in parentGenesisHash.withC { hp, _ in
            aviary_subchain_op_digest(xp, xl, parentChainID, hp) } }, "subchain_op_digest")
    }
    /// Signs an operation as registrar `registrarIndex` and returns it with that approval added.
    /// Throws when that registrar already approved. The secret is not retained.
    public static func signOp(_ op: Data, parentChainID: UInt64, parentGenesisHash: Data, registrarIndex: UInt16,
                              scheme: SignatureScheme = .ed25519, secret: Data) throws -> Data {
        ensureReady(); try requireHash(parentGenesisHash, "parent genesis hash")
        return try consume(op.withC { xp, xl in parentGenesisHash.withC { hp, _ in secret.withC { sp, sl in
            aviary_subchain_op_sign(xp, xl, parentChainID, hp, registrarIndex, scheme.rawValue, sp, sl) } } }, "subchain_op_sign")
    }
    /// Adds an approval whose signature over `opDigest` was made elsewhere.
    public static func addOpApproval(_ op: Data, registrarIndex: UInt16, signature: Data) throws -> Data {
        ensureReady()
        return try consume(op.withC { xp, xl in signature.withC { sp, sl in
            aviary_subchain_op_add_approval(xp, xl, registrarIndex, sp, sl) } }, "subchain_op_add_approval")
    }

    /// The sizes of a dynamic registry and its authority epoch.
    public struct RegistryCounts { public let records: UInt32; public let open: UInt32; public let authorityEpoch: UInt32 }

    /// One child's entry in a dynamic registry: what the parent commits about it and what a proof carries.
    public struct ChildRecord {
        public let index: UInt32; public let chainID: UInt64; public let status: ChildStatus; public let maxDepth: UInt8
        public let genesisHeaderHash: Data
        public let registeredHeight: UInt64; public let closedHeight: UInt64; public let tipHeight: UInt64
        public let tipHash: Data; public let tipStateRoot: Data; public let tipSubchainRoot: Data
        public let vsetHash: Data; public let tipVsetHash: Data
        /// The 230 byte encoding.
        public let encoded: Data

        /// Parses the fixed 230 byte record layout.
        public init(decoding b: Data) throws {
            guard b.count == 230 else { throw AviaryError(message: "child record: must be 230 bytes") }
            let x = [UInt8](b)
            func u64(_ o: Int) -> UInt64 { x[o..<o + 8].reduce(UInt64(0)) { $0 << 8 | UInt64($1) } }
            func h32(_ o: Int) -> Data { Data(x[o..<o + 32]) }
            index = x[0..<4].reduce(UInt32(0)) { $0 << 8 | UInt32($1) }
            chainID = u64(4); status = x[12] == 2 ? .closed : .open; maxDepth = x[13]
            genesisHeaderHash = h32(14)
            registeredHeight = u64(46); closedHeight = u64(54); tipHeight = u64(62)
            tipHash = h32(70); tipStateRoot = h32(102); tipSubchainRoot = h32(134)
            vsetHash = h32(166); tipVsetHash = h32(198)
            encoded = Data(x)
        }
    }

    /// Checks a child proof against a 32 byte version 4 subchain root from a parent header the
    /// caller trusts and returns the proven record.
    public static func verifyChildProof(subchainRoot: Data, proof: Data) throws -> ChildRecord {
        ensureReady()
        guard subchainRoot.count == 32 else { throw AviaryError(message: "child proof: subchain root must be 32 bytes") }
        return try ChildRecord(decoding: try consume(subchainRoot.withC { rp, _ in proof.withC { pp, pl in
            aviary_child_proof_verify(rp, pp, pl) } }, "child_proof_verify"))
    }
    /// The next anchor `parent` can take for `child`, built from the child's archive and the
    /// parent's recorded tip, at most `limit` headers (0 = the policy bound); nil when nothing
    /// is pending. Neither chain may be in use by another thread.
    public static func pendingAnchor(parent: Chain, child: Chain, limit: UInt16 = 0) throws -> Data? {
        let b = try consume(aviary_chain_pending_anchor(UnsafeMutableRawPointer(parent.h), UnsafeMutableRawPointer(child.h), limit), "chain_pending_anchor")
        return b.isEmpty ? nil : b
    }
    /// A cross-chain proof through any mixture of parents: tagged hops (top parent first) and the
    /// state proof of the last chain.
    public static func encodeSubchainProofV2(hops: [Data], stateProof: Data) throws -> Data {
        ensureReady(); let blob = optionalList(hops)
        return try consume(blob.withC { hp, hl in stateProof.withC { sp, sl in aviary_subchain_proof_v2_encode(hp, hl, sp, sl) } }, "subchain_proof_v2_encode")
    }
    /// Whether the proof is committed under the parent's 32 byte subchain root all the way down
    /// and its state proof holds.
    public static func verifySubchainProofV2(parentSubchainRoot: Data, proof: Data) -> Bool {
        ensureReady(); guard parentSubchainRoot.count == 32 else { return false }
        return parentSubchainRoot.withC { rp, _ in proof.withC { pp, pl in aviary_subchain_proof_v2_verify(rp, pp, pl) == 1 } }
    }
}

extension Aviary.Chain {
    private var raw: UnsafeMutableRawPointer { UnsafeMutableRawPointer(h) }

    /// Proposes the next block on a parent with a dynamic policy: anchors (ascending child id)
    /// and signed operations, operations applied first. Throws with the reason when any is invalid.
    public func proposeSubchain(timestamp: UInt64, anchors: [Data] = [], ops: [Data] = []) throws -> Data {
        let a = Aviary.optionalList(anchors), o = Aviary.optionalList(ops)
        return try consume(a.withC { ap, al in o.withC { op, ol in
            aviary_chain_propose_subchain(raw, timestamp, ap, al, op, ol) } }, "chain_propose_subchain")
    }
    /// The dynamic registry's counts, or nil when this chain has no dynamic policy.
    public func registryCounts() -> Aviary.RegistryCounts? {
        var records: UInt32 = 0, open: UInt32 = 0, epoch: UInt32 = 0
        guard aviary_chain_registry_counts(raw, &records, &open, &epoch) == 1 else { return nil }
        return Aviary.RegistryCounts(records: records, open: open, authorityEpoch: epoch)
    }
    /// The current record of a child; throws when this chain has no dynamic policy or the child
    /// was never registered.
    public func childRecord(_ childChainID: UInt64) throws -> Aviary.ChildRecord {
        try Aviary.ChildRecord(decoding: try consume(aviary_chain_child_record(raw, childChainID), "chain_child_record"))
    }
    /// A proof that a child's record is committed by this parent's CURRENT subchain root.
    public func childProof(_ childChainID: UInt64) throws -> Data {
        try consume(aviary_chain_child_proof(raw, childChainID), "chain_child_proof")
    }
    /// A proof hop from this parent's current registry into a child, its `HopKind` in the first byte.
    public func subchainHopV2(_ childChainID: UInt64) throws -> Data {
        try consume(aviary_chain_subchain_hop_v2(raw, childChainID), "chain_subchain_hop_v2")
    }
}
