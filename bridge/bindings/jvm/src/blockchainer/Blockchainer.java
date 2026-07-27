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
            submit, submitFee, produce, height, balance, nonce, mempool, stateRoot, drain, metrics, prove,
            verifyProof, ethAddr, btcAddr, solAddr, bufFree, strFree,
            newAccountHybrid, submitHybrid,
            ksNew, ksAdd, ksReveal, ksSerialise, ksLoad, ksUnlock, ksUnlocked, ksFree,
            kemKeygen, pSealPass, pOpenPass, pSealTo, pOpenTo, pSealSigned, pOpenSigned,
            thCreate, thInfo, thSeal, thUnseal, thCommit, thCerNew, thCerFree, thSign, thVerify, thAgg,
            asDescribe, txEncode, smSeal, smNew, smAdd, smSize, smReveal, smFree;

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
        submitFee  = h("blockchainer_node_submit_transfer_fee", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG, JAVA_LONG));
        newAccountHybrid = h("blockchainer_node_new_account_hybrid", FunctionDescriptor.of(RES, ADDRESS));
        submitHybrid = h("blockchainer_node_submit_transfer_hybrid", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
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
        ksNew      = h("blockchainer_keystore_new", FunctionDescriptor.of(ADDRESS, ADDRESS));
        ksAdd      = h("blockchainer_keystore_add_account", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS));
        ksReveal   = h("blockchainer_keystore_reveal", FunctionDescriptor.of(RES, ADDRESS, ADDRESS));
        ksSerialise= h("blockchainer_keystore_serialise", FunctionDescriptor.of(RES, ADDRESS));
        ksLoad     = h("blockchainer_keystore_load", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG));
        ksUnlock   = h("blockchainer_keystore_unlock", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS));
        ksUnlocked = h("blockchainer_keystore_unlocked", FunctionDescriptor.of(JAVA_INT, ADDRESS));
        ksFree     = h("blockchainer_keystore_free", FunctionDescriptor.ofVoid(ADDRESS));
        kemKeygen  = h("blockchainer_hybrid_kem_keygen", FunctionDescriptor.of(RES));
        pSealPass  = h("blockchainer_portable_seal_passphrase", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        pOpenPass  = h("blockchainer_portable_open_passphrase", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS));
        pSealTo    = h("blockchainer_portable_seal_to", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        pOpenTo    = h("blockchainer_portable_open_to", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        pSealSigned= h("blockchainer_portable_seal_signed_to", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        pOpenSigned= h("blockchainer_portable_open_signed_from", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        thCreate   = h("blockchainer_threshold_create", FunctionDescriptor.of(RES, JAVA_SHORT, JAVA_SHORT));
        thInfo     = h("blockchainer_threshold_share_info", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        thSeal     = h("blockchainer_threshold_share_seal", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        thUnseal   = h("blockchainer_threshold_share_unseal", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS));
        thCommit   = h("blockchainer_threshold_commit", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        thCerNew   = h("blockchainer_threshold_ceremony_new", FunctionDescriptor.of(ADDRESS, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        thCerFree  = h("blockchainer_threshold_ceremony_free", FunctionDescriptor.ofVoid(ADDRESS));
        thSign     = h("blockchainer_threshold_sign", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS));
        thVerify   = h("blockchainer_threshold_verify_share", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_SHORT, ADDRESS, ADDRESS));
        thAgg      = h("blockchainer_threshold_aggregate", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, ADDRESS, JAVA_LONG));
        asDescribe = h("blockchainer_assurance_describe", FunctionDescriptor.of(RES, JAVA_SHORT));
        txEncode   = h("blockchainer_tx_encode", FunctionDescriptor.of(RES, JAVA_BYTE, JAVA_LONG, JAVA_BYTE, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        smSeal     = h("blockchainer_sealed_tx_seal", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        smNew      = h("blockchainer_sealed_mempool_new", FunctionDescriptor.of(ADDRESS, JAVA_LONG));
        smAdd      = h("blockchainer_sealed_mempool_add", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        smSize     = h("blockchainer_sealed_mempool_size", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        smReveal   = h("blockchainer_sealed_mempool_reveal", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, JAVA_LONG));
        smFree     = h("blockchainer_sealed_mempool_free", FunctionDescriptor.ofVoid(ADDRESS));
        try { if ((int) init.invoke() != 0) throw new IllegalStateException("init failed"); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    public record Account(byte[] secret, byte[] publicKey, byte[] address) {}

    public String version()        { return cstr(invokePtr(version)); }
    public String cryptoBackend()  { return cstr(invokePtr(backend)); }

    public String ethAddress(byte[] secret32) { return new String(call1(ethAddr, secret32), StandardCharsets.UTF_8); }
    public String btcAddress(byte[] secret32) { return new String(call1(btcAddr, secret32), StandardCharsets.UTF_8); }
    public String solAddress(byte[] edSecret) { return new String(call1(solAddr, edSecret), StandardCharsets.UTF_8); }

    // ── Portable account export (tiered encrypted backup) ──────────────────────

    /** A signing account's material (or a KEM device key), returned by the open calls. */
    public record PortableAccount(byte[] publicKey, byte[] secret) {}

    private PortableAccount splitBlob(byte[] blob) {
        int pl = ((blob[0] & 0xFF) << 24) | ((blob[1] & 0xFF) << 16) | ((blob[2] & 0xFF) << 8) | (blob[3] & 0xFF);
        return new PortableAccount(java.util.Arrays.copyOfRange(blob, 4, 4 + pl),
                                   java.util.Arrays.copyOfRange(blob, 4 + pl, blob.length));
    }

    /** A hybrid-KEM device keypair for the recipient-bound export tiers. */
    public PortableAccount hybridKemKeypair() {
        try { return splitBlob(consume((MemorySegment) kemKeygen.invoke(arena.allocate(RES)))); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Tier 1 — seal an account under a passphrase (ops=mem=0 → SENSITIVE preset). */
    public byte[] sealAccountPassphrase(byte[] pub, byte[] secret, String passphrase, long ops, long mem) {
        try { return consume((MemorySegment) pSealPass.invoke(arena.allocate(RES), seg(pub), (long) pub.length,
                seg(secret), (long) secret.length, arena.allocateFrom(passphrase), ops, mem)); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
    public PortableAccount openAccountPassphrase(byte[] envelope, String passphrase) {
        try { return splitBlob(consume((MemorySegment) pOpenPass.invoke(arena.allocate(RES),
                seg(envelope), (long) envelope.length, arena.allocateFrom(passphrase)))); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Tier 2 — recipient-bound (PQ): only the device's KEM secret opens it. */
    public byte[] sealAccountTo(byte[] pub, byte[] secret, byte[] recipientKemPublic) {
        try { return consume((MemorySegment) pSealTo.invoke(arena.allocate(RES), seg(pub), (long) pub.length,
                seg(secret), (long) secret.length, seg(recipientKemPublic), (long) recipientKemPublic.length)); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
    public PortableAccount openAccountTo(byte[] envelope, byte[] recipientKemSecret) {
        try { return splitBlob(consume((MemorySegment) pOpenTo.invoke(arena.allocate(RES),
                seg(envelope), (long) envelope.length, seg(recipientKemSecret), (long) recipientKemSecret.length))); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Tier 3 — signed-recipient (Fortress-grade): recipient-bound + the receiver verifies the owner. */
    public byte[] sealAccountSignedTo(byte[] pub, byte[] secret, byte[] recipientKemPublic, byte[] senderSigSecret) {
        try { return consume((MemorySegment) pSealSigned.invoke(arena.allocate(RES), seg(pub), (long) pub.length,
                seg(secret), (long) secret.length, seg(recipientKemPublic), (long) recipientKemPublic.length,
                seg(senderSigSecret), (long) senderSigSecret.length)); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
    public PortableAccount openAccountSignedFrom(byte[] envelope, byte[] recipientKemSecret, byte[] senderSigPublic) {
        try { return splitBlob(consume((MemorySegment) pOpenSigned.invoke(arena.allocate(RES),
                seg(envelope), (long) envelope.length, seg(recipientKemSecret), (long) recipientKemSecret.length,
                seg(senderSigPublic), (long) senderSigPublic.length))); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    // ── Threshold (FROST t-of-n) accounts ──────────────────────────────────────

    /** A participant's share of a threshold account (opaque, self-describing). */
    public record ShareCard(byte[] bytes, int index, byte[] groupPublicKey, byte[] publicShare) {}

    /** Deal a t-of-n account: n share cards, any t of which can sign. */
    public ShareCard[] createThresholdAccount(int n, int t) {
        byte[] blob = consume((MemorySegment) invokeShort2(thCreate, (short) n, (short) t));
        int count = ((blob[0] & 0xFF) << 8) | (blob[1] & 0xFF);
        ShareCard[] cards = new ShareCard[count];
        int off = 2;
        for (int i = 0; i < count; i++) {
            int len = ((blob[off] & 0xFF) << 24) | ((blob[off+1] & 0xFF) << 16) | ((blob[off+2] & 0xFF) << 8) | (blob[off+3] & 0xFF);
            off += 4;
            byte[] card = java.util.Arrays.copyOfRange(blob, off, off + len);
            off += len;
            byte[] info = consume((MemorySegment) invokeResA(thInfo, card));
            int idx = ((info[0] & 0xFF) << 8) | (info[1] & 0xFF);
            cards[i] = new ShareCard(card, idx, java.util.Arrays.copyOfRange(info, 2, 34),
                                     java.util.Arrays.copyOfRange(info, 34, 66));
        }
        return cards;
    }

    /** Seal a share card for rest (ops=mem=0 → SENSITIVE preset). */
    public byte[] sealShare(byte[] card, String passphrase, long ops, long mem) {
        try { return consume((MemorySegment) thSeal.invoke(arena.allocate(RES), seg(card), (long) card.length,
                arena.allocateFrom(passphrase), ops, mem)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }
    public byte[] unsealShare(byte[] envelope, String passphrase) {
        try { return consume((MemorySegment) thUnseal.invoke(arena.allocate(RES), seg(envelope), (long) envelope.length,
                arena.allocateFrom(passphrase))); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** Round 1 for a card: [nonces 64][commitment 66]. Keep nonces; broadcast commitment. */
    public byte[] commit(byte[] card) {
        try { return consume((MemorySegment) thCommit.invoke(arena.allocate(RES), seg(card), (long) card.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** A two-round signing ceremony over a fixed message + roster. */
    public final class Ceremony implements AutoCloseable {
        private final MemorySegment h;
        Ceremony(byte[] groupPK, byte[] message, byte[][] commitments) {
            int total = 0; for (byte[] c : commitments) total += c.length;
            byte[] roster = new byte[total]; int o = 0;
            for (byte[] c : commitments) { System.arraycopy(c, 0, roster, o, c.length); o += c.length; }
            try {
                h = (MemorySegment) thCerNew.invoke(seg(groupPK), seg(message), (long) message.length, seg(roster), (long) roster.length);
            } catch (Throwable e) { throw new RuntimeException(e); }
            if (h.address() == 0) throw new IllegalStateException("threshold ceremony creation failed");
        }
        /** Round 2 for a card using its round-1 nonces: a 32-byte share. */
        public byte[] sign(byte[] card, byte[] nonces) {
            try { return consume((MemorySegment) thSign.invoke(arena.allocate(RES), h, seg(card), (long) card.length, seg(nonces))); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Attribute a share to participant [id] (public share 32 B). */
        public boolean verifyShare(int id, byte[] publicShare, byte[] sigShare) {
            try { return (int) thVerify.invoke(h, (short) id, seg(publicShare), seg(sigShare)) == 1; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Aggregate (id → 32-byte share) pairs into a 64-byte Ed25519 signature. */
        public byte[] aggregate(int[] ids, byte[][] sigShares) {
            MemorySegment idSeg = arena.allocate(JAVA_SHORT, ids.length);
            for (int i = 0; i < ids.length; i++) idSeg.setAtIndex(JAVA_SHORT, i, (short) ids[i]);
            byte[] flat = new byte[sigShares.length * 32]; int o = 0;
            for (byte[] s : sigShares) { System.arraycopy(s, 0, flat, o, 32); o += 32; }
            try { return consume((MemorySegment) thAgg.invoke(arena.allocate(RES), h, idSeg, seg(flat), (long) ids.length)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        @Override public void close() { try { thCerFree.invoke(h); } catch (Throwable ignored) {} }
    }
    public Ceremony newCeremony(byte[] groupPK, byte[] message, byte[][] commitments) {
        return new Ceremony(groupPK, message, commitments);
    }

    private Object invokeShort2(MethodHandle mh, short a, short b) {
        try { return mh.invoke(arena.allocate(RES), a, b); } catch (Throwable e) { throw new RuntimeException(e); }
    }
    private Object invokeResA(MethodHandle mh, byte[] a) {
        try { return mh.invoke(arena.allocate(RES), seg(a), (long) a.length); } catch (Throwable e) { throw new RuntimeException(e); }
    }

    // ── AssuranceProfile + SealedMempool ───────────────────────────────────────

    /** The tiers a security posture mandates at every seam. */
    public record AssuranceProfile(boolean ratcheted, boolean vrf, boolean thresholdCustody,
                                   boolean sealedMempool, byte[] sigAlgos) {}

    /** Describe a posture: 1 = standard, 2 = hardened, 3 = fortress. */
    public AssuranceProfile describeAssurance(int level) {
        byte[] b;
        try { b = consume((MemorySegment) asDescribe.invoke(arena.allocate(RES), (short) level)); }
        catch (Throwable e) { throw new RuntimeException(e); }
        int n = b[4] & 0xFF;
        return new AssuranceProfile(b[0] == 1, b[1] == 1, b[2] == 1, b[3] == 1,
                                    java.util.Arrays.copyOfRange(b, 5, 5 + n));
    }

    /** Encode a transaction from its fields (e.g. to seal into a SealedMempool). */
    public byte[] encodeTx(int version, long chainId, int sigAlgo, long fee, byte[] body, byte[] author, byte[] signature) {
        try { return consume((MemorySegment) txEncode.invoke(arena.allocate(RES), (byte) version, chainId, (byte) sigAlgo, fee,
                seg(body), (long) body.length, seg(author), (long) author.length, seg(signature), (long) signature.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }
    /** Seal an encoded transaction to the proposer's hybrid-KEM public key. */
    public byte[] sealTx(byte[] txEncoded, byte[] proposerKemPublic) {
        try { return consume((MemorySegment) smSeal.invoke(arena.allocate(RES), seg(txEncoded), (long) txEncoded.length,
                seg(proposerKemPublic), (long) proposerKemPublic.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** An encrypted mempool: submissions stay sealed to the proposer until reveal. */
    public final class SealedMempool implements AutoCloseable {
        private final MemorySegment h;
        SealedMempool(long capacity) {
            try { h = (MemorySegment) smNew.invoke(capacity); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public boolean add(byte[] sealedTx) {
            try { return (int) smAdd.invoke(h, seg(sealedTx), (long) sealedTx.length) == 1; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        public long size() { try { return (long) smSize.invoke(h); } catch (Throwable e) { throw new RuntimeException(e); } }
        public byte[][] reveal(byte[] proposerKemSecret, long max) {
            byte[] blob;
            try { blob = consume((MemorySegment) smReveal.invoke(arena.allocate(RES), h, seg(proposerKemSecret), (long) proposerKemSecret.length, max)); }
            catch (Throwable e) { throw new RuntimeException(e); }
            int count = ((blob[0] & 0xFF) << 24) | ((blob[1] & 0xFF) << 16) | ((blob[2] & 0xFF) << 8) | (blob[3] & 0xFF);
            byte[][] txs = new byte[count][]; int off = 4;
            for (int i = 0; i < count; i++) {
                int len = ((blob[off] & 0xFF) << 24) | ((blob[off+1] & 0xFF) << 16) | ((blob[off+2] & 0xFF) << 8) | (blob[off+3] & 0xFF);
                off += 4; txs[i] = java.util.Arrays.copyOfRange(blob, off, off + len); off += len;
            }
            return txs;
        }
        @Override public void close() { try { smFree.invoke(h); } catch (Throwable ignored) {} }
    }
    public SealedMempool newSealedMempool(long capacity) { return new SealedMempool(capacity); }

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

    /** Create a custodial key store protected by {@code passphrase} (unlocked). */
    public KeyStore createKeyStore(String passphrase) {
        try {
            MemorySegment h = (MemorySegment) ksNew.invoke(arena.allocateFrom(passphrase));
            if (h.address() == 0) throw new IllegalStateException("keystore create failed");
            return new KeyStore(h);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Load a serialised key store. It comes back LOCKED — call unlock(). */
    public KeyStore loadKeyStore(byte[] blob) {
        try {
            MemorySegment h = (MemorySegment) ksLoad.invoke(seg(blob), (long) blob.length);
            if (h.address() == 0) throw new IllegalStateException("keystore load failed");
            return new KeyStore(h);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** A custodial store that seals account signing secrets at rest. */
    public final class KeyStore implements AutoCloseable {
        private final MemorySegment ks;
        private KeyStore(MemorySegment ks) { this.ks = ks; }

        /** Seal an account's secret; returns its 32-byte address. Requires unlocked. */
        public byte[] addAccount(byte[] pub, byte[] secret) {
            try {
                MemorySegment out = arena.allocate(32);
                int rc = (int) ksAdd.invoke(ks, seg(pub), (long) pub.length, seg(secret),
                                            (long) secret.length, out);
                if (rc != 0) throw new IllegalStateException("keystore add account failed (locked?)");
                return out.reinterpret(32).toArray(JAVA_BYTE);
            } catch (Throwable t) { throw new RuntimeException(t); }
        }

        /** Decrypt the sealed secret for {@code address}. Requires unlocked. */
        public byte[] reveal(byte[] address) {
            try { return consume((MemorySegment) ksReveal.invoke(arena.allocate(RES), ks, seg(address))); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }

        /** The persistable (encrypted) blob — no plaintext key. */
        public byte[] serialise() {
            try { return consume((MemorySegment) ksSerialise.invoke(arena.allocate(RES), ks)); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }

        /** Recover the master key from {@code passphrase}. Throws on a wrong passphrase. */
        public void unlock(String passphrase) {
            try {
                if ((int) ksUnlock.invoke(ks, arena.allocateFrom(passphrase)) != 0) {
                    throw new IllegalStateException("keystore unlock failed");
                }
            } catch (Throwable t) { throw new RuntimeException(t); }
        }

        /** Whether secrets can currently be revealed. */
        public boolean unlocked() {
            try { return (int) ksUnlocked.invoke(ks) == 1; } catch (Throwable t) { throw new RuntimeException(t); }
        }

        @Override public void close() { try { ksFree.invoke(ks); } catch (Throwable ignored) {} }
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
        /** Fresh hybrid (Ed25519 + ML-DSA-65) post-quantum account. */
        public Account newAccountHybrid() {
            byte[] blob = consume(invokeRes(newAccountHybrid, node));
            // [pub_len: 4 bytes big-endian][public][secret]
            int pl = ((blob[0] & 0xFF) << 24) | ((blob[1] & 0xFF) << 16) | ((blob[2] & 0xFF) << 8) | (blob[3] & 0xFF);
            byte[] pub = java.util.Arrays.copyOfRange(blob, 4, 4 + pl);
            byte[] sec = java.util.Arrays.copyOfRange(blob, 4 + pl, blob.length);
            return new Account(sec, pub, accountAddress(pub));
        }
        /** Build, sign, and admit a hybrid (post-quantum) transfer. */
        public byte[] submitTransferHybrid(byte[] secret, byte[] pub, byte[] to, long amount, long n) {
            try {
                return consume((MemorySegment) submitHybrid.invoke(arena.allocate(RES), node,
                    seg(secret), (long) secret.length, seg(pub), (long) pub.length, seg(to), amount, n));
            } catch (Throwable t) { throw new RuntimeException(t); }
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
        /** Ed25519 transfer carrying a priority-fee tip (version-2 envelope) for fee-market priority. */
        public byte[] submitTransferFee(byte[] secret, byte[] to, long amount, long n, long fee) {
            try { return consume((MemorySegment) submitFee.invoke(arena.allocate(RES), node, seg(secret), (long) secret.length, seg(to), amount, n, fee)); }
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
