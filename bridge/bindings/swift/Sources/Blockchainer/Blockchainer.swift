// Blockchainer — a Swift API over the Blockchainer C ABI.
//
// The native library initializes lazily and synchronously on first use, so
// ledger/interop calls never require `await`. `Blockchainer.preload()` is an
// OPTIONAL warm-up that runs the one-time crypto-backend init on a background
// queue (off the main thread), so the first call on the main thread is
// effectively free. The API works identically whether or not you preload.
import BlockchainerC
import Foundation

public struct BlockchainerError: Error, CustomStringConvertible {
    public let message: String
    public var description: String { "BlockchainerError: \(message)" }
}

public enum Blockchainer {
    // Swift guarantees a `static let` initializer runs exactly once and is
    // thread-safe, so this is the one-time init regardless of which thread
    // (main lazy path or the preload background queue) reaches it first.
    private static let initialized: Bool = (blockchainer_init() == 0)

    /// Ensure the native backend is initialized (synchronous, idempotent).
    @discardableResult
    public static func ensureReady() -> Bool { initialized }

    /// Optionally warm the native backend OFF the main thread.
    ///
    /// Runs the one-time init on a background queue; because it is process-
    /// global, the main thread's later synchronous calls pay no init cost. Fire-
    /// and-forget — you never need to wait on it. `completion` (if given) runs on
    /// the main queue once warm.
    public static func preload(completion: (() -> Void)? = nil) {
        DispatchQueue.global(qos: .userInitiated).async {
            let ok = ensureReady()
            if let completion { DispatchQueue.main.async { _ = ok; completion() } }
        }
    }

    public static var version: String {
        ensureReady(); return String(cString: blockchainer_version())
    }
    public static var cryptoBackendVersion: String {
        ensureReady(); return String(cString: blockchainer_crypto_backend_version())
    }

    /// BLAKE3-256 digest.
    public static func hash(_ msg: Data) throws -> Data {
        ensureReady()
        return try consume(msg.withC { blockchainer_hash($0, $1) }, "hash")
    }

    /// Verify a state-inclusion proof against a 32-byte state root.
    public static func verifyStateProof(stateRoot: Data, proof: Data) -> Bool {
        ensureReady()
        return stateRoot.withC { rootPtr, _ in
            proof.withC { proofPtr, proofLen in
                blockchainer_verify_state_proof(rootPtr, proofPtr, proofLen) == 1
            }
        }
    }

    // ── Interop address derivation ─────────────────────────────────────────────
    public static func ethAddress(secret32: Data) throws -> String {
        ensureReady()
        return try consumeString(secret32.withC { p, _ in blockchainer_eth_address(p) }, "eth_address")
    }
    public static func btcAddress(secret32: Data) throws -> String {
        ensureReady()
        return try consumeString(secret32.withC { p, _ in blockchainer_btc_address(p) }, "btc_address")
    }
    public static func solAddress(edSecret64: Data) throws -> String {
        ensureReady()
        return try consumeString(edSecret64.withC { p, _ in blockchainer_sol_address(p) }, "sol_address")
    }

    // ── Portable account export (tiered encrypted backup) ──────────────────────

