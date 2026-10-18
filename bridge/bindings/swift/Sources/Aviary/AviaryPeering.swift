import AviaryC
import Foundation

extension KeyStore {
    /// A record write signed by the stored account at `from` (record state model). The
    /// scheme follows the stored key; the secret stays in the engine.
    public func signRecord(chainID: UInt64, from: Data, nonce: UInt64, key: Data,
                           op: Aviary.RecordOp, value: Data = Data()) throws -> Data {
        try consume(from.withC { fp, _ in key.withC { kp, _ in value.withC { vp, vl in
            aviary_keystore_sign_record(rawHandle, chainID, fp, nonce, kp, op.rawValue, vp, vl) } } },
            "keystore_sign_record")
    }
}

extension Aviary.Chain {
    /// Signs a record write with the key store account at `from`, nonce from the chain, and
    /// queues it.
    @discardableResult
    public func writeFromStore(store: KeyStore, from: Data, key: Data, op: Aviary.RecordOp,
                               value: Data = Data()) throws -> Data {
        try addTx(store.signRecord(chainID: chainID, from: from, nonce: recordNonce(address: from),
                                   key: key, op: op, value: value))
    }
}

extension Aviary {
    /// An admission challenge: keep `nonce`, send `wire` to the peer.
    public struct AdmissionChallenge { public let nonce: Data; public let wire: Data }

    /// A record write signed by a hybrid Ed25519 + ML-DSA-65 account (version 2 envelope).
    public static func signRecordTxHybrid(chainID: UInt64, nonce: UInt64, key: Data, op: RecordOp,
                                          value: Data = Data(), publicKey: Data, secret: Data) throws -> Data {
        ensureReady()
        return try consume(key.withC { kp, _ in value.withC { vp, vl in publicKey.withC { pp, pl in secret.withC { sp, sl in
            aviary_record_tx_sign_hybrid(chainID, nonce, kp, op.rawValue, vp, vl, pp, pl, sp, sl) } } } },
            "record_tx_sign_hybrid")
    }

    /// A peer record's fields.
    public struct PeerRecordInfo {
        public let id: UInt32; public let host: String; public let port: UInt16
        public let kemPublic: Data; public let sigPublic: Data; public let prekey: Data; public let signature: Data
    }

    /// A record for validator `id` at host:port signed with `identity`'s hybrid signature key.
    public static func signPeerRecord(id: UInt32, host: String, port: UInt16, identity: SecureIdentity,
                                      prekey: Data = Data()) throws -> Data {
        ensureReady()
        let kem = identity.kem.publicKey, sp = identity.sig.publicKey, ss = identity.sig.secret
        return try consume(kem.withC { kp, kl in sp.withC { pp, pl in prekey.withC { rp, rl in ss.withC { sk, sl in
            aviary_peer_record_sign(id, host, port, kp, kl, pp, pl, rp, rl, sk, sl) } } } }, "peer_record_sign")
    }

    public static func verifyPeerRecord(_ record: Data) -> Bool {
        ensureReady()
        return record.withC { aviary_peer_record_verify($0, $1) } == 1
    }

    public static func readPeerRecord(_ record: Data) throws -> PeerRecordInfo {
        ensureReady()
        let f = splitBlobList(try consume(record.withC { aviary_peer_record_fields($0, $1) }, "peer_record_fields"))
        guard f.count == 7, f[0].count == 4, f[2].count == 2 else { throw AviaryError(message: "peer record: malformed fields") }
        let be = { (d: Data) in d.reduce(UInt64(0)) { $0 << 8 | UInt64($1) } }
        return PeerRecordInfo(id: UInt32(be(f[0])), host: String(decoding: f[1], as: UTF8.self), port: UInt16(be(f[2])),
                              kemPublic: f[3], sigPublic: f[4], prekey: f[5], signature: f[6])
    }

