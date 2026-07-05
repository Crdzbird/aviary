package blockchainer;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.charset.StandardCharsets;
import static java.lang.foreign.ValueLayout.*;

/**
 * Blockchainer JVM binding (Java FFM / Panama). Drives the full node — create a
 * chain, accounts, transfer, produce blocks, stream events, metrics, light-client
 * verification — plus EVM/Bitcoin/Solana interop, over the C ABI. Usable from
 * Java and Kotlin alike.
 */
public final class Blockchainer implements AutoCloseable {
    private final Arena arena = Arena.ofShared();
    private final Linker linker = Linker.nativeLinker();
    private final SymbolLookup lib;

    static final GroupLayout BUF = MemoryLayout.structLayout(ADDRESS.withName("data"), JAVA_LONG.withName("len"));
    static final GroupLayout RES = MemoryLayout.structLayout(BUF.withName("buf"), ADDRESS.withName("error"));
    static final long B = BUF.byteSize();

    private final MethodHandle init, version, backend, nodeNew, nodeFree, newAccount, addr, credit,
            submit, produce, height, balance, nonce, mempool, stateRoot, drain, metrics, prove,
            verifyProof, ethAddr, btcAddr, solAddr, bufFree, strFree;

    public Blockchainer() {
        lib = SymbolLookup.libraryLookup(NativeLoader.resolve(), arena);
        init       = h("blockchainer_init", FunctionDescriptor.of(JAVA_INT));
        version    = h("blockchainer_version", FunctionDescriptor.of(ADDRESS));
        backend    = h("blockchainer_crypto_backend_version", FunctionDescriptor.of(ADDRESS));
        nodeNew    = h("blockchainer_node_new", FunctionDescriptor.of(ADDRESS, JAVA_LONG));
        nodeFree   = h("blockchainer_node_free", FunctionDescriptor.ofVoid(ADDRESS));
        newAccount = h("blockchainer_node_new_account", FunctionDescriptor.of(RES, ADDRESS));
        addr       = h("blockchainer_account_address", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        credit     = h("blockchainer_node_credit", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        submit     = h("blockchainer_node_submit_transfer", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        produce    = h("blockchainer_node_produce_block", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS));
        height     = h("blockchainer_node_height", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        balance    = h("blockchainer_node_balance", FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS));
        nonce      = h("blockchainer_node_nonce", FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS));
        mempool    = h("blockchainer_node_mempool_size", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        stateRoot  = h("blockchainer_node_state_root", FunctionDescriptor.of(RES, ADDRESS));
        drain      = h("blockchainer_node_drain_events", FunctionDescriptor.of(RES, ADDRESS));
        metrics    = h("blockchainer_node_metrics", FunctionDescriptor.of(RES, ADDRESS));
        prove      = h("blockchainer_node_prove_account", FunctionDescriptor.of(RES, ADDRESS, ADDRESS));
        verifyProof= h("blockchainer_verify_state_proof", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        ethAddr    = h("blockchainer_eth_address", FunctionDescriptor.of(RES, ADDRESS));
        btcAddr    = h("blockchainer_btc_address", FunctionDescriptor.of(RES, ADDRESS));
        solAddr    = h("blockchainer_sol_address", FunctionDescriptor.of(RES, ADDRESS));
        bufFree    = h("blockchainer_buffer_free", FunctionDescriptor.ofVoid(ADDRESS));
        strFree    = h("blockchainer_str_free", FunctionDescriptor.ofVoid(ADDRESS));
        try { if ((int) init.invoke() != 0) throw new IllegalStateException("init failed"); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    public record Account(byte[] secret, byte[] publicKey, byte[] address) {}

    public String version()        { return cstr(invokePtr(version)); }
    public String cryptoBackend()  { return cstr(invokePtr(backend)); }

    public String ethAddress(byte[] secret32) { return new String(call1(ethAddr, secret32), StandardCharsets.UTF_8); }
    public String btcAddress(byte[] secret32) { return new String(call1(btcAddr, secret32), StandardCharsets.UTF_8); }
    public String solAddress(byte[] edSecret) { return new String(call1(solAddr, edSecret), StandardCharsets.UTF_8); }

    public boolean verifyStateProof(byte[] root, byte[] proof) {
        try { return (int) verifyProof.invoke(seg(root), seg(proof), (long) proof.length) == 1; }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    public Node createNode(long chainId) {
        try {
            MemorySegment h = (MemorySegment) nodeNew.invoke(chainId);
            if (h.address() == 0) throw new IllegalStateException("node create failed");
            return new Node(h);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** A running chain node. */
    public final class Node implements AutoCloseable {
        private final MemorySegment node;
        private Node(MemorySegment node) { this.node = node; }

        public Account newAccount() {
            byte[] secret = consume(invokeRes(newAccount, node));
            byte[] pub = java.util.Arrays.copyOfRange(secret, 32, 64);
            return new Account(secret, pub, accountAddress(pub));
        }
        public byte[] accountAddress(byte[] pub) {
            try { return consume((MemorySegment) addr.invoke(arena.allocate(RES), seg(pub), (long) pub.length)); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }
        // Valid until the first block is produced (reads do NOT seal genesis);
        // throws afterwards — never a silent no-op.
        public void credit(byte[] address, long amount) {
            int rc;
            try { rc = (int) credit.invoke(node, seg(address), amount); }
            catch (Throwable t) { throw new RuntimeException(t); }
            if (rc == 2) throw new IllegalStateException(
                "genesis is sealed (a block was produced); fund accounts with transfers");
            if (rc != 0) throw new RuntimeException("credit failed (status " + rc + ")");
        }
        public byte[] submitTransfer(byte[] secret, byte[] to, long amount, long n) {
            try { return consume((MemorySegment) submit.invoke(arena.allocate(RES), node, seg(secret), (long) secret.length, seg(to), amount, n)); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }
        public long produceBlock(long ts) {
            try {
                MemorySegment out = arena.allocate(JAVA_LONG);
                if ((int) produce.invoke(node, ts, out) != 0) throw new IllegalStateException("produce failed");
                return out.get(JAVA_LONG, 0);
            } catch (Throwable t) { throw new RuntimeException(t); }
        }
        public long height()      { return u64(height); }
        public long mempoolSize() { return u64(mempool); }
        public long balance(byte[] a) { return u64a(balance, a); }
        public long nonce(byte[] a)   { return u64a(nonce, a); }
        public byte[] stateRoot()  { return consume(invokeRes(stateRoot, node)); }
        public String drainEvents(){ return new String(consume(invokeRes(drain, node)), StandardCharsets.UTF_8); }
        public String metrics()    { return new String(consume(invokeRes(metrics, node)), StandardCharsets.UTF_8); }
        public byte[] proveAccount(byte[] a) {
            try { return consume((MemorySegment) prove.invoke(arena.allocate(RES), node, seg(a))); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }
        private long u64(MethodHandle mh) { try { return (long) mh.invoke(node); } catch (Throwable t) { throw new RuntimeException(t); } }
        private long u64a(MethodHandle mh, byte[] a) { try { return (long) mh.invoke(node, seg(a)); } catch (Throwable t) { throw new RuntimeException(t); } }
        public void close() { try { nodeFree.invoke(node); } catch (Throwable ignored) {} }
    }

    // ── FFM plumbing ─────────────────────────────────────────────────────────
    private MethodHandle h(String name, FunctionDescriptor fd) {
        return linker.downcallHandle(lib.find(name).orElseThrow(
                () -> new IllegalStateException("symbol not found: " + name)), fd);
    }
    private MemorySegment seg(byte[] b) {
        MemorySegment s = arena.allocate(b.length == 0 ? 1 : b.length);
        MemorySegment.copy(b, 0, s, JAVA_BYTE, 0, b.length);
        return s;
    }
    private MemorySegment invokePtr(MethodHandle mh) { try { return (MemorySegment) mh.invoke(); } catch (Throwable t) { throw new RuntimeException(t); } }
    private MemorySegment invokeRes(MethodHandle mh, MemorySegment node) {
        try { return (MemorySegment) mh.invoke(arena.allocate(RES), node); } catch (Throwable t) { throw new RuntimeException(t); }
    }
    private byte[] call1(MethodHandle mh, byte[] in) {
        try { return consume((MemorySegment) mh.invoke(arena.allocate(RES), seg(in))); } catch (Throwable t) { throw new RuntimeException(t); }
    }
    private String cstr(MemorySegment p) { return p.reinterpret(Long.MAX_VALUE).getString(0); }

    private byte[] consume(MemorySegment res) {
        MemorySegment data = res.get(ADDRESS, 0);
        long len = res.get(JAVA_LONG, ADDRESS.byteSize());
        MemorySegment err = res.get(ADDRESS, RES.byteOffset(MemoryLayout.PathElement.groupElement("error")));
        if ((data.address() == 0 || len == 0) && err.address() != 0) {
            String msg = err.reinterpret(Long.MAX_VALUE).getString(0);
            try { strFree.invoke(err); } catch (Throwable ignored) {}
            throw new RuntimeException(msg);
        }
        byte[] out = (data.address() == 0 || len == 0) ? new byte[0] : data.reinterpret(len).toArray(JAVA_BYTE);
        try {
            MemorySegment bufCopy = arena.allocate(BUF);
            MemorySegment.copy(res, 0, bufCopy, 0, B);
            bufFree.invoke(bufCopy);
        } catch (Throwable t) { throw new RuntimeException(t); }
        return out;
    }

    public void close() { arena.close(); }
}