    /// A hybrid-KEM device keypair for the recipient-bound export tiers.
    public static func hybridKemKeypair() throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(blockchainer_hybrid_kem_keygen(), "hybrid_kem_keygen"))
    }

    /// Tier 1 — seal an account under a passphrase (MolecularVault + Argon2id;
    /// ops=mem=0 → SENSITIVE preset).
    public static func sealAccountPassphrase(publicKey: Data, secret: Data, passphrase: String,
                                             ops: UInt64 = 0, mem: Int = 0) throws -> Data {
        ensureReady()
        return try consume(publicKey.withC { pp, pl in
            secret.withC { sp, sl in
                blockchainer_portable_seal_passphrase(pp, pl, sp, sl, passphrase, ops, mem)
            }
        }, "portable_seal_passphrase")
    }
    public static func openAccountPassphrase(envelope: Data, passphrase: String) throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(envelope.withC { ep, el in
            blockchainer_portable_open_passphrase(ep, el, passphrase)
        }, "portable_open_passphrase"))
    }

    /// Tier 2 — recipient-bound (PQ): only the device's KEM secret opens it.
    public static func sealAccountTo(publicKey: Data, secret: Data, recipientKemPublic: Data) throws -> Data {
        ensureReady()
        return try consume(publicKey.withC { pp, pl in
            secret.withC { sp, sl in
                recipientKemPublic.withC { rp, rl in
                    blockchainer_portable_seal_to(pp, pl, sp, sl, rp, rl)
                }
            }
        }, "portable_seal_to")
    }
    public static func openAccountTo(envelope: Data, recipientKemSecret: Data) throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(envelope.withC { ep, el in
            recipientKemSecret.withC { rp, rl in blockchainer_portable_open_to(ep, el, rp, rl) }
        }, "portable_open_to"))
    }

    /// Tier 3 — signed-recipient (Fortress-grade): recipient-bound AND the
    /// receiver verifies the owner's hybrid signature.
    public static func sealAccountSignedTo(publicKey: Data, secret: Data,
                                           recipientKemPublic: Data, senderSigSecret: Data) throws -> Data {
        ensureReady()
        return try consume(publicKey.withC { pp, pl in
            secret.withC { sp, sl in
                recipientKemPublic.withC { rp, rl in
                    senderSigSecret.withC { ssp, ssl in
                        blockchainer_portable_seal_signed_to(pp, pl, sp, sl, rp, rl, ssp, ssl)
                    }
                }
            }
        }, "portable_seal_signed_to")
    }
    public static func openAccountSignedFrom(envelope: Data, recipientKemSecret: Data,
                                             senderSigPublic: Data) throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(envelope.withC { ep, el in
            recipientKemSecret.withC { rp, rl in
                senderSigPublic.withC { spp, spl in
                    blockchainer_portable_open_signed_from(ep, el, rp, rl, spp, spl)
                }
            }
        }, "portable_open_signed_from"))
    }

    // ── Threshold (FROST t-of-n) accounts ──────────────────────────────────────

    /// A participant's share of a threshold account (opaque bytes + identity fields).
    public struct ThresholdShare {
        public let bytes: Data
        public let index: UInt16
        public let groupPublicKey: Data
        public let publicShare: Data
    }

    /// Deal a t-of-n account: n share cards, any t of which can sign. The dealt
    /// group secret is zeroised before returning.
    public static func createThresholdAccount(n: UInt16, t: UInt16) throws -> [ThresholdShare] {
        ensureReady()
        let blob = try consume(blockchainer_threshold_create(n, t), "threshold_create")
        var out: [ThresholdShare] = []
        let count = Int(blob[0]) << 8 | Int(blob[1])
        var off = 2
        for _ in 0..<count {
            let len = Int(blob[off]) << 24 | Int(blob[off+1]) << 16 | Int(blob[off+2]) << 8 | Int(blob[off+3])
            off += 4
            let card = blob.subdata(in: off..<(off + len))
            off += len
            let info = try consume(card.withC { blockchainer_threshold_share_info($0, $1) }, "threshold_share_info")
            out.append(ThresholdShare(bytes: card, index: UInt16(info[0]) << 8 | UInt16(info[1]),
                                      groupPublicKey: info.subdata(in: 2..<34), publicShare: info.subdata(in: 34..<66)))
        }
        return out
    }

    /// Seal a share card for rest (ops=mem=0 → SENSITIVE preset).
    public static func sealShare(card: Data, passphrase: String, ops: UInt64 = 0, mem: Int = 0) throws -> Data {
        ensureReady()
        return try consume(card.withC { cp, cl in
            blockchainer_threshold_share_seal(cp, cl, passphrase, ops, mem)
        }, "threshold_share_seal")
    }
    public static func unsealShare(envelope: Data, passphrase: String) throws -> Data {
        ensureReady()
        return try consume(envelope.withC { ep, el in
            blockchainer_threshold_share_unseal(ep, el, passphrase)
        }, "threshold_share_unseal")
    }

    /// Round 1 for a card: (secret nonces, public commitment).
    public static func commit(card: Data) throws -> (nonces: Data, commitment: Data) {
        ensureReady()
        let b = try consume(card.withC { blockchainer_threshold_commit($0, $1) }, "threshold_commit")
        return (b.subdata(in: 0..<64), b.subdata(in: 64..<130))
    }

    /// A two-round FROST signing ceremony over a fixed message + roster.
    public final class Ceremony {
        private let h: OpaquePointer
        init(groupPK: Data, message: Data, commitments: [Data]) throws {
            var roster = Data()
            for c in commitments { roster.append(c) }
            let raw: UnsafeMutableRawPointer? = groupPK.withC { gp, _ in
                message.withC { mp, ml in
                    roster.withC { rp, rl in
                        blockchainer_threshold_ceremony_new(gp, mp, ml, rp, rl)
                    }
                }
            }
            guard let raw else { throw BlockchainerError(message: "threshold ceremony creation failed") }
            h = OpaquePointer(raw)
        }
        deinit { blockchainer_threshold_ceremony_free(UnsafeMutableRawPointer(h)) }

        /// Round 2 for a card using its round-1 nonces: a 32-byte share.
        public func sign(card: Data, nonces: Data) throws -> Data {
            try consume(card.withC { cp, cl in nonces.withC { np, _ in
                blockchainer_threshold_sign(UnsafeMutableRawPointer(h), cp, cl, np) } }, "threshold_sign")
        }
        /// Attribute a share to participant [id] (public share 32 B).
        public func verifyShare(id: UInt16, publicShare: Data, sigShare: Data) -> Bool {
            publicShare.withC { pp, _ in sigShare.withC { sp, _ in
                blockchainer_threshold_verify_share(UnsafeMutableRawPointer(h), id, pp, sp) == 1 } }
        }
        /// Aggregate (id → 32-byte share) pairs into a 64-byte Ed25519 signature.
        public func aggregate(ids: [UInt16], sigShares: [Data]) throws -> Data {
            var flat = Data(); for s in sigShares { flat.append(s) }
            let res: BlockchainerBufferResult = ids.withUnsafeBufferPointer { idp in
                flat.withC { fp, _ in
                    blockchainer_threshold_aggregate(UnsafeMutableRawPointer(h), idp.baseAddress, fp, ids.count)
                }
            }
            return try consume(res, "threshold_aggregate")
        }
    }
    public static func makeCeremony(groupPK: Data, message: Data, commitments: [Data]) throws -> Ceremony {
        ensureReady()
        return try Ceremony(groupPK: groupPK, message: message, commitments: commitments)
    }

    // ── AssuranceProfile + SealedMempool ───────────────────────────────────────

    /// The tiers a security posture mandates at every seam.
    public struct AssuranceProfile {
        public let ratcheted: Bool
        public let vrf: Bool
        public let thresholdCustody: Bool
        public let sealedMempool: Bool
        public let sigAlgos: [UInt8]
    }

    /// Describe a posture: 1 = standard, 2 = hardened, 3 = fortress.
    public static func describeAssurance(level: UInt16) throws -> AssuranceProfile {
        ensureReady()
        let b = try consume(blockchainer_assurance_describe(level), "assurance_describe")
        let n = Int(b[4])
        return AssuranceProfile(ratcheted: b[0] == 1, vrf: b[1] == 1, thresholdCustody: b[2] == 1,
                                sealedMempool: b[3] == 1, sigAlgos: [UInt8](b.subdata(in: 5..<(5 + n))))
    }

    /// Encode a transaction from its fields (e.g. to seal into a SealedMempool).
    public static func encodeTx(version: UInt8 = 2, chainID: UInt64, sigAlgo: UInt8 = 1, fee: UInt64,
                                body: Data, author: Data, signature: Data) throws -> Data {
        ensureReady()
        return try consume(body.withC { bp, bl in author.withC { ap, al in signature.withC { sp, sl in
            blockchainer_tx_encode(version, chainID, sigAlgo, fee, bp, bl, ap, al, sp, sl) } } }, "tx_encode")
    }
    /// Seal an encoded tx to the proposer's hybrid-KEM public key.
    public static func sealTx(txEncoded: Data, proposerKemPublic: Data) throws -> Data {
        ensureReady()
        return try consume(txEncoded.withC { tp, tl in proposerKemPublic.withC { pp, pl in
            blockchainer_sealed_tx_seal(tp, tl, pp, pl) } }, "sealed_tx_seal")
    }

    /// An encrypted mempool: submissions stay sealed to the proposer until reveal.
    public final class SealedMempool {
        private let h: OpaquePointer
        init(capacity: Int) {
            let raw = blockchainer_sealed_mempool_new(capacity)!
            h = OpaquePointer(raw)
        }
        deinit { blockchainer_sealed_mempool_free(UnsafeMutableRawPointer(h)) }

        public func add(_ sealedTx: Data) -> Bool {
            sealedTx.withC { sp, sl in
                blockchainer_sealed_mempool_add(UnsafeMutableRawPointer(h), sp, sl) == 1 }
        }
        public var size: Int { blockchainer_sealed_mempool_size(UnsafeMutableRawPointer(h)) }
        public func reveal(proposerKemSecret: Data, max: Int) throws -> [Data] {
            let blob = try consume(proposerKemSecret.withC { sp, sl in
                blockchainer_sealed_mempool_reveal(UnsafeMutableRawPointer(h), sp, sl, max) }, "sealed_mempool_reveal")
            var out: [Data] = []
            let count = Int(blob[0]) << 24 | Int(blob[1]) << 16 | Int(blob[2]) << 8 | Int(blob[3])
            var off = 4
            for _ in 0..<count {
                let len = Int(blob[off]) << 24 | Int(blob[off+1]) << 16 | Int(blob[off+2]) << 8 | Int(blob[off+3])
                off += 4
                out.append(blob.subdata(in: off..<(off + len)))
                off += len
            }
            return out
        }
    }
    public static func makeSealedMempool(capacity: Int = 4096) -> SealedMempool {
        ensureReady()
        return SealedMempool(capacity: capacity)
    }

    /// Create a new in-memory single-validator chain.
    public static func makeNode(chainID: UInt64) throws -> Node {
        ensureReady()
        guard let h = blockchainer_node_new(chainID) else {
            throw BlockchainerError(message: "node creation failed")
        }
        return Node(handle: h)
    }

    /// Create a custodial key store protected by [passphrase] (returned unlocked).
    public static func makeKeyStore(passphrase: String) throws -> KeyStore {
        ensureReady()
        guard let h = blockchainer_keystore_new(passphrase) else {
            throw BlockchainerError(message: "keystore creation failed")
        }
        return KeyStore(handle: h)
    }

    /// Load a serialised key store. It comes back LOCKED — call `unlock`.
    public static func loadKeyStore(_ blob: Data) throws -> KeyStore {
        ensureReady()
        guard let h = blob.withC({ p, l in blockchainer_keystore_load(p, l) }) else {
            throw BlockchainerError(message: "keystore load failed")
        }
        return KeyStore(handle: h)
    }
}

