import AviaryC
import Foundation

extension Aviary {
    /// Rebuilds a chain from the genesis it was assembled from and a `Chain.snapshot()`, as
    /// validator `me`. A restored chain produces byte for byte what the original would. Throws
    /// with the reason when the snapshot is refused.
    public static func restoreChain(genesis: Data, me: UInt32, blsPub: Data, blsSec: Data, snapshot: Data) throws -> Chain {
        ensureReady()
        let raw = genesis.withC { gp, gl in blsPub.withC { pp, pl in blsSec.withC { sp, sl in snapshot.withC { np, nl in
            aviary_chain_restore(gp, gl, me, pp, pl, sp, sl, np, nl) } } } }
        if let raw { return Chain(raw: raw) }
        let reason = chainSnapshotError(genesis: genesis, snapshot: snapshot)
        throw AviaryError(message: reason.isEmpty ? "chain restore refused" : "chain restore refused: " + reason)
    }
    /// Why `restoreChain` would refuse a snapshot under a genesis; "" means it would be accepted.
    /// Needs no key.
    public static func chainSnapshotError(genesis: Data, snapshot: Data) -> String {
        ensureReady()
        do {
            return String(decoding: try consume(genesis.withC { gp, gl in snapshot.withC { sp, sl in
                aviary_chain_snapshot_error(gp, gl, sp, sl) } }, "chain_snapshot_error"), as: UTF8.self)
        } catch { return (error as? AviaryError)?.message ?? "rejected" }
    }
    /// Rebuilds a chain tree from the SAME root node it was built from and a `ChainTree.snapshot()`.
    /// Hooks are not restored; set `onFinalized` and `txSource` again.
    public static func restoreChainTree(_ root: TreeNode, snapshot: Data) throws -> ChainTree {
        ensureReady()
        let spec = try root.encode()
        guard let raw = spec.withC({ sp, sl in snapshot.withC { np, nl in aviary_chain_tree_restore(sp, sl, np, nl) } }) else {
            throw AviaryError(message: "chain tree restore refused (another family, or a malformed snapshot)") }
        return ChainTree(raw: raw)
    }
}

extension Aviary.Chain {
    private var handle: UnsafeMutableRawPointer { UnsafeMutableRawPointer(h) }

    /// The current tip header (the genesis header before any block).
    public func tip() throws -> Data { try consume(aviary_chain_tip(handle), "chain_tip") }
    /// Everything this chain needs to resume: ledger, tip receipts, consensus state and, when
    /// enabled, the block archive. Bound to the genesis hash, holds no secret key, and leaves
    /// out transactions queued with `addTx`.
    public func snapshot() throws -> Data { try consume(aviary_chain_snapshot(handle), "chain_snapshot") }
    /// Keeps every block committed from now on, with its certificate, in the handle and its
    /// snapshots. BFT chains only.
    public func enableArchive() throws {
        if aviary_chain_archive_enable(handle) != 1 { throw AviaryError(message: "chain: the archive needs a BFT engine") }
    }
    /// The archived height range, or nil while the archive is empty.
    public func archiveRange() -> (first: UInt64, last: UInt64)? {
        var first: UInt64 = 0, last: UInt64 = 0
        guard aviary_chain_archive_range(handle, &first, &last) == 1 else { return nil }
        return (first, last)
    }
    /// The archived encoded block at a height.
    public func archivedBlock(_ height: UInt64) throws -> Data {
        try consume(aviary_chain_archived_block(handle, height), "chain_archived_block")
    }
    /// The commit certificate of the archived block at a height.
    public func archivedCertificate(_ height: UInt64) throws -> Data {
        try consume(aviary_chain_archived_certificate(handle, height), "chain_archived_certificate")
    }
    /// Drops archived blocks below a height and returns how many were dropped.
    @discardableResult
    public func pruneArchive(below height: UInt64) -> UInt64 { aviary_chain_archive_prune(handle, height) }
}

extension Aviary.ChainTree {
    /// The whole family as one blob: every node's chain snapshot. Holds no secret key.
    public func snapshot() throws -> Data {
        try consume(aviary_chain_tree_snapshot(UnsafeMutableRawPointer(h)), "chain_tree_snapshot")
    }
}