    /// The peer records a node knows. `add` is the trusted local path; `merge` verifies every
    /// record and never overwrites a known id.
    public final class AddressBook {
        private let h: UnsafeMutableRawPointer
        /// An empty book, or one decoded from `encoded` (verified only when merged).
        public init(encoded: Data? = nil) throws {
            Aviary.ensureReady()
            let raw: UnsafeMutableRawPointer?
            if let encoded { raw = encoded.withC { aviary_address_book_decode($0, $1) } } else { raw = aviary_address_book_new() }
            guard let raw else { throw AviaryError(message: "address book: malformed encoding") }
            h = raw
        }
        deinit { aviary_address_book_free(h) }
        public func add(_ record: Data) throws {
            if record.withC({ aviary_address_book_add(h, $0, $1) }) != 0 { throw AviaryError(message: "address book: malformed record") }
        }
        /// Learns the verified records of an encoded book that are not yet known; returns how many.
        @discardableResult
        public func merge(_ encoded: Data) throws -> Int {
            let n = encoded.withC { aviary_address_book_merge(h, $0, $1) }
            guard n >= 0 else { throw AviaryError(message: "address book: malformed book") }
            return Int(n)
        }
        public func record(id: UInt32) throws -> Data? {
            contains(id) ? try consume(aviary_address_book_get(h, id), "address_book_get") : nil
        }
        public func contains(_ id: UInt32) -> Bool { aviary_address_book_contains(h, id) == 1 }
        public var count: Int { Int(aviary_address_book_size(h)) }
        public func records() throws -> [Data] { Aviary.splitBlobList(try consume(aviary_address_book_records(h), "address_book_records")) }
        public func encode() throws -> Data { try consume(aviary_address_book_encode(h), "address_book_encode") }
    }

    /// A Noise identity: the X25519 static pair and the hybrid signature pair.
    public struct NoiseIdentity { public let noiseStatic: PortableAccount; public let sig: PortableAccount }

    public static func generateNoiseIdentity() throws -> NoiseIdentity {
        ensureReady()
        let p = splitBlobList(try consume(aviary_noise_identity_generate(), "noise_identity_generate"))
        return NoiseIdentity(noiseStatic: PortableAccount(publicKey: p[0], secret: p[1]),
                             sig: PortableAccount(publicKey: p[2], secret: p[3]))
    }

    static func noiseBlob(_ me: NoiseIdentity) -> Data {
        blobList([me.noiseStatic.publicKey, me.noiseStatic.secret, me.sig.publicKey, me.sig.secret])
    }

    /// A Noise XX link with a post quantum identity proof; confidentiality is X25519. Pin
    /// `peerIdentity` against the keys you expect.
    public final class NoiseSession {
        private let h: UnsafeMutableRawPointer
        init(handle: UnsafeMutableRawPointer) { h = handle }
        deinit { aviary_noise_free(h) }
        private func check(_ rc: Int32, _ what: String) throws { if rc != 0 { throw AviaryError(message: "noise: \(what)") } }
        public func handshakeMessage() throws -> Data { try consume(aviary_noise_handshake_message(h), "noise_handshake_message") }
        public func completeInitiator(_ msg1: Data) throws -> Data {
            try consume(msg1.withC { aviary_noise_complete_initiator(h, $0, $1) }, "noise_complete_initiator")
        }
        public func completeResponder(_ msg2: Data) throws { try check(msg2.withC { aviary_noise_complete_responder(h, $0, $1) }, "handshake refused") }
        public func identityProof() throws -> Data { try consume(aviary_noise_identity_proof(h), "noise_identity_proof") }
        public func verifyIdentityProof(_ proof: Data) throws {
            try check(proof.withC { aviary_noise_verify_identity_proof(h, $0, $1) }, "identity proof refused")
        }
        public var isOpen: Bool { aviary_noise_open(h) == 1 }
        public func peerIdentity() throws -> Data { try consume(aviary_noise_peer_identity(h), "noise_peer_identity") }
        public func peerStatic() throws -> Data { try consume(aviary_noise_peer_static(h), "noise_peer_static") }
        public func handshakeHash() throws -> Data { try consume(aviary_noise_handshake_hash(h), "noise_handshake_hash") }
        public func encrypt(_ plaintext: Data) throws -> Data { try consume(plaintext.withC { aviary_noise_encrypt(h, $0, $1) }, "noise_encrypt") }
        public func decrypt(_ record: Data) throws -> Data { try consume(record.withC { aviary_noise_decrypt(h, $0, $1) }, "noise_decrypt") }
    }

