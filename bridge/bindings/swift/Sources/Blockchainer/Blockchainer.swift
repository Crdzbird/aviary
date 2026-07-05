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