/// A custodial store that seals account signing secrets at rest. Add accounts,
/// serialise the (encrypted) blob, reload it locked, unlock, then reveal a secret
/// only to sign.
public final class KeyStore {
    private let handle: OpaquePointer
    init(handle: UnsafeMutableRawPointer) { self.handle = OpaquePointer(handle) }
    deinit { blockchainer_keystore_free(UnsafeMutableRawPointer(handle)) }

    private var h: UnsafeMutableRawPointer { UnsafeMutableRawPointer(handle) }

    /// Seal an account's [secret]; returns its 32-byte address. Requires unlocked.
    public func addAccount(publicKey: Data, secret: Data) throws -> Data {
        var address = Data(count: 32)
        let rc = publicKey.withC { pp, pl in
            secret.withC { sp, sl in
                address.withUnsafeMutableBytes { ap in
                    blockchainer_keystore_add_account(h, pp, pl, sp, sl,
                                                      ap.bindMemory(to: UInt8.self).baseAddress!)
                }
            }
        }
        if rc != 0 { throw BlockchainerError(message: "keystore add account failed (locked?)") }
        return address
    }

    /// Decrypt the sealed secret for [address]. Requires unlocked.
    public func reveal(address: Data) throws -> Data {
        try consume(address.withC { p, _ in blockchainer_keystore_reveal(h, p) }, "keystore_reveal")
    }