    /// The dialing side of a Noise link: its handshake message is msg0.
    public static func initiateNoiseSession(me: NoiseIdentity, prologue: Data = Data()) throws -> NoiseSession {
        ensureReady()
        let id = noiseBlob(me)
        guard let raw = id.withC({ ip, il in prologue.withC { pp, pl in aviary_noise_initiate(ip, il, pp, pl) } }) else {
            throw AviaryError(message: "noise: initiate refused") }
        return NoiseSession(handle: raw)
    }

    /// The accepting side, from msg0: its handshake message is msg1.
    public static func acceptNoiseSession(me: NoiseIdentity, msg0: Data, prologue: Data = Data()) throws -> NoiseSession {
        ensureReady()
        let id = noiseBlob(me)
        guard let raw = id.withC({ ip, il in msg0.withC { mp, ml in prologue.withC { pp, pl in
            aviary_noise_accept(ip, il, mp, ml, pp, pl) } } }) else { throw AviaryError(message: "noise: accept refused") }
        return NoiseSession(handle: raw)
    }

    /// A deterministic in-memory mesh of NetNodes for tests, simulations and single process
    /// networks. Relays go in emission order; a timeout fires only when none is pending,
    /// earliest round and step first. A node's index is its mesh id and must match its `me`.
    public final class PeerMesh {
        public private(set) var nodes: [NetNode]
        private var queue: [(from: UInt32, to: Int, payload: Data)] = []
        private var timeouts: [(round: UInt32, step: UInt8, node: Int)] = []
        public init(_ nodes: [NetNode]) { self.nodes = nodes }
        public func append(_ node: NetNode) { nodes.append(node) }
        public func link(_ a: Int, _ b: Int) { nodes[a].addPeer(UInt32(b)); nodes[b].addPeer(UInt32(a)) }
        public func linkAll() { for a in nodes.indices { for b in nodes.indices where b > a { link(a, b) } } }
        public func apply(_ node: Int, _ o: Outbound) {
            for r in o.relays { queue.append((UInt32(node), Int(r.to), r.message)) }
            for t in o.timeouts { timeouts.append((t.round, t.step.rawValue, node)) }
        }
        public func start() throws { for (i, n) in nodes.enumerated() { apply(i, try n.start()) } }
        public func advance() throws { for (i, n) in nodes.enumerated() { apply(i, try n.advance()) } }
        /// Delivers the next relay; false when none is pending.
        @discardableResult
        public func deliver() throws -> Bool {
            guard !queue.isEmpty else { return false }
            let m = queue.removeFirst()
            apply(m.to, try nodes[m.to].onGossip(from: m.from, payload: m.payload))
            return true
        }
        /// Delivers one relay, or fires the earliest armed timeout when none is pending.
        @discardableResult
        public func step() throws -> Bool {
            if try deliver() { return true }
            guard !timeouts.isEmpty else { return false }
            timeouts.sort { ($0.round, $0.step, $0.node) < ($1.round, $1.step, $1.node) }
            let t = timeouts.removeFirst()
            let request = TimeoutRequest(step: ConsensusStep(rawValue: t.step) ?? .propose, round: t.round)
            apply(t.node, try nodes[t.node].onTimeout(request))
            return true
        }
        /// Steps until `until` holds or `budget` steps passed; returns whether it held.
        public func run(until: () -> Bool, budget: Int = 20000) throws -> Bool {
            var i = 0
            while i < budget && !until() { if try !step() { break }; i += 1 }
            return until()
        }
        /// Runs until the given validators (all by default) committed, then drains the relays;
        /// armed timeouts are left alone.
        @discardableResult
        public func settle(validators: [Int]? = nil) throws -> Bool {
            let v = validators ?? Array(nodes.indices)
            let ok = try run(until: { v.allSatisfy { nodes[$0].committed } })
            while try deliver() {}
            return ok
        }
    }
}

extension Aviary.CredentialPolicy {
    /// The answer to an admission challenge `wire` from a peer holding `credential` under
    /// this policy.
    public func admissionAnswer(credential: Data, wire: Data) throws -> Data {
        try consume(encoded.withC { ep, el in credential.withC { cp, cl in wire.withC { wp, wl in
            aviary_admission_answer(ep, el, cp, cl, wp, wl) } } }, "admission_answer")
    }
}
