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