    /// The persistable (encrypted) blob — no plaintext key.
    public func serialise() throws -> Data {
        try consume(blockchainer_keystore_serialise(h), "keystore_serialise")
    }

    /// Recover the master key from [passphrase]. Throws on a wrong passphrase.
    public func unlock(passphrase: String) throws {
        if blockchainer_keystore_unlock(h, passphrase) != 0 {
            throw BlockchainerError(message: "keystore unlock failed")
        }
    }

    /// Whether secrets can currently be revealed.
    public var unlocked: Bool { blockchainer_keystore_unlocked(h) == 1 }
}

/// A live in-memory chain node (ledger + BFT + mempool + observability).
public final class Node {
    private let handle: OpaquePointer
    init(handle: UnsafeMutableRawPointer) { self.handle = OpaquePointer(handle) }
    deinit { blockchainer_node_free(UnsafeMutableRawPointer(handle)) }

    private var h: UnsafeMutableRawPointer { UnsafeMutableRawPointer(handle) }

    public struct Account { public let secret: Data; public let publicKey: Data; public let address: Data }

    public func newAccount() throws -> Account {
        let secret = try consume(blockchainer_node_new_account(h), "new_account")
        let publicKey = secret.subdata(in: 32..<64)
        let address = try consume(publicKey.withC { blockchainer_account_address($0, $1) }, "account_address")
        return Account(secret: secret, publicKey: publicKey, address: address)
    }

