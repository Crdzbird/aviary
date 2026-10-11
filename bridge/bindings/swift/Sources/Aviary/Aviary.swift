// Aviary - a Swift API over the Aviary C ABI.
//
// The native library initializes lazily and synchronously on first use, so
// ledger/interop calls never require `await`. `Aviary.preload()` is an
// OPTIONAL warm-up that runs the one-time crypto-backend init on a background
// queue (off the main thread), so the first call on the main thread is
// effectively free. The API works identically whether or not you preload.
import AviaryC
import Foundation

public struct AviaryError: Error, CustomStringConvertible {
    public let message: String
    public var description: String { "AviaryError: \(message)" }
}

public enum Aviary {
    // Swift guarantees a `static let` initializer runs exactly once and is
    // thread-safe, so this is the one-time init regardless of which thread
    // (main lazy path or the preload background queue) reaches it first.
    private static let initialized: Bool = (aviary_init() == 0)

    /// Ensure the native backend is initialized (synchronous, idempotent).
    @discardableResult
    public static func ensureReady() -> Bool { initialized }

    /// Optionally warm the native backend OFF the main thread.
    ///
    /// Runs the one-time init on a background queue; because it is process-
    /// global, the main thread's later synchronous calls pay no init cost. Fire-
    /// and-forget - you never need to wait on it. `completion` (if given) runs on
    /// the main queue once warm.
    public static func preload(completion: (() -> Void)? = nil) {
        DispatchQueue.global(qos: .userInitiated).async {
            let ok = ensureReady()
            if let completion { DispatchQueue.main.async { _ = ok; completion() } }
        }
    }

    public static var version: String {
        ensureReady(); return String(cString: aviary_version())
    }
    public static var cryptoBackendVersion: String {
        ensureReady(); return String(cString: aviary_crypto_backend_version())
    }

    /// BLAKE3-256 digest.
    public static func hash(_ msg: Data) throws -> Data {
        ensureReady()
        return try consume(msg.withC { aviary_hash($0, $1) }, "hash")
    }

    /// Verify a state-inclusion proof against a 32-byte state root.
    public static func verifyStateProof(stateRoot: Data, proof: Data) -> Bool {
        ensureReady()
        return stateRoot.withC { rootPtr, _ in
            proof.withC { proofPtr, proofLen in
                aviary_verify_state_proof(rootPtr, proofPtr, proofLen) == 1
            }
        }
    }

    // -- Interop address derivation ---------------------------------------------
    public static func ethAddress(secret32: Data) throws -> String {
        ensureReady()
        return try consumeString(secret32.withC { p, _ in aviary_eth_address(p) }, "eth_address")
    }
    public static func btcAddress(secret32: Data) throws -> String {
        ensureReady()
        return try consumeString(secret32.withC { p, _ in aviary_btc_address(p) }, "btc_address")
    }
    public static func solAddress(edSecret64: Data) throws -> String {
        ensureReady()
        return try consumeString(edSecret64.withC { p, _ in aviary_sol_address(p) }, "sol_address")
    }

    // -- Portable account export (tiered encrypted backup) ----------------------