    /// Credit an address at genesis. Valid until the first block is produced
    /// (reads do NOT seal genesis); throws afterwards — never a silent no-op.
    public func credit(address: Data, amount: UInt64) throws {
        let rc = address.withC { p, _ in blockchainer_node_credit(h, p, amount) }
        if rc == 2 {
            throw BlockchainerError(
                message: "genesis is sealed (a block was produced); fund accounts with transfers")
        }
        if rc != 0 { throw BlockchainerError(message: "credit failed (status \(rc))") }
    }

    @discardableResult
    public func submitTransfer(secret: Data, to: Data, amount: UInt64, nonce: UInt64) throws -> Data {
        try consume(secret.withC { sp, sl in
            to.withC { tp, _ in blockchainer_node_submit_transfer(h, sp, sl, tp, amount, nonce) }
        }, "submit_transfer")
    }

    /// Submit an Ed25519 transfer carrying a priority-fee `fee` (version-2 envelope).
    @discardableResult
    public func submitTransferFee(secret: Data, to: Data, amount: UInt64, nonce: UInt64,
                                  fee: UInt64) throws -> Data {
        try consume(secret.withC { sp, sl in
            to.withC { tp, _ in blockchainer_node_submit_transfer_fee(h, sp, sl, tp, amount, nonce, fee) }
        }, "submit_transfer_fee")
    }

    /// Generate a fresh hybrid (Ed25519 + ML-DSA-65) post-quantum account.
    public func newAccountHybrid() throws -> Account {
        let blob = try consume(blockchainer_node_new_account_hybrid(h), "new_account_hybrid")
        // [pub_len: 4 bytes big-endian][public][secret]
        let pl = Int(blob[0]) << 24 | Int(blob[1]) << 16 | Int(blob[2]) << 8 | Int(blob[3])
        let publicKey = blob.subdata(in: 4..<(4 + pl))
        let secret = blob.subdata(in: (4 + pl)..<blob.count)
        let address = try consume(publicKey.withC { blockchainer_account_address($0, $1) }, "account_address")
        return Account(secret: secret, publicKey: publicKey, address: address)
    }

    /// Build, sign, and admit a hybrid (post-quantum) transfer.
    @discardableResult
    public func submitTransferHybrid(secret: Data, publicKey: Data, to: Data,
                                     amount: UInt64, nonce: UInt64) throws -> Data {
        try consume(secret.withC { sp, sl in
            publicKey.withC { pp, pl in
                to.withC { tp, _ in
                    blockchainer_node_submit_transfer_hybrid(h, sp, sl, pp, pl, tp, amount, nonce)
                }
            }
        }, "submit_transfer_hybrid")
    }

    @discardableResult
    public func produceBlock(timestamp: UInt64) throws -> UInt64 {
        var height: UInt64 = 0
        if blockchainer_node_produce_block(h, timestamp, &height) != 0 {
            throw BlockchainerError(message: "produce_block failed")
        }
        return height
    }

    public var height: UInt64 { blockchainer_node_height(h) }
    public var mempoolSize: UInt64 { blockchainer_node_mempool_size(h) }
    public func balance(address: Data) -> UInt64 { address.withC { p, _ in blockchainer_node_balance(h, p) } }
    public func nonce(address: Data) -> UInt64 { address.withC { p, _ in blockchainer_node_nonce(h, p) } }
    public func stateRoot() throws -> Data { try consume(blockchainer_node_state_root(h), "state_root") }
    public func drainEvents() throws -> String { String(decoding: try consume(blockchainer_node_drain_events(h), "drain_events"), as: UTF8.self) }
    public func metrics() throws -> String { String(decoding: try consume(blockchainer_node_metrics(h), "metrics"), as: UTF8.self) }
    public func proveAccount(address: Data) throws -> Data {
        try consume(address.withC { p, _ in blockchainer_node_prove_account(h, p) }, "prove_account")
    }
}

/// A signing account's material — public key and secret — as returned by the
/// portable-account "open" calls (and the device key from `hybridKemKeypair`).
public struct PortableAccount {
    public let publicKey: Data
    public let secret: Data
}

// Unpack the C ABI account blob [pub_len:4 BE][public][secret].
private func splitAccountBlob(_ blob: Data) -> PortableAccount {
    let pl = Int(blob[0]) << 24 | Int(blob[1]) << 16 | Int(blob[2]) << 8 | Int(blob[3])
    return PortableAccount(publicKey: blob.subdata(in: 4..<(4 + pl)),
                           secret: blob.subdata(in: (4 + pl)..<blob.count))
}

// ── Buffer marshalling helpers ─────────────────────────────────────────────────

private extension Data {
    /// Call a C function with (const uint8_t* ptr, size_t len) over these bytes.
    func withC<R>(_ body: (UnsafePointer<UInt8>, Int) -> R) -> R {
        if isEmpty {
            var dummy: UInt8 = 0
            return withUnsafePointer(to: &dummy) { body($0, 0) }
        }
        return withUnsafeBytes { raw in body(raw.bindMemory(to: UInt8.self).baseAddress!, count) }
    }
}

private func consume(_ result: BlockchainerBufferResult, _ op: String) throws -> Data {
    var r = result
    if r.buf.data == nil {
        if let e = r.error { let m = String(cString: e); blockchainer_str_free(e); throw BlockchainerError(message: m) }
        throw BlockchainerError(message: "\(op) failed")
    }
    let data = Data(bytes: r.buf.data, count: r.buf.len)
    blockchainer_buffer_free(&r.buf)
    return data
}

private func consumeString(_ result: BlockchainerBufferResult, _ op: String) throws -> String {
    String(decoding: try consume(result, op), as: UTF8.self)
}