    /// A hybrid-KEM device keypair for the recipient-bound export tiers.
    public static func hybridKemKeypair() throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(aviary_hybrid_kem_keygen(), "hybrid_kem_keygen"))
    }

    /// Tier 1 - seal an account under a passphrase (MolecularVault + Argon2id;
    /// ops=mem=0 -> SENSITIVE preset).
    public static func sealAccountPassphrase(publicKey: Data, secret: Data, passphrase: String,
                                             ops: UInt64 = 0, mem: Int = 0) throws -> Data {
        ensureReady()
        return try consume(publicKey.withC { pp, pl in
            secret.withC { sp, sl in
                aviary_portable_seal_passphrase(pp, pl, sp, sl, passphrase, ops, mem)
            }
        }, "portable_seal_passphrase")
    }
    public static func openAccountPassphrase(envelope: Data, passphrase: String) throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(envelope.withC { ep, el in
            aviary_portable_open_passphrase(ep, el, passphrase)
        }, "portable_open_passphrase"))
    }

    /// Tier 2 - recipient-bound (PQ): only the device's KEM secret opens it.
    public static func sealAccountTo(publicKey: Data, secret: Data, recipientKemPublic: Data) throws -> Data {
        ensureReady()
        return try consume(publicKey.withC { pp, pl in
            secret.withC { sp, sl in
                recipientKemPublic.withC { rp, rl in
                    aviary_portable_seal_to(pp, pl, sp, sl, rp, rl)
                }
            }
        }, "portable_seal_to")
    }
    public static func openAccountTo(envelope: Data, recipientKemSecret: Data) throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(envelope.withC { ep, el in
            recipientKemSecret.withC { rp, rl in aviary_portable_open_to(ep, el, rp, rl) }
        }, "portable_open_to"))
    }

    /// Tier 3 - signed-recipient (Fortress-grade): recipient-bound AND the
    /// receiver verifies the owner's hybrid signature.
    public static func sealAccountSignedTo(publicKey: Data, secret: Data,
                                           recipientKemPublic: Data, senderSigSecret: Data) throws -> Data {
        ensureReady()
        return try consume(publicKey.withC { pp, pl in
            secret.withC { sp, sl in
                recipientKemPublic.withC { rp, rl in
                    senderSigSecret.withC { ssp, ssl in
                        aviary_portable_seal_signed_to(pp, pl, sp, sl, rp, rl, ssp, ssl)
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
                    aviary_portable_open_signed_from(ep, el, rp, rl, spp, spl)
                }
            }
        }, "portable_open_signed_from"))
    }

    // -- Threshold (FROST t-of-n) accounts --------------------------------------

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
        let blob = try consume(aviary_threshold_create(n, t), "threshold_create")
        var out: [ThresholdShare] = []
        let count = Int(blob[0]) << 8 | Int(blob[1])
        var off = 2
        for _ in 0..<count {
            let len = Int(blob[off]) << 24 | Int(blob[off+1]) << 16 | Int(blob[off+2]) << 8 | Int(blob[off+3])
            off += 4
            let card = blob.subdata(in: off..<(off + len))
            off += len
            let info = try consume(card.withC { aviary_threshold_share_info($0, $1) }, "threshold_share_info")
            out.append(ThresholdShare(bytes: card, index: UInt16(info[0]) << 8 | UInt16(info[1]),
                                      groupPublicKey: info.subdata(in: 2..<34), publicShare: info.subdata(in: 34..<66)))
        }
        return out
    }

    /// Seal a share card for rest (ops=mem=0 -> SENSITIVE preset).
    public static func sealShare(card: Data, passphrase: String, ops: UInt64 = 0, mem: Int = 0) throws -> Data {
        ensureReady()
        return try consume(card.withC { cp, cl in
            aviary_threshold_share_seal(cp, cl, passphrase, ops, mem)
        }, "threshold_share_seal")
    }
    public static func unsealShare(envelope: Data, passphrase: String) throws -> Data {
        ensureReady()
        return try consume(envelope.withC { ep, el in
            aviary_threshold_share_unseal(ep, el, passphrase)
        }, "threshold_share_unseal")
    }

    /// Round 1 for a card: (secret nonces, public commitment).
    public static func commit(card: Data) throws -> (nonces: Data, commitment: Data) {
        ensureReady()
        let b = try consume(card.withC { aviary_threshold_commit($0, $1) }, "threshold_commit")
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
                        aviary_threshold_ceremony_new(gp, mp, ml, rp, rl)
                    }
                }
            }
            guard let raw else { throw AviaryError(message: "threshold ceremony creation failed") }
            h = OpaquePointer(raw)
        }
        deinit { aviary_threshold_ceremony_free(UnsafeMutableRawPointer(h)) }

        /// Round 2 for a card using its round-1 nonces: a 32-byte share.
        public func sign(card: Data, nonces: Data) throws -> Data {
            try consume(card.withC { cp, cl in nonces.withC { np, _ in
                aviary_threshold_sign(UnsafeMutableRawPointer(h), cp, cl, np) } }, "threshold_sign")
        }
        /// Attribute a share to participant [id] (public share 32 B).
        public func verifyShare(id: UInt16, publicShare: Data, sigShare: Data) -> Bool {
            publicShare.withC { pp, _ in sigShare.withC { sp, _ in
                aviary_threshold_verify_share(UnsafeMutableRawPointer(h), id, pp, sp) == 1 } }
        }
        /// Aggregate (id -> 32-byte share) pairs into a 64-byte Ed25519 signature.
        public func aggregate(ids: [UInt16], sigShares: [Data]) throws -> Data {
            var flat = Data(); for s in sigShares { flat.append(s) }
            let res: AviaryBufferResult = ids.withUnsafeBufferPointer { idp in
                flat.withC { fp, _ in
                    aviary_threshold_aggregate(UnsafeMutableRawPointer(h), idp.baseAddress, fp, ids.count)
                }
            }
            return try consume(res, "threshold_aggregate")
        }
    }
    public static func makeCeremony(groupPK: Data, message: Data, commitments: [Data]) throws -> Ceremony {
        ensureReady()
        return try Ceremony(groupPK: groupPK, message: message, commitments: commitments)
    }

    // -- AssuranceProfile + SealedMempool ---------------------------------------

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
        let b = try consume(aviary_assurance_describe(level), "assurance_describe")
        let n = Int(b[4])
        return AssuranceProfile(ratcheted: b[0] == 1, vrf: b[1] == 1, thresholdCustody: b[2] == 1,
                                sealedMempool: b[3] == 1, sigAlgos: [UInt8](b.subdata(in: 5..<(5 + n))))
    }

    /// Encode a transaction from its fields (e.g. to seal into a SealedMempool).
    public static func encodeTx(version: UInt8 = 2, chainID: UInt64, sigAlgo: UInt8 = 1, fee: UInt64,
                                body: Data, author: Data, signature: Data) throws -> Data {
        ensureReady()
        return try consume(body.withC { bp, bl in author.withC { ap, al in signature.withC { sp, sl in
            aviary_tx_encode(version, chainID, sigAlgo, fee, bp, bl, ap, al, sp, sl) } } }, "tx_encode")
    }
    /// Seal an encoded tx to the proposer's hybrid-KEM public key.
    public static func sealTx(txEncoded: Data, proposerKemPublic: Data) throws -> Data {
        ensureReady()
        return try consume(txEncoded.withC { tp, tl in proposerKemPublic.withC { pp, pl in
            aviary_sealed_tx_seal(tp, tl, pp, pl) } }, "sealed_tx_seal")
    }

    /// An encrypted mempool: submissions stay sealed to the proposer until reveal.
    public final class SealedMempool {
        private let h: OpaquePointer
        init(capacity: Int) {
            let raw = aviary_sealed_mempool_new(capacity)!
            h = OpaquePointer(raw)
        }
        deinit { aviary_sealed_mempool_free(UnsafeMutableRawPointer(h)) }

        public func add(_ sealedTx: Data) -> Bool {
            sealedTx.withC { sp, sl in
                aviary_sealed_mempool_add(UnsafeMutableRawPointer(h), sp, sl) == 1 }
        }
        public var size: Int { aviary_sealed_mempool_size(UnsafeMutableRawPointer(h)) }
        public func reveal(proposerKemSecret: Data, max: Int) throws -> [Data] {
            let blob = try consume(proposerKemSecret.withC { sp, sl in
                aviary_sealed_mempool_reveal(UnsafeMutableRawPointer(h), sp, sl, max) }, "sealed_mempool_reveal")
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

    // -- CredentialGate (anonymous membership admission, Option A) ---------------

    /// A BBS issuer keypair (public 96 B, secret 32 B) - the chain's issuer identity.
    public static func newBBSIssuer(keyMaterial: Data) throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(keyMaterial.withC { p, l in
            aviary_bbs_keygen(p, l) }, "bbs_keygen"))
    }

    /// A membership policy. Its canonical `encoded` bytes drive the gate; its
    /// `hash()` is meant to be committed into genesis (like an AssuranceProfile).
    /// Admission mode (section 3): unlinkable = Option A, pseudonymous = Option B.
    public enum CredentialMode: UInt8 { case unlinkable = 0, pseudonymous = 1 }

    public struct CredentialPolicy {
        public let encoded: Data
        public init(chainID: UInt64, issuerPublic: Data, header: Data,
                    attributeCount: UInt32 = 4, minTier: UInt8 = 0,
                    mode: CredentialMode = .unlinkable) throws {
            Aviary.ensureReady()
            if mode == .unlinkable {
                encoded = try consume(issuerPublic.withC { ip, il in header.withC { hp, hl in
                    aviary_credential_policy_encode(chainID, ip, il, hp, hl, attributeCount, minTier) } },
                    "credential_policy_encode")
            } else {
                encoded = try consume(issuerPublic.withC { ip, il in header.withC { hp, hl in
                    aviary_credential_policy_encode_ex(chainID, ip, il, hp, hl, attributeCount, minTier, mode.rawValue) } },
                    "credential_policy_encode_ex")
            }
        }
        /// The 32-byte commitment to commit into genesis.
        public func hash() throws -> Data {
            try consume(encoded.withC { p, l in aviary_credential_policy_hash(p, l) },
                        "credential_policy_hash")
        }
        /// Issuer side: assemble + BBS-sign a credential (holder-secret blob).
        public func issue(issuerSecret: Data, memberSecret: Data, tier: UInt8,
                          notAfter: UInt64, issuerEpoch: UInt32) throws -> Data {
            try consume(encoded.withC { pp, pl in issuerSecret.withC { sp, sl in memberSecret.withC { mp, ml in
                aviary_credential_issue(pp, pl, sp, sl, mp, ml, tier, notAfter, issuerEpoch) } } },
                "credential_issue")
        }
        /// Holder side: an unlinkable presentation for (epoch, scope, challenge).
        public func present(credential: Data, epoch: UInt32, scope: String, challenge: Data) throws -> Data {
            try consume(encoded.withC { pp, pl in credential.withC { cp, cl in challenge.withC { chp, chl in
                aviary_credential_present(pp, pl, cp, cl, epoch, scope, chp, chl) } } },
                "credential_present")
        }

        // -- Option B - pseudonymous --------------------------------------------
        /// Holder step 1: derive a nym seed from [memberSecret] and blind-commit it.
        /// The returned blob is holder-secret; `extractCommitment` gives the part to
        /// send to the issuer.
        public func membershipCommit(memberSecret: Data) throws -> Data {
            try consume(encoded.withC { pp, pl in memberSecret.withC { mp, ml in
                aviary_membership_commit(pp, pl, mp, ml) } }, "membership_commit")
        }
        /// Issuer step 2: blind-sign the holder's [commitment] (never sees the seed).
        public func blindIssue(issuerSecret: Data, commitment: Data, tier: UInt8,
                               notAfter: UInt64, issuerEpoch: UInt32) throws -> Data {
            try consume(encoded.withC { pp, pl in issuerSecret.withC { sp, sl in commitment.withC { cp, cl in
                aviary_blind_issue(pp, pl, sp, sl, cp, cl, tier, notAfter, issuerEpoch) } } },
                "blind_issue")
        }
        /// Holder step 3: a per-epoch pseudonymous presentation.
        public func presentPseudonymous(credential: Data, epoch: UInt32, scope: String, challenge: Data) throws -> Data {
            try consume(encoded.withC { pp, pl in credential.withC { cp, cl in challenge.withC { chp, chl in
                aviary_credential_present_pseudonymous(pp, pl, cp, cl, epoch, scope, chp, chl) } } },
                "credential_present_pseudonymous")
        }
    }

    /// The commitment is the first length-prefixed field of a membership-commit blob.
    public static func extractCommitment(_ membershipCommit: Data) -> Data {
        let b = [UInt8](membershipCommit)
        let n = Int(b[0]) << 24 | Int(b[1]) << 16 | Int(b[2]) << 8 | Int(b[3])
        return membershipCommit.subdata(in: 4..<(4 + n))
    }
    /// Holder step 3 (assemble): finalize the pseudonymous credential (holder-secret).
    public static func finalizeMembership(membershipCommit: Data, blindIssuance: Data) throws -> Data {
        ensureReady()
        return try consume(membershipCommit.withC { mp, ml in blindIssuance.withC { bp, bl in
            aviary_finalize_membership(mp, ml, bp, bl) } }, "finalize_membership")
    }

    /// A verifier's admission gate (holds the issuer-epoch deny-list).
    public final class CredentialGate {
        private let h: OpaquePointer
        init(policy: CredentialPolicy) {
            let raw = policy.encoded.withC { p, l in aviary_credential_gate_new(p, l) }!
            h = OpaquePointer(raw)
        }
        deinit { aviary_credential_gate_free(UnsafeMutableRawPointer(h)) }

        /// Bulk revocation: refuse every credential issued in this epoch.
        public func revokeIssuerEpoch(_ epoch: UInt32) {
            aviary_credential_gate_revoke_epoch(UnsafeMutableRawPointer(h), epoch)
        }
        /// Admit? Runs the full checklist. Total - a malformed presentation is a quiet false.
        public func admit(epoch: UInt32, scope: String, challenge: Data, presentation: Data, now: UInt64) -> Bool {
            challenge.withC { chp, chl in presentation.withC { pp, pl in
                aviary_credential_gate_admit(UnsafeMutableRawPointer(h), epoch, scope, chp, chl, pp, pl, now) == 1 } }
        }
        /// Option B: admit? Returns the 48-byte pseudonym on success, or nil on reject.
        public func admitPseudonymous(epoch: UInt32, scope: String, challenge: Data, presentation: Data, now: UInt64) throws -> Data? {
            let res = challenge.withC { chp, chl in presentation.withC { pp, pl in
                aviary_credential_gate_admit_pseudonymous(UnsafeMutableRawPointer(h), epoch, scope, chp, chl, pp, pl, now) } }
            let nym = try consume(res, "admit_pseudonymous")
            return nym.isEmpty ? nil : nym
        }
        /// section 6.2 targeted revocation of an exact pseudonym.
        public func revokeNym(_ nym: Data) {
            nym.withC { np, nl in aviary_credential_gate_revoke_nym(UnsafeMutableRawPointer(h), np, nl) }
        }
        /// section 6.3 Sybil cap: record an admitted nym; true if first this (epoch, scope).
        public func noteAdmission(epoch: UInt32, scope: String, nym: Data) -> Bool {
            nym.withC { np, nl in
                aviary_credential_gate_note_admission(UnsafeMutableRawPointer(h), epoch, scope, np, nl) == 1 }
        }
    }
    public static func makeCredentialGate(policy: CredentialPolicy) -> CredentialGate {
        ensureReady()
        return CredentialGate(policy: policy)
    }

    // -- Composable chains (blueprint + registry discovery + assembly) -----------

    /// A validator BLS keypair (public 48 B, secret 32 B).
    public static func newBLSValidator(keyMaterial: Data) throws -> PortableAccount {
        ensureReady()
        return splitAccountBlob(try consume(keyMaterial.withC { p, l in aviary_bls_keygen(p, l) }, "bls_keygen"))
    }

    /// A declared mixture: engine / state model / proposer ids + component params.
    public struct Blueprint {
        public let encoded: Data
        public init(consensus: String, stateModel: String, proposer: String = "", params: Data = Data()) throws {
            Aviary.ensureReady()
            encoded = try consume(params.withC { pp, pl in
                aviary_blueprint_encode(consensus, stateModel, proposer, pp, pl) }, "blueprint_encode")
        }
        public func hash() throws -> Data {
            try consume(encoded.withC { p, l in aviary_blueprint_hash(p, l) }, "blueprint_hash")
        }
    }

    private static func decodeIds(_ b: Data) -> [String] {
        let x = [UInt8](b)
        let n = Int(x[0]) << 24 | Int(x[1]) << 16 | Int(x[2]) << 8 | Int(x[3])
        var out: [String] = []; var off = 4
        for _ in 0..<n {
            let l = Int(x[off]) << 24 | Int(x[off+1]) << 16 | Int(x[off+2]) << 8 | Int(x[off+3]); off += 4
            out.append(String(decoding: x[off..<(off + l)], as: UTF8.self)); off += l
        }
        return out
    }
    public static func engineIds() throws -> [String] { ensureReady(); return decodeIds(try consume(aviary_registry_ids(0), "registry_ids")) }
    public static func stateModelIds() throws -> [String] { ensureReady(); return decodeIds(try consume(aviary_registry_ids(1), "registry_ids")) }
    public static func proposerIds() throws -> [String] { ensureReady(); return decodeIds(try consume(aviary_registry_ids(2), "registry_ids")) }

    public struct GenesisAccount { public let address: Data; public let balance: UInt64
        public init(address: Data, balance: UInt64) { self.address = address; self.balance = balance } }
    public struct GenesisValidator { public let publicKey: Data; public let power: UInt64
        public init(publicKey: Data, power: UInt64) { self.publicKey = publicKey; self.power = power } }

    /// A minimal genesis: chain params + posture (1/2/3) + seeded state + validators.
    public static func buildGenesis(chainID: UInt64, timestamp: UInt64 = 1, assuranceLevel: UInt16 = 1,
                                    stateModel: UInt8 = 0, accounts: [GenesisAccount],
                                    validators: [GenesisValidator]) throws -> Data {
        ensureReady()
        var addrs = Data(); for a in accounts { addrs.append(a.address) }
        var pks = Data(); for v in validators { pks.append(v.publicKey) }
        let balances = accounts.map { $0.balance }, powers = validators.map { $0.power }
        let res = addrs.withC { ap, _ in pks.withC { kp, _ in
            balances.withUnsafeBufferPointer { bb in powers.withUnsafeBufferPointer { pb in
                aviary_genesis_build(chainID, timestamp, assuranceLevel, stateModel,
                                           ap, bb.baseAddress, accounts.count, kp, pb.baseAddress, validators.count) } } } }
        return try consume(res, "genesis_build")
    }
    /// Commit a mixture into a genesis (its hash changes: the mixture is chain identity).
    public static func commitBlueprint(genesis: Data, blueprint: Blueprint) throws -> Data {
        try consume(genesis.withC { gp, gl in blueprint.encoded.withC { bp, bl in
            aviary_genesis_commit_blueprint(gp, gl, bp, bl) } }, "genesis_commit_blueprint")
    }
    public static func genesisHash(_ genesis: Data) throws -> Data {
        try consume(genesis.withC { p, l in aviary_genesis_hash(p, l) }, "genesis_hash")
    }

    /// An assembled node behind the uniform propose -> (seal | mine) -> submit flow.
    public final class Chain {
        private let h: OpaquePointer
        init?(genesis: Data, me: UInt32, blsPub: Data, blsSec: Data) {
            let raw = genesis.withC { gp, gl in blsPub.withC { pp, pl in blsSec.withC { sp, sl in
                aviary_chain_assemble(gp, gl, me, pp, pl, sp, sl) } } }
            guard let r = raw else { return nil }
            h = OpaquePointer(r)
        }
        deinit {
            if nodeEventHooks[UnsafeMutableRawPointer(h)] != nil {
                aviary_chain_set_event_hook(UnsafeMutableRawPointer(h), nil, nil)
                nodeEventHooks[UnsafeMutableRawPointer(h)] = nil
            }
            aviary_chain_free(UnsafeMutableRawPointer(h))
        }
        public var height: UInt64 { aviary_chain_height(UnsafeMutableRawPointer(h)) }
        /// Buffered block events (block_finalized or block_rejected per submit) as a JSON
        /// array; empty while a hook is set.
        public func drainEvents() throws -> String { try consumeString(aviary_chain_drain_events(UnsafeMutableRawPointer(h)), "chain_drain_events") }
        /// Receives every block event as one JSON object, synchronously from the submit that
        /// caused it; nil removes the hook.
        public func setEventHook(_ hook: ((String) -> Void)?) {
            hookEvents(UnsafeMutableRawPointer(h), hook) { aviary_chain_set_event_hook($0, $1, $2) }
        }
        /// Prometheus text: chain height, blocks finalized and rejected, transactions applied.
        public func metrics() throws -> String { try consumeString(aviary_chain_metrics(UnsafeMutableRawPointer(h)), "chain_metrics") }
        public func blueprint() throws -> Data { try consume(aviary_chain_blueprint(UnsafeMutableRawPointer(h)), "chain_blueprint") }
        public func propose(timestamp: UInt64) throws -> Data { try consume(aviary_chain_propose(UnsafeMutableRawPointer(h), timestamp), "chain_propose") }
        /// PoS / VRF justification; empty for engines that do not seal.
        public func seal(_ block: Data) throws -> Data {
            try consume(block.withC { bp, bl in aviary_chain_seal(UnsafeMutableRawPointer(h), bp, bl) }, "chain_seal")
        }
        /// PoW: the solved block.
        public func mine(_ block: Data) throws -> Data {
            try consume(block.withC { bp, bl in aviary_chain_mine(UnsafeMutableRawPointer(h), bp, bl) }, "chain_mine")
        }
        public func submit(block: Data, justification: Data = Data()) -> Bool {
            block.withC { bp, bl in justification.withC { jp, jl in
                aviary_chain_submit(UnsafeMutableRawPointer(h), bp, bl, jp, jl) == 1 } }
        }

        // -- Subchains --
        /// Propose the next block on a PARENT chain, anchoring encoded anchors (ascending child id). Throws on an invalid anchor.
        public func proposeAnchored(timestamp: UInt64, anchors: [Data]) throws -> Data {
            let blob = blobList(anchors)
            return try consume(blob.withC { ap, al in aviary_chain_propose_anchored(UnsafeMutableRawPointer(h), timestamp, ap, al) }, "chain_propose_anchored")
        }
        /// BFT: this validator's precommit aggregated into a commit certificate (the justification for submit; what a parent anchors).
        public func certify(_ block: Data) throws -> Data {
            try consume(block.withC { bp, bl in aviary_chain_certify(UnsafeMutableRawPointer(h), bp, bl) }, "chain_certify")
        }
        /// BFT: schedule a coordinated validator-set change; the next proposal announces it. Throws if not BFT / malformed.
        public func scheduleValidators(_ validators: [GenesisValidator]) throws {
            var pks = Data(); for v in validators { pks.append(v.publicKey) }
            let powers = validators.map { $0.power }
            let ok = pks.withC { kp, _ in powers.withUnsafeBufferPointer { pb in
                aviary_chain_schedule_validators(UnsafeMutableRawPointer(h), kp, pb.baseAddress, validators.count) } }
            if ok != 1 { throw AviaryError(message: "chain_schedule_validators: not BFT, or malformed set") }
        }
        /// BFT: the validator set in force for the next block.
        public func validators() throws -> [GenesisValidator] {
            let b = try consume(aviary_chain_validators(UnsafeMutableRawPointer(h)), "chain_validators")
            func u32(_ o: Int) -> Int { Int(UInt32(bigEndian: b.subdata(in: o..<o+4).withUnsafeBytes { $0.load(as: UInt32.self) })) }
            func u64(_ o: Int) -> UInt64 { UInt64(bigEndian: b.subdata(in: o..<o+8).withUnsafeBytes { $0.load(as: UInt64.self) }) }
            var out: [GenesisValidator] = []; var off = 4
            for _ in 0..<u32(0) { let l = u32(off); off += 4; let pk = b.subdata(in: off..<off+l); off += l; out.append(.init(publicKey: pk, power: u64(off))); off += 8 }
            return out
        }
        /// A parent's last anchored header for a child.
        public func childTip(_ childChainID: UInt64) throws -> Data { try consume(aviary_chain_child_tip(UnsafeMutableRawPointer(h), childChainID), "chain_child_tip") }
        /// A proof hop from this parent's current registry into a child.
        public func subchainHop(_ childChainID: UInt64) throws -> Data { try consume(aviary_chain_subchain_hop(UnsafeMutableRawPointer(h), childChainID), "chain_subchain_hop") }
        /// State-inclusion proof for a 32-byte address on this account-model chain.
        public func proveAccount(_ address: Data) throws -> Data {
            try consume(address.withC { ap, _ in aviary_chain_prove_account(UnsafeMutableRawPointer(h), ap) }, "chain_prove_account")
        }
    }

    // -- Light client: follow a header chain with certificates only ------------

    static func encodeValidatorSet(_ vs: [GenesisValidator]) -> Data {
        var d = Data(); d.append(contentsOf: withUnsafeBytes(of: UInt32(vs.count).bigEndian) { Data($0) })
        for v in vs {
            d.append(contentsOf: withUnsafeBytes(of: UInt32(v.publicKey.count).bigEndian) { Data($0) }); d.append(v.publicKey)
            d.append(contentsOf: withUnsafeBytes(of: v.power.bigEndian) { Data($0) })
        }
        return d
    }
    static func decodeValidatorSet(_ b: Data) -> [GenesisValidator] {
        func u32(_ o: Int) -> Int { Int(UInt32(bigEndian: b.subdata(in: o..<o+4).withUnsafeBytes { $0.load(as: UInt32.self) })) }
        func u64(_ o: Int) -> UInt64 { UInt64(bigEndian: b.subdata(in: o..<o+8).withUnsafeBytes { $0.load(as: UInt64.self) }) }
        var out: [GenesisValidator] = []; var off = 4
        for _ in 0..<u32(0) { let l = u32(off); off += 4; let pk = b.subdata(in: off..<off+l); off += l; out.append(.init(publicKey: pk, power: u64(off))); off += 8 }
        return out
    }
    /// A header, its certificate, and the set in force at its height.
    public struct Checkpoint { public var header: Data; public var certificate: Data; public var validators: [GenesisValidator]
        public init(header: Data, certificate: Data, validators: [GenesisValidator]) { self.header = header; self.certificate = certificate; self.validators = validators } }
    public enum SkipResult: Int32 { case refused = 0, accepted = 1, insufficientOverlap = 2 }

    /// Sequential accept plus the OVERLAP (skipping) rule: acceptSkipping jumps to a far header when
    /// its own set certifies it and the signers hold more than the trust level (default 1/3) of the
    /// set the client already trusts; syncTo bisects when the overlap is too thin.
    public final class LightClient {
        private let h: OpaquePointer
        init?(chainID: UInt64, validators: [GenesisValidator], trustedHeader: Data) {
            let vs = Aviary.encodeValidatorSet(validators)
            let raw = vs.withC { vp, vl in trustedHeader.withC { hp, hl in aviary_light_client_new(chainID, vp, vl, hp, hl) } }
            guard let r = raw else { return nil }
            h = OpaquePointer(r)
        }
        deinit { aviary_light_client_free(UnsafeMutableRawPointer(h)) }
        public var height: UInt64 { aviary_light_client_height(UnsafeMutableRawPointer(h)) }
        public func tip() throws -> Data { try consume(aviary_light_client_tip(UnsafeMutableRawPointer(h)), "light_client_tip") }
        public func validators() throws -> [GenesisValidator] { Aviary.decodeValidatorSet(try consume(aviary_light_client_validators(UnsafeMutableRawPointer(h)), "light_client_validators")) }
        public func setTrustLevel(numerator: UInt32, denominator: UInt32) { aviary_light_client_set_trust_level(UnsafeMutableRawPointer(h), numerator, denominator) }
        public func setMaxSkip(_ blocks: UInt64) { aviary_light_client_set_max_skip(UnsafeMutableRawPointer(h), blocks) }
        public func accept(header: Data, certificate: Data) -> Bool {
            header.withC { hp, hl in certificate.withC { cp, cl in aviary_light_client_accept(UnsafeMutableRawPointer(h), hp, hl, cp, cl) == 1 } }
        }
        public func acceptSkipping(_ cp: Checkpoint) -> SkipResult {
            let vs = Aviary.encodeValidatorSet(cp.validators)
            let r = cp.header.withC { hp, hl in cp.certificate.withC { cpp, cl in vs.withC { vp, vl in
                aviary_light_client_accept_skipping(UnsafeMutableRawPointer(h), hp, hl, cpp, cl, vp, vl) } } }
            return SkipResult(rawValue: r) ?? .refused
        }
        /// Bisection: fetch(height) -> checkpoint or nil. Throws on a hard refusal / missing checkpoint.
        public func syncTo(_ target: Checkpoint, targetHeight: UInt64, fetch: (UInt64) -> Checkpoint?) throws {
            var stack: [(Checkpoint, UInt64)] = [(target, targetHeight)]
            var n = 0
            while let (cp, hi) = stack.last {
                n += 1; if n > 128 { throw AviaryError(message: "light client: bisection exceeded its fetch budget") }
                switch acceptSkipping(cp) {
                case .accepted: stack.removeLast()
                case .refused: throw AviaryError(message: "light client: checkpoint refused")
                case .insufficientOverlap:
                    let lo = height
                    if hi - lo <= 1 { throw AviaryError(message: "light client: insufficient overlap and no intermediate header") }
                    let mid = lo + (hi - lo) / 2
                    guard let m = fetch(mid) else { throw AviaryError(message: "light client: checkpoint unavailable at \(mid)") }
                    stack.append((m, mid))
                }
            }
        }
    }
    /// nil = malformed validator set or header.
    public static func newLightClient(chainID: UInt64, validators: [GenesisValidator], trustedHeader: Data) -> LightClient? {
        ensureReady(); return LightClient(chainID: chainID, validators: validators, trustedHeader: trustedHeader)
    }

    // -- Chain tree: declare a FAMILY of chains and drive it together ----------
    /// A validator with the secret this process holds (nil = not held).
    public struct TreeValidator { public var publicKey: Data; public var secret: Data?; public var power: UInt64
        public init(publicKey: Data, secret: Data?, power: UInt64 = 1) { self.publicKey = publicKey; self.secret = secret; self.power = power } }
    /// One chain of a family; children are subchains (must be BFT).
    public struct TreeNode {
        public var chainID: UInt64; public var blueprint: Blueprint?; public var assuranceLevel: UInt16
        public var accounts: [GenesisAccount]; public var validators: [TreeValidator]
        public var maxAnchorsPerBlock: UInt16; public var maxHeadersPerAnchor: UInt16; public var children: [TreeNode]
        public init(chainID: UInt64, blueprint: Blueprint? = nil, assuranceLevel: UInt16 = 1, accounts: [GenesisAccount] = [],
                    validators: [TreeValidator], maxAnchorsPerBlock: UInt16 = 0, maxHeadersPerAnchor: UInt16 = 0, children: [TreeNode] = []) {
            self.chainID = chainID; self.blueprint = blueprint; self.assuranceLevel = assuranceLevel; self.accounts = accounts
            self.validators = validators; self.maxAnchorsPerBlock = maxAnchorsPerBlock; self.maxHeadersPerAnchor = maxHeadersPerAnchor; self.children = children
        }
        public func encode() throws -> Data {
            ensureReady()
            let bp = try (blueprint ?? Blueprint(consensus: "bft", stateModel: "account")).encoded
            var addrs = Data(); for a in accounts { addrs.append(a.address) }
            var pks = Data(), secrets = Data(); for v in validators { pks.append(v.publicKey); secrets.append(v.secret ?? Data(count: 32)) }
            let balances = accounts.map { $0.balance }, powers = validators.map { $0.power }
            let kids = blobList(try children.map { try $0.encode() })
            let res = bp.withC { bpp, bpl in addrs.withC { ap, _ in pks.withC { kp, _ in secrets.withC { sp, _ in kids.withC { cp, cl in
                balances.withUnsafeBufferPointer { bb in powers.withUnsafeBufferPointer { pb in
                    aviary_chain_tree_node_encode(chainID, bpp, bpl, assuranceLevel, ap, bb.baseAddress, accounts.count,
                                                        kp, validators.isEmpty ? nil : sp, pb.baseAddress, validators.count,
                                                        maxAnchorsPerBlock, maxHeadersPerAnchor, cp, cl) } } } } } } }
            return try consume(res, "chain_tree_node_encode")
        }
    }
    /// Why a spec is structurally invalid ("" = valid).
    public static func chainTreeSpecError(_ spec: Data) throws -> String {
        ensureReady(); return String(decoding: try consume(spec.withC { sp, sl in aviary_chain_tree_spec_error(sp, sl) }, "chain_tree_spec_error"), as: UTF8.self)
    }
    /// A built family driven in-process: produce() anchors children upward; proveAccount() proves any descendant from the root.
    public final class ChainTree {
        private let h: OpaquePointer
        public var onFinalized: ((UInt64, Data) -> Void)?
        public var txSource: ((UInt64, UInt64) -> [Data])?
        init(root: TreeNode) throws {
            let spec = try root.encode()
            let err = try Aviary.chainTreeSpecError(spec)
            if !err.isEmpty { throw AviaryError(message: "chain tree: " + err) }
            guard let raw = spec.withC({ sp, sl in aviary_chain_tree_build(sp, sl) }) else {
                throw AviaryError(message: "chain tree: build rejected (unknown mixture or posture-forbidden engine)") }
            h = OpaquePointer(raw)
            let ctx = Unmanaged.passUnretained(self).toOpaque()
            let fin: AviaryFinalizedFn = { ctx, id, block, len in
                guard let ctx, let block else { return }
                let t = Unmanaged<ChainTree>.fromOpaque(ctx).takeUnretainedValue()
                t.onFinalized?(id, Data(bytes: block, count: len))
            }
            let tx: AviaryTxSourceFn = { ctx, id, ts, out, cap, outLen in
                guard let ctx, let out, let outLen else { return 0 }
                let t = Unmanaged<ChainTree>.fromOpaque(ctx).takeUnretainedValue()
                guard let src = t.txSource else { return 0 }
                let blob = Aviary.blobList(src(id, ts)); if blob.count > cap { return 0 }
                blob.copyBytes(to: out, count: blob.count); outLen.pointee = blob.count; return 1
            }
            aviary_chain_tree_set_finalized_hook(UnsafeMutableRawPointer(h), fin, ctx)
            aviary_chain_tree_set_tx_source(UnsafeMutableRawPointer(h), tx, ctx)
        }
        deinit { aviary_chain_tree_free(UnsafeMutableRawPointer(h)) }
        public func chainIDs() throws -> [UInt64] {
            let b = try consume(aviary_chain_tree_chain_ids(UnsafeMutableRawPointer(h)), "chain_tree_chain_ids")
            let n = Int(UInt32(bigEndian: b.subdata(in: 0..<4).withUnsafeBytes { $0.load(as: UInt32.self) }))
            return (0..<n).map { i in UInt64(bigEndian: b.subdata(in: 4 + 8*i..<12 + 8*i).withUnsafeBytes { $0.load(as: UInt64.self) }) }
        }
        public func height(_ chainID: UInt64) -> UInt64 { aviary_chain_tree_height(UnsafeMutableRawPointer(h), chainID) }
        public func genesis(_ chainID: UInt64) throws -> Data { try consume(aviary_chain_tree_genesis(UnsafeMutableRawPointer(h), chainID), "chain_tree_genesis") }
        public func tip(_ chainID: UInt64) throws -> Data { try consume(aviary_chain_tree_tip(UnsafeMutableRawPointer(h), chainID), "chain_tree_tip") }
        public func pendingAnchors(_ chainID: UInt64) -> UInt32 { aviary_chain_tree_pending_anchors(UnsafeMutableRawPointer(h), chainID) }
        public func produce(_ chainID: UInt64, timestamp: UInt64) throws -> Data { try consume(aviary_chain_tree_produce(UnsafeMutableRawPointer(h), chainID, timestamp), "chain_tree_produce") }
        public func proveAccount(_ chainID: UInt64, address: Data) throws -> Data {
            try consume(address.withC { ap, _ in aviary_chain_tree_prove_account(UnsafeMutableRawPointer(h), chainID, ap) }, "chain_tree_prove_account")
        }
        public func verify(_ proof: Data) -> Bool { proof.withC { pp, pl in aviary_chain_tree_verify(UnsafeMutableRawPointer(h), pp, pl) == 1 } }
    }
    public static func buildChainTree(_ root: TreeNode) throws -> ChainTree { ensureReady(); return try ChainTree(root: root) }

    // -- Subchains: a parent block anchors a child chain's finalized headers ----
    // (docs/design/SUBCHAINS.md). Register a child at the parent's genesis by its
    // genesis HEADER + fixed validator set; anchor its certified headers; prove a
    // state entry on a descendant from the parent's header. v1 anchors BFT children.

    /// [count u32]([len u32][bytes])x - the C ABI's blob-list convention.
    static func blobList(_ items: [Data]) -> Data {
        var d = Data(); d.append(contentsOf: withUnsafeBytes(of: UInt32(items.count).bigEndian) { Data($0) })
        for it in items { d.append(contentsOf: withUnsafeBytes(of: UInt32(it.count).bigEndian) { Data($0) }); d.append(it) }
        return d
    }
    /// A child registered at a parent's genesis: its genesis HEADER, fixed validators, and its own subtree depth.
    public struct ChildRegistration {
        public var chainID: UInt64; public var genesisHeader: Data; public var validators: [GenesisValidator]; public var maxDepth: UInt8
        public init(chainID: UInt64, genesisHeader: Data, validators: [GenesisValidator], maxDepth: UInt8 = 0) {
            self.chainID = chainID; self.genesisHeader = genesisHeader; self.validators = validators; self.maxDepth = maxDepth
        }
        public func encode() throws -> Data {
            ensureReady()
            var pks = Data(); for v in validators { pks.append(v.publicKey) }
            let powers = validators.map { $0.power }
            let res = genesisHeader.withC { hp, hl in pks.withC { kp, _ in powers.withUnsafeBufferPointer { pb in
                aviary_child_registration_encode(chainID, hp, hl, kp, pb.baseAddress, validators.count, maxDepth) } } }
            return try consume(res, "child_registration_encode")
        }
    }
    /// A genesis's genesis HEADER - the identity a parent registers a child by.
    public static func genesisHeader(_ genesis: Data) throws -> Data {
        ensureReady(); return try consume(genesis.withC { gp, gl in aviary_genesis_header(gp, gl) }, "genesis_header")
    }
    /// Encode a parent's subchain policy (children ascending, unique chain ids). 0 = unlimited.
    public static func encodeSubchainPolicy(maxChildren: UInt16, maxDepth: UInt8 = 1, maxAnchorsPerBlock: UInt16 = 0,
                                            maxHeadersPerAnchor: UInt16 = 0, children: [ChildRegistration]) throws -> Data {
        ensureReady()
        let blob = blobList(try children.map { try $0.encode() })
        return try consume(blob.withC { bp, bl in aviary_subchain_policy_encode(maxChildren, maxDepth, maxAnchorsPerBlock, maxHeadersPerAnchor, bp, bl) }, "subchain_policy_encode")
    }
    /// Commit a policy into a genesis (this chain becomes a PARENT; its hash changes). Throws on an invalid policy.
    public static func commitSubchainPolicy(genesis: Data, policy: Data) throws -> Data {
        ensureReady(); return try consume(genesis.withC { gp, gl in policy.withC { pp, pl in aviary_genesis_commit_subchain_policy(gp, gl, pp, pl) } }, "genesis_commit_subchain_policy")
    }
    /// Bind a child genesis to its parent (id + 32-byte parent genesis hash); the child can never be re-parented.
    public static func commitParentBinding(genesis: Data, parentChainID: UInt64, parentGenesisHash: Data) throws -> Data {
        ensureReady(); return try consume(genesis.withC { gp, gl in parentGenesisHash.withC { hp, _ in aviary_genesis_commit_parent_binding(gp, gl, parentChainID, hp) } }, "genesis_commit_parent_binding")
    }
    public static func blockHeader(_ block: Data) throws -> Data { ensureReady(); return try consume(block.withC { bp, bl in aviary_block_header(bp, bl) }, "block_header") }
    /// A header's 32-byte subchain root (zero below version 3).
    public static func headerSubchainRoot(_ header: Data) throws -> Data { ensureReady(); return try consume(header.withC { hp, hl in aviary_header_subchain_root(hp, hl) }, "header_subchain_root") }
    public static func certifiedHeader(header: Data, certificate: Data) throws -> Data {
        ensureReady(); return try consume(header.withC { hp, hl in certificate.withC { cp, cl in aviary_certified_header_encode(hp, hl, cp, cl) } }, "certified_header_encode")
    }
    /// An anchor: one child's CONSECUTIVE certified headers.
    public static func encodeAnchor(childChainID: UInt64, certifiedHeaders: [Data]) throws -> Data {
        ensureReady(); let blob = blobList(certifiedHeaders)
        return try consume(blob.withC { bp, bl in aviary_anchor_encode(childChainID, bp, bl) }, "anchor_encode")
    }
    /// A cross-chain proof: hops (parent -> ... -> the chain holding the entry) + that chain's state proof.
    public static func encodeSubchainProof(hops: [Data], stateProof: Data) throws -> Data {
        ensureReady(); let blob = blobList(hops)
        return try consume(blob.withC { hp, hl in stateProof.withC { sp, sl in aviary_subchain_proof_encode(hp, hl, sp, sl) } }, "subchain_proof_encode")
    }
    /// Whether the proof is committed under the parent's 32-byte subchain root all the way down and its state proof holds.
    public static func verifySubchainProof(parentSubchainRoot: Data, proof: Data) -> Bool {
        ensureReady(); guard parentSubchainRoot.count == 32 else { return false }
        return parentSubchainRoot.withC { rp, _ in proof.withC { pp, pl in aviary_subchain_proof_verify(rp, pp, pl) == 1 } }
    }
    /// nil = rejected (fail-closed): unknown id, state mismatch, or posture-forbidden engine.
    public static func assembleChain(genesis: Data, me: UInt32, blsPub: Data, blsSec: Data) -> Chain? {
        ensureReady()
        return Chain(genesis: genesis, me: me, blsPub: blsPub, blsSec: blsSec)
    }

    // -- Foreign components (engines / proposers implemented in Swift) ----------

    /// A consensus engine implemented in Swift, plugged into a mixture by id. The
    /// consensus pipeline stays native; Swift supplies the JUSTIFICATION policy.
    /// Both methods run synchronously inside consensus and MUST be deterministic
    /// (no clock, no randomness) and must not call back into the chain.
    public protocol ForeignEngine: AnyObject {
        /// The justification for a block this node proposed (<= 4096 B), or nil to refuse.
        func justify(block: Data) -> Data?
        func verify(block: Data, justification: Data) -> Bool
    }
    private final class ProposerBox { let fn: (UInt64, UInt32) -> UInt32; init(_ f: @escaping (UInt64, UInt32) -> UInt32) { fn = f } }
    private static var retainedForeign: [AnyObject] = []  // kept alive for the process lifetime

    /// Register a Swift [ForeignEngine] under [id] (chain identity - never rename).
    @discardableResult
    public static func registerEngine(id: String, _ engine: ForeignEngine, grindProof: Bool = false) -> Bool {
        ensureReady()
        retainedForeign.append(engine)
        let ctx = Unmanaged.passUnretained(engine as AnyObject).toOpaque()
        let justify: AviaryJustifyFn = { ctx, block, len, out, cap, outLen in
            guard let ctx, let block, let out, let outLen,
                  let e = Unmanaged<AnyObject>.fromOpaque(ctx).takeUnretainedValue() as? ForeignEngine,
                  let j = e.justify(block: Data(bytes: block, count: len)), j.count <= cap else { return 0 }
            j.copyBytes(to: out, count: j.count)
            outLen.pointee = j.count
            return 1
        }
        let verify: AviaryVerifyFn = { ctx, block, len, just, jlen in
            guard let ctx, let block, let just,
                  let e = Unmanaged<AnyObject>.fromOpaque(ctx).takeUnretainedValue() as? ForeignEngine else { return 0 }
            return e.verify(block: Data(bytes: block, count: len), justification: Data(bytes: just, count: jlen)) ? 1 : 0
        }
        return aviary_registry_add_engine(id, justify, verify, ctx, grindProof ? 1 : 0) == 1
    }

    /// Register a Swift proposer policy under [id]. Must be deterministic.
    @discardableResult
    public static func registerProposer(id: String, _ policy: @escaping (UInt64, UInt32) -> UInt32) -> Bool {
        ensureReady()
        let box = ProposerBox(policy)
        retainedForeign.append(box)
        let ctx = Unmanaged.passUnretained(box).toOpaque()
        let fn: AviaryProposerFn = { ctx, height, n in
            guard let ctx else { return 0 }
            return Unmanaged<ProposerBox>.fromOpaque(ctx).takeUnretainedValue().fn(height, n)
        }
        return aviary_registry_add_proposer(id, fn, ctx) == 1
    }

    /// Create a new in-memory single-validator chain.
    public static func makeNode(chainID: UInt64) throws -> Node {
        ensureReady()
        guard let h = aviary_node_new(chainID) else {
            throw AviaryError(message: "node creation failed")
        }
        return Node(handle: h)
    }

    /// Create a custodial key store protected by [passphrase] (returned unlocked).
    public static func makeKeyStore(passphrase: String) throws -> KeyStore {
        ensureReady()
        guard let h = aviary_keystore_new(passphrase) else {
            throw AviaryError(message: "keystore creation failed")
        }
        return KeyStore(handle: h)
    }

    /// Load a serialised key store. It comes back LOCKED - call `unlock`.
    public static func loadKeyStore(_ blob: Data) throws -> KeyStore {
        ensureReady()
        guard let h = blob.withC({ p, l in aviary_keystore_load(p, l) }) else {
            throw AviaryError(message: "keystore load failed")
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
    deinit { aviary_keystore_free(UnsafeMutableRawPointer(handle)) }

    private var h: UnsafeMutableRawPointer { UnsafeMutableRawPointer(handle) }

    /// Seal an account's [secret]; returns its 32-byte address. Requires unlocked.
    public func addAccount(publicKey: Data, secret: Data) throws -> Data {
        var address = Data(count: 32)
        let rc = publicKey.withC { pp, pl in
            secret.withC { sp, sl in
                address.withUnsafeMutableBytes { ap in
                    aviary_keystore_add_account(h, pp, pl, sp, sl,
                                                      ap.bindMemory(to: UInt8.self).baseAddress!)
                }
            }
        }
        if rc != 0 { throw AviaryError(message: "keystore add account failed (locked?)") }
        return address
    }

    /// Decrypt the sealed secret for [address]. Requires unlocked.
    public func reveal(address: Data) throws -> Data {
        try consume(address.withC { p, _ in aviary_keystore_reveal(h, p) }, "keystore_reveal")
    }

    /// The persistable (encrypted) blob - no plaintext key.
    public func serialise() throws -> Data {
        try consume(aviary_keystore_serialise(h), "keystore_serialise")
    }

    /// Recover the master key from [passphrase]. Throws on a wrong passphrase.
    public func unlock(passphrase: String) throws {
        if aviary_keystore_unlock(h, passphrase) != 0 {
            throw AviaryError(message: "keystore unlock failed")
        }
    }

    /// Whether secrets can currently be revealed.
    public var unlocked: Bool { aviary_keystore_unlocked(h) == 1 }
}

/// A live in-memory chain node (ledger + BFT + mempool + observability).
public final class Node {
    private let handle: OpaquePointer
    init(handle: UnsafeMutableRawPointer) { self.handle = OpaquePointer(handle) }
    deinit {
        if nodeEventHooks[UnsafeMutableRawPointer(handle)] != nil {
            aviary_node_set_event_hook(UnsafeMutableRawPointer(handle), nil, nil)
            nodeEventHooks[UnsafeMutableRawPointer(handle)] = nil
        }
        aviary_node_free(UnsafeMutableRawPointer(handle))
    }

    private var h: UnsafeMutableRawPointer { UnsafeMutableRawPointer(handle) }

    public struct Account { public let secret: Data; public let publicKey: Data; public let address: Data }

    public func newAccount() throws -> Account {
        let secret = try consume(aviary_node_new_account(h), "new_account")
        let publicKey = secret.subdata(in: 32..<64)
        let address = try consume(publicKey.withC { aviary_account_address($0, $1) }, "account_address")
        return Account(secret: secret, publicKey: publicKey, address: address)
    }

    /// Credit an address at genesis. Valid until the first block is produced
    /// (reads do NOT seal genesis); throws afterwards - never a silent no-op.
    public func credit(address: Data, amount: UInt64) throws {
        let rc = address.withC { p, _ in aviary_node_credit(h, p, amount) }
        if rc == 2 {
            throw AviaryError(
                message: "genesis is sealed (a block was produced); fund accounts with transfers")
        }
        if rc != 0 { throw AviaryError(message: "credit failed (status \(rc))") }
    }

    @discardableResult
    public func submitTransfer(secret: Data, to: Data, amount: UInt64, nonce: UInt64) throws -> Data {
        try consume(secret.withC { sp, sl in
            to.withC { tp, _ in aviary_node_submit_transfer(h, sp, sl, tp, amount, nonce) }
        }, "submit_transfer")
    }

    /// Submit an Ed25519 transfer carrying a priority-fee `fee` (version-2 envelope).
    @discardableResult
    public func submitTransferFee(secret: Data, to: Data, amount: UInt64, nonce: UInt64,
                                  fee: UInt64) throws -> Data {
        try consume(secret.withC { sp, sl in
            to.withC { tp, _ in aviary_node_submit_transfer_fee(h, sp, sl, tp, amount, nonce, fee) }
        }, "submit_transfer_fee")
    }

    /// Generate a fresh hybrid (Ed25519 + ML-DSA-65) post-quantum account.
    public func newAccountHybrid() throws -> Account {
        let blob = try consume(aviary_node_new_account_hybrid(h), "new_account_hybrid")
        // [pub_len: 4 bytes big-endian][public][secret]
        let pl = Int(blob[0]) << 24 | Int(blob[1]) << 16 | Int(blob[2]) << 8 | Int(blob[3])
        let publicKey = blob.subdata(in: 4..<(4 + pl))
        let secret = blob.subdata(in: (4 + pl)..<blob.count)
        let address = try consume(publicKey.withC { aviary_account_address($0, $1) }, "account_address")
        return Account(secret: secret, publicKey: publicKey, address: address)
    }

    /// Build, sign, and admit a hybrid (post-quantum) transfer.
    @discardableResult
    public func submitTransferHybrid(secret: Data, publicKey: Data, to: Data,
                                     amount: UInt64, nonce: UInt64) throws -> Data {
        try consume(secret.withC { sp, sl in
            publicKey.withC { pp, pl in
                to.withC { tp, _ in
                    aviary_node_submit_transfer_hybrid(h, sp, sl, pp, pl, tp, amount, nonce)
                }
            }
        }, "submit_transfer_hybrid")
    }

    @discardableResult
    public func produceBlock(timestamp: UInt64) throws -> UInt64 {
        var height: UInt64 = 0
        if aviary_node_produce_block(h, timestamp, &height) != 0 {
            throw AviaryError(message: "produce_block failed")
        }
        return height
    }

    public var height: UInt64 { aviary_node_height(h) }
    public var mempoolSize: UInt64 { aviary_node_mempool_size(h) }
    public func balance(address: Data) -> UInt64 { address.withC { p, _ in aviary_node_balance(h, p) } }
    public func nonce(address: Data) -> UInt64 { address.withC { p, _ in aviary_node_nonce(h, p) } }
    /// The committed nonce advanced past every pending transfer from the address.
    public func nextNonce(address: Data) -> UInt64 { address.withC { p, _ in aviary_node_next_nonce(h, p) } }
    public func stateRoot() throws -> Data { try consume(aviary_node_state_root(h), "state_root") }
    public func drainEvents() throws -> String { String(decoding: try consume(aviary_node_drain_events(h), "drain_events"), as: UTF8.self) }
    public func metrics() throws -> String { String(decoding: try consume(aviary_node_metrics(h), "metrics"), as: UTF8.self) }
    public func proveAccount(address: Data) throws -> Data {
        try consume(address.withC { p, _ in aviary_node_prove_account(h, p) }, "prove_account")
    }
}

/// A signing account's material - public key and secret - as returned by the
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

// -- Buffer marshalling helpers -------------------------------------------------

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

private func consume(_ result: AviaryBufferResult, _ op: String) throws -> Data {
    var r = result
    if r.buf.data == nil {
        if let e = r.error { let m = String(cString: e); aviary_str_free(e); throw AviaryError(message: m) }
        throw AviaryError(message: "\(op) failed")
    }
    let data = Data(bytes: r.buf.data, count: r.buf.len)
    aviary_buffer_free(&r.buf)
    return data
}

private func consumeString(_ result: AviaryBufferResult, _ op: String) throws -> String {
    String(decoding: try consume(result, op), as: UTF8.self)
}

// -- Events, persistence, custody signing --
private var nodeEventHooks: [UnsafeMutableRawPointer: (String) -> Void] = [:]

/// Points the native hook of handle h at hook (or clears it when nil) through setter.
private func hookEvents(_ h: UnsafeMutableRawPointer, _ hook: ((String) -> Void)?,
                        _ setter: (UnsafeMutableRawPointer, AviaryEventFn?, UnsafeMutableRawPointer?) -> Void) {
    guard let hook else { setter(h, nil, nil); nodeEventHooks[h] = nil; return }
    nodeEventHooks[h] = hook
    let cb: AviaryEventFn = { ctx, json, len in
        guard let ctx, let json, let hook = nodeEventHooks[ctx] else { return }
        hook(String(decoding: Data(bytes: json, count: len), as: UTF8.self))
    }
    setter(h, cb, h)
}

extension Node {
    /// Receives every chain event as one JSON object, synchronously from the node call that
    /// caused it; nil removes the hook. While set, drainEvents() is empty.
    public func setEventHook(_ hook: ((String) -> Void)?) {
        hookEvents(h, hook) { aviary_node_set_event_hook($0, $1, $2) }
    }
    /// The whole chain as one blob: chain id, genesis credits and every finalized block with
    /// its certificate. Pending transactions are not included.
    public func snapshot() throws -> Data { try consume(aviary_node_snapshot(h), "node_snapshot") }
    /// The encoded finalized block at height (1 based).
    public func blockAt(_ height: UInt64) throws -> Data { try consume(aviary_node_block_at(h, height), "node_block_at") }
    /// Admits an already signed, encoded transaction; returns its txid.
    @discardableResult
    public func submitTx(_ encoded: Data) throws -> Data {
        try consume(encoded.withC { p, l in aviary_node_submit_tx(h, p, l) }, "node_submit_tx")
    }
    /// Signs a transfer from the key store account at `from` and admits it; the secret never
    /// leaves the engine. Returns the txid.
    @discardableResult
    public func submitTransfer(fromStore store: KeyStore, from: Data, to: Data, amount: UInt64,
                               nonce: UInt64, fee: UInt64 = 0) throws -> Data {
        try consume(from.withC { fp, _ in to.withC { tp, _ in
            aviary_node_submit_transfer_keystore(h, store.rawHandle, fp, tp, amount, nonce, fee) } }, "node_submit_transfer_keystore")
    }
}

extension KeyStore {
    var rawHandle: UnsafeMutableRawPointer { h }
    /// Signs a transfer from the stored account at `from` without revealing its secret. The
    /// scheme follows the stored key; a zero fee gives the version 1 envelope.
    public func signTransfer(chainID: UInt64, from: Data, to: Data, amount: UInt64, nonce: UInt64,
                             fee: UInt64 = 0) throws -> Data {
        try consume(from.withC { fp, _ in to.withC { tp, _ in
            aviary_keystore_sign_transfer(h, chainID, fp, tp, amount, nonce, fee) } }, "keystore_sign_transfer")
    }
    /// The stored public key for `address`.
    public func publicKey(address: Data) throws -> Data {
        try consume(address.withC { p, _ in aviary_keystore_public_key(h, p) }, "keystore_public_key")
    }
    /// Whether the store holds `address`.
    public func has(address: Data) -> Bool { address.withC { p, _ in aviary_keystore_has(h, p) == 1 } }
    /// Every stored address.
    public func addresses() throws -> [Data] { Aviary.splitBlobList(try consume(aviary_keystore_addresses(h), "keystore_addresses")) }
    /// Forgets the master key until the next unlock.
    public func lock() { aviary_keystore_lock(h) }
}

extension Aviary {
    /// Rebuilds a node from `Node.snapshot()`. A current snapshot resumes from the tip at once;
    /// `replay` forces every block through consensus again (slow; audits the archive).
    public static func restoreNode(_ snapshot: Data, replay: Bool = false) throws -> Node {
        ensureReady()
        guard let raw = snapshot.withC({ p, l in replay ? aviary_node_restore_replay(p, l) : aviary_node_restore(p, l) }) else {
            throw AviaryError(message: "node restore refused (malformed snapshot)") }
        return Node(handle: raw)
    }
    /// [count u32]([len u32][bytes]) repeated, decoded.
    static func splitBlobList(_ b: Data) -> [Data] {
        func u32(_ o: Int) -> Int { Int(UInt32(bigEndian: b.subdata(in: o..<o+4).withUnsafeBytes { $0.load(as: UInt32.self) })) }
        var out: [Data] = []; var off = 4
        for _ in 0..<u32(0) { let l = u32(off); off += 4; out.append(b.subdata(in: off..<off+l)); off += l }
        return out
    }

    // -- Networking: consensus over a network the app owns --
    /// The validator index that follows a chain without voting.
    public static let netObserver: UInt32 = 0xFFFF_FFFF
    /// The step of a consensus round a timeout belongs to.
    public enum ConsensusStep: UInt8 { case propose = 0, prevote = 1, precommit = 2 }
    /// A gossip message to send to one peer.
    public struct Relay { public let to: UInt32; public let message: Data
        public init(to: UInt32, message: Data) { self.to = to; self.message = message } }
    /// A consensus timer to arm; when it fires, call `NetNode.onTimeout`.
    public struct TimeoutRequest { public let step: ConsensusStep; public let round: UInt32
        public init(step: ConsensusStep, round: UInt32) { self.step = step; self.round = round } }
    /// What a `NetNode` wants done after an input: relays to send and timers to arm. Durations
    /// are the caller's choice.
    public struct Outbound { public let relays: [Relay]; public let timeouts: [TimeoutRequest]
        public init(relays: [Relay], timeouts: [TimeoutRequest]) { self.relays = relays; self.timeouts = timeouts } }
    /// A long term peer identity: a hybrid KEM pair and a hybrid signature pair.
    public struct SecureIdentity { public let kem: PortableAccount; public let sig: PortableAccount
        public init(kem: PortableAccount, sig: PortableAccount) { self.kem = kem; self.sig = sig } }

    static func decodeOutbound(_ b: Data) -> Outbound {
        func u32(_ o: Int) -> Int { Int(UInt32(bigEndian: b.subdata(in: o..<o+4).withUnsafeBytes { $0.load(as: UInt32.self) })) }
        var off = 0; var relays: [Relay] = []; var timeouts: [TimeoutRequest] = []
        let rc = u32(off); off += 4
        for _ in 0..<rc { let to = UInt32(u32(off)); let l = u32(off + 4); off += 8; relays.append(Relay(to: to, message: b.subdata(in: off..<off+l))); off += l }
        let tc = u32(off); off += 4
        for _ in 0..<tc { timeouts.append(TimeoutRequest(step: ConsensusStep(rawValue: b[off]) ?? .propose, round: UInt32(u32(off + 1)))); off += 5 }
        return Outbound(relays: relays, timeouts: timeouts)
    }

    /// A validator or observer running BFT consensus over a gossip mesh with no socket and no
    /// clock of its own. Send each relay over your transport, arm each timeout, and feed what
    /// arrives back through `onGossip` and `onTimeout`.
    public final class NetNode {
        private let h: UnsafeMutableRawPointer
        init(handle: UnsafeMutableRawPointer) { h = handle }
        deinit {
            if nodeEventHooks[h] != nil { aviary_net_node_set_event_hook(h, nil, nil); nodeEventHooks[h] = nil }
            aviary_net_node_free(h)
        }
        public func addPeer(_ peer: UInt32) { aviary_net_node_add_peer(h, peer) }
        /// Buffered events (a block_finalized per committed height) as a JSON array; empty
        /// while a hook is set.
        public func drainEvents() throws -> String { try consumeString(aviary_net_node_drain_events(h), "net_node_drain_events") }
        /// Receives every event as one JSON object, synchronously from the input that caused
        /// it; nil removes the hook.
        public func setEventHook(_ hook: ((String) -> Void)?) {
            hookEvents(h, hook) { aviary_net_node_set_event_hook($0, $1, $2) }
        }
        /// Prometheus text: chain height, blocks finalized, gossip received, relayed and
        /// dropped, timeouts fired, peers banned.
        public func metrics() throws -> String { try consumeString(aviary_net_node_metrics(h), "net_node_metrics") }
        /// Encoded transactions this node proposes at the current height; cleared by `advance`.
        public func setTransactions(_ encodedTxs: [Data]) throws {
            let blob = Aviary.blobList(encodedTxs)
            if blob.withC({ p, l in aviary_net_node_set_transactions(h, p, l) }) != 1 {
                throw AviaryError(message: "net node: malformed transaction") }
        }
        public func start() throws -> Outbound { decodeOutbound(try consume(aviary_net_node_start(h), "net_node_start")) }
        public func onGossip(from: UInt32, payload: Data) throws -> Outbound {
            decodeOutbound(try consume(payload.withC { p, l in aviary_net_node_on_gossip(h, from, p, l) }, "net_node_on_gossip"))
        }
        public func onTimeout(_ t: TimeoutRequest) throws -> Outbound {
            decodeOutbound(try consume(aviary_net_node_on_timeout(h, t.step.rawValue, t.round), "net_node_on_timeout"))
        }
        /// Rolls into the next height after `committed`.
        public func advance() throws -> Outbound { decodeOutbound(try consume(aviary_net_node_advance(h), "net_node_advance")) }
        public var committed: Bool { aviary_net_node_committed(h) == 1 }
        public var height: UInt64 { aviary_net_node_height(h) }
        public func peerBanned(_ peer: UInt32) -> Bool { aviary_net_node_peer_banned(h, peer) == 1 }
        /// Every unique consensus payload of the current height; replay it to a late peer.
        public func replayLog() throws -> [Data] { Aviary.splitBlobList(try consume(aviary_net_node_replay_log(h), "net_node_replay_log")) }
        public func tip() throws -> Data { try consume(aviary_net_node_tip(h), "net_node_tip") }
        public func stateRoot() throws -> Data { try consume(aviary_net_node_state_root(h), "net_node_state_root") }
        public func balance(address: Data) -> UInt64 { address.withC { p, _ in aviary_net_node_balance(h, p) } }
        public func proveAccount(address: Data) throws -> Data {
            try consume(address.withC { p, _ in aviary_net_node_prove_account(h, p) }, "net_node_prove_account")
        }
    }
    /// A consensus node for validator `me` of `genesis`, or an observer when `me` is `netObserver`.
    public static func newNetNode(genesis: Data, me: UInt32, blsKey: PortableAccount? = nil,
                                  timestamp: UInt64 = 1, peers: [UInt32] = []) throws -> NetNode {
        ensureReady()
        let pk = blsKey?.publicKey ?? Data(), sk = blsKey?.secret ?? Data()
        guard let raw = genesis.withC({ gp, gl in pk.withC { pp, pl in sk.withC { sp, sl in
            aviary_net_node_new(gp, gl, me, pp, pl, sp, sl, timestamp) } } }) else {
            throw AviaryError(message: "net node: malformed genesis or key") }
        let node = NetNode(handle: raw)
        peers.forEach(node.addPeer)
        return node
    }

    /// A fresh long term identity.
    public static func generateSecureIdentity() throws -> SecureIdentity {
        ensureReady()
        let parts = splitBlobList(try consume(aviary_secure_identity_generate(), "secure_identity_generate"))
        return SecureIdentity(kem: PortableAccount(publicKey: parts[0], secret: parts[1]),
                              sig: PortableAccount(publicKey: parts[2], secret: parts[3]))
    }
    /// A confidential, mutually authenticated, post quantum hybrid link. Pin `peerIdentity`
    /// against the key you expect.
    public final class SecureSession {
        private let h: UnsafeMutableRawPointer
        init(handle: UnsafeMutableRawPointer) { h = handle }
        deinit { aviary_secure_free(h) }
        /// The handshake message this side must send: the first message for an initiator, the
        /// reply for a responder.
        public func handshakeMessage() throws -> Data { try consume(aviary_secure_handshake_message(h), "secure_handshake_message") }
        /// Initiator only: consumes the responder's reply. Throws when the responder does not verify.
        public func complete(reply: Data) throws {
            if reply.withC({ p, l in aviary_secure_complete(h, p, l) }) != 1 {
                throw AviaryError(message: "secure session: responder refused") }
        }
        public var isOpen: Bool { aviary_secure_open(h) == 1 }
        public func peerIdentity() throws -> Data { try consume(aviary_secure_peer_identity(h), "secure_peer_identity") }
        public func encrypt(_ plaintext: Data) throws -> Data { try consume(plaintext.withC { p, l in aviary_secure_encrypt(h, p, l) }, "secure_encrypt") }
        public func decrypt(_ frame: Data) throws -> Data { try consume(frame.withC { p, l in aviary_secure_decrypt(h, p, l) }, "secure_decrypt") }
    }
    /// Dialing side: encapsulates to the responder's KEM public key and authenticates with `sig`.
    public static func initiateSecureSession(responderKemPublic: Data, sig: PortableAccount) throws -> SecureSession {
        ensureReady()
        guard let raw = responderKemPublic.withC({ rp, rl in sig.publicKey.withC { pp, pl in sig.secret.withC { sp, sl in
            aviary_secure_initiate(rp, rl, pp, pl, sp, sl) } } }) else {
            throw AviaryError(message: "secure session: initiate failed") }
        return SecureSession(handle: raw)
    }
    /// Accepting side: consumes the initiator's first message under `me`; open at once.
    public static func acceptSecureSession(me: SecureIdentity, firstMessage: Data) throws -> SecureSession {
        ensureReady()
        guard let raw = me.kem.publicKey.withC({ kp, kl in me.kem.secret.withC { ks, ksl in me.sig.publicKey.withC { sp, sl in me.sig.secret.withC { ss, ssl in firstMessage.withC { mp, ml in
            aviary_secure_accept(kp, kl, ks, ksl, sp, sl, ss, ssl, mp, ml) } } } } }) else {
            throw AviaryError(message: "secure session: initiator refused") }
        return SecureSession(handle: raw)
    }
}
