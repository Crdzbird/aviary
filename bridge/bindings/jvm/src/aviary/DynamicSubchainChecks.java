package aviary;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.List;
import java.util.Map;

/** Checks for subchains version 2 and chain persistence, run by Main. They mirror the Go binding's dynamic_subchain_test.go. */
final class DynamicSubchainChecks {
    private final Aviary bc;
    private final byte[] registrarSecret;

    private DynamicSubchainChecks(Aviary bc, byte[] registrarSecret) { this.bc = bc; this.registrarSecret = registrarSecret; }

    static void run(Aviary bc) throws Exception {
        Map<String, String> want = loadVectors();
        if (want == null) {
            System.out.println("subchain-v2 vectors skipped (vectors/subchain_v2_vectors.txt not found)");
        } else {
            new DynamicSubchainChecks(bc, HexFormat.of().parseHex(want.get("registrar_secret"))).vectors(want);
            System.out.println("subchain-v2 vectors OK (" + want.size() + " lines)");
        }
        restartAndRejections(bc);
        System.out.println("subchain-v2 restart and rejections OK");
        treeSnapshot(bc);
        System.out.println("chain-tree snapshot OK");
    }

    private static Map<String, String> loadVectors() throws Exception {
        String override = System.getProperty("aviary.vectors");
        Path file = override != null ? Path.of(override) : null;
        for (Path dir = Path.of(System.getProperty("user.dir")).toAbsolutePath(); file == null && dir != null; dir = dir.getParent()) {
            Path candidate = dir.resolve("vectors/subchain_v2_vectors.txt");
            if (Files.exists(candidate)) file = candidate;
        }
        if (file == null || !Files.exists(file)) return null;
        var out = new java.util.LinkedHashMap<String, String>();
        for (String raw : Files.readAllLines(file, StandardCharsets.UTF_8)) {
            String line = raw.strip();
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] fields = line.split("\\s+");
            if (fields.length != 2) throw new IllegalStateException("malformed vector line: " + line);
            out.put(fields[0], fields[1]);
        }
        return out;
    }

    private byte[] recordGenesis(long chainId, Aviary.PortableAccount v) {
        byte[] base = bc.buildGenesis(chainId, 1, 1, 2, new byte[0][], new long[0], new byte[][]{v.publicKey()}, new long[]{1});
        return bc.commitBlueprint(base, bc.new Blueprint("bft", "record", "", null));
    }

    private Aviary.Chain assemble(byte[] genesis, Aviary.PortableAccount v) {
        Aviary.Chain c = bc.assembleChain(genesis, 0, v.publicKey(), v.secret());
        if (c == null || !c.enableArchive()) throw new IllegalStateException("assemble with an archive");
        return c;
    }

    private static byte[] commit(Aviary.Chain c, byte[] block) {
        if (!c.submit(block, c.certify(block))) throw new IllegalStateException("block refused");
        return block;
    }

    private byte[] registration(long chainId, byte[] genesis, Aviary.PortableAccount v) {
        return bc.encodeChildRegistration(chainId, bc.genesisHeader(genesis), new byte[][]{v.publicKey()}, new long[]{1}, 0);
    }

    private byte[] sign(byte[] op, long parentId, byte[] parentHash) {
        return bc.signOp(op, parentId, parentHash, 0, Aviary.SIG_ED25519, registrarSecret);
    }

    private byte[] policy() {
        var p = new Aviary.DynamicSubchainPolicy();
        p.maxDepth = 1; p.threshold = 1;
        p.registrars = List.of(new Aviary.Registrar(Aviary.SIG_ED25519, Arrays.copyOfRange(registrarSecret, 32, 64)));
        return bc.encodeDynamicSubchainPolicy(p);
    }

    private static byte[] filled(int value) { byte[] b = new byte[32]; Arrays.fill(b, (byte) value); return b; }

    private static void expect(boolean ok, String what) { if (!ok) throw new IllegalStateException(what); }

    private static void refused(Runnable call, String reason) {
        try { call.run(); } catch (RuntimeException e) {
            if (reason == null || String.valueOf(e.getMessage()).contains(reason)) return;
            throw new IllegalStateException("refused for another reason: " + e.getMessage());
        }
        throw new IllegalStateException("must be refused" + (reason == null ? "" : ": " + reason));
    }

    private void vectors(Map<String, String> want) {
        HexFormat hex = HexFormat.of();
        int[] seen = {0};
        java.util.function.BiConsumer<String, byte[]> check = (name, got) -> {
            seen[0]++;
            if (!hex.formatHex(got).equals(want.get(name))) throw new IllegalStateException("vector " + name + ": got " + hex.formatHex(got));
        };
        var server = bc.newBLSValidator(filled(0x01));
        var user = bc.newBLSValidator(filled(0x10));
        check.accept("registrar_secret", registrarSecret);

        byte[] policy = policy();
        check.accept("policy", policy);
        byte[] mainGenesis = bc.commitSubchainPolicy(recordGenesis(1, server), policy);
        byte[] mainHash = bc.genesisHash(mainGenesis);
        check.accept("main_genesis_hash", mainHash);
        check.accept("main_genesis_header", bc.genesisHeader(mainGenesis));

        byte[] userGenesis = bc.commitParentBinding(recordGenesis(1001, user), 1, mainHash);
        byte[] registration = registration(1001, userGenesis, user);
        check.accept("registration", registration);
        byte[] opRegister = bc.opRegister(0, 0, registration);
        check.accept("op_register", opRegister);
        check.accept("op_register_digest", bc.opDigest(opRegister, 1, mainHash));
        byte[] opRegisterSigned = sign(opRegister, 1, mainHash);
        check.accept("op_register_signed", opRegisterSigned);

        try (var mainChain = assemble(mainGenesis, server); var userChain = assemble(userGenesis, user)) {
            byte[] header1 = bc.blockHeader(commit(mainChain, mainChain.proposeSubchain(10, null, new byte[][]{opRegisterSigned})));
            check.accept("header_1", header1);
            check.accept("header_1_hash", bc.encodedHeaderHash(header1));
            check.accept("subchain_root_1", bc.headerSubchainRoot(header1));
            check.accept("child_record_1", mainChain.childRecord(1001).encoded());

            byte[] key = filled(0x42);
            userChain.addTx(bc.signRecordTx(1001, 0, key, Aviary.RecordOp.PUT, "door opened".getBytes(StandardCharsets.UTF_8), registrarSecret));
            commit(userChain, userChain.propose(20));
            commit(userChain, userChain.propose(21));
            byte[] anchor = bc.pendingAnchor(mainChain, userChain, 0);
            check.accept("anchor", anchor);
            byte[] header2 = bc.blockHeader(commit(mainChain, mainChain.proposeSubchain(11, new byte[][]{anchor}, null)));
            check.accept("header_2_hash", bc.encodedHeaderHash(header2));
            byte[] root2 = bc.headerSubchainRoot(header2);
            check.accept("subchain_root_2", root2);
            var record2 = mainChain.childRecord(1001);
            check.accept("child_record_2", record2.encoded());
            byte[] proof2 = mainChain.childProof(1001);
            check.accept("child_proof_2", proof2);
            var proven = bc.verifyChildProof(root2, proof2);
            check.accept("child_proof_2_verified", proven.encoded());
            expect(proven.tipHeight() == 2 && proven.isOpen() && Arrays.equals(proven.tipHash(), bc.encodedHeaderHash(userChain.tip())), "proven record");
            byte[] recordProof = userChain.proveRecord(key);
            expect(bc.verifyStateProof(proven.tipStateRoot(), recordProof), "record proof under the proven tip");
            byte[] path = bc.encodeSubchainProofV2(new byte[][]{mainChain.subchainHopV2(1001)}, recordProof);
            check.accept("path_proof_2", path);
            expect(bc.verifySubchainProofV2(root2, path) && !bc.verifySubchainProofV2(bc.headerSubchainRoot(header1), path), "path proof");
            check.accept("main_snapshot_hash_2", bc.hash(mainChain.snapshot()));
            check.accept("user_snapshot_hash_2", bc.hash(userChain.snapshot()));

            byte[] opClose = bc.opClose(0, 0, 1001, record2.tipHeight(), record2.tipHash());
            check.accept("op_close", opClose);
            check.accept("op_close_digest", bc.opDigest(opClose, 1, mainHash));
            byte[] header3 = bc.blockHeader(commit(mainChain, mainChain.proposeSubchain(12, null, new byte[][]{sign(opClose, 1, mainHash)})));
            check.accept("header_3_hash", bc.encodedHeaderHash(header3));
            check.accept("subchain_root_3", bc.headerSubchainRoot(header3));
            var record3 = mainChain.childRecord(1001);
            check.accept("child_record_3", record3.encoded());
            expect(record3.status() == Aviary.CHILD_CLOSED && record3.closedHeight() == 3, "closed record");
            check.accept("main_snapshot_hash_3", bc.hash(mainChain.snapshot()));
        }

        byte[] wideGenesis = bc.commitSubchainPolicy(recordGenesis(2, server), policy);
        byte[] wideHash = bc.genesisHash(wideGenesis);
        try (var wide = assemble(wideGenesis, server)) {
            byte[][] ops = new byte[11][];
            for (int i = 0; i < ops.length; i++) {
                long id = 2000 + i;
                ops[i] = sign(bc.opRegister(0, 0, registration(id, recordGenesis(id, user), user)), 2, wideHash);
            }
            byte[] wideRoot = bc.headerSubchainRoot(bc.blockHeader(commit(wide, wide.proposeSubchain(10, null, ops))));
            check.accept("wide_root", wideRoot);
            byte[] wideProof = wide.childProof(2007);
            check.accept("wide_proof", wideProof);
            var wideRecord = bc.verifyChildProof(wideRoot, wideProof);
            check.accept("wide_proof_verified", wideRecord.encoded());
            expect(wideRecord.chainId() == 2007 && wideRecord.index() == 7, "wide record");
        }
        expect(seen[0] == want.size(), "replayed " + seen[0] + " vectors, the file pins " + want.size());
    }

    private static void restartAndRejections(Aviary bc) {
        try (Aviary.Node node = bc.createNode(0x5501)) {
            byte[] secret = node.newAccount().secret(), stranger = node.newAccount().secret();
            var f = new DynamicSubchainChecks(bc, secret);
            var server = bc.newBLSValidator(filled(0x02));
            var user = bc.newBLSValidator(filled(0x12));
            byte[] mainGenesis = bc.commitSubchainPolicy(f.recordGenesis(7, server), f.policy());
            byte[] mainHash = bc.genesisHash(mainGenesis);
            byte[] userGenesis = bc.commitParentBinding(f.recordGenesis(7001, user), 7, mainHash);
            try (var mainChain = f.assemble(mainGenesis, server); var userChain = f.assemble(userGenesis, user)) {
                var counts = mainChain.registryCounts();
                expect(counts != null && counts.records() == 0 && counts.authorityEpoch() == 0, "empty dynamic registry");
                expect(userChain.registryCounts() == null, "a leaf chain has no registry");

                byte[] op = bc.opRegister(0, 0, f.registration(7001, userGenesis, user));
                refused(() -> mainChain.proposeSubchain(10, null, new byte[][]{op}), "not enough approvals");
                byte[] forged = bc.signOp(op, 7, mainHash, 0, Aviary.SIG_ED25519, stranger);
                refused(() -> mainChain.proposeSubchain(10, null, new byte[][]{forged}), "bad registrar signature");
                byte[] otherParent = mainHash.clone(); otherParent[0] ^= 1;
                byte[] replayed = bc.signOp(op, 7, otherParent, 0, Aviary.SIG_ED25519, secret);
                refused(() -> mainChain.proposeSubchain(10, null, new byte[][]{replayed}), null);
                expect(bc.opDigest(op, 7, mainHash).length == 32, "digest");
                byte[] signed = f.sign(op, 7, mainHash);
                refused(() -> bc.signOp(signed, 7, mainHash, 0, Aviary.SIG_ED25519, secret), null);
                commit(mainChain, mainChain.proposeSubchain(10, null, new byte[][]{signed}));
                counts = mainChain.registryCounts();
                expect(counts.records() == 1 && counts.open() == 1, "one open child");
                commit(userChain, userChain.propose(20));
                commit(mainChain, mainChain.proposeSubchain(11, new byte[][]{bc.pendingAnchor(mainChain, userChain, 0)}, null));
                expect(bc.pendingAnchor(mainChain, userChain, 0) == null, "nothing pending after anchoring");

                byte[] mainSnapshot = mainChain.snapshot(), userSnapshot = userChain.snapshot();
                expect(bc.chainSnapshotError(mainGenesis, mainSnapshot).isEmpty(), "a good snapshot has no error");
                try (var mainBack = bc.restoreChain(mainGenesis, 0, server.publicKey(), server.secret(), mainSnapshot);
                     var userBack = bc.restoreChain(userGenesis, 0, user.publicKey(), user.secret(), userSnapshot)) {
                    expect(Arrays.equals(mainBack.snapshot(), mainSnapshot) && Arrays.equals(mainBack.tip(), mainChain.tip()), "restored chain differs");
                    for (var u : List.of(userChain, userBack)) commit(u, u.propose(21));
                    byte[] straight = commit(mainChain, mainChain.proposeSubchain(12, new byte[][]{bc.pendingAnchor(mainChain, userChain, 0)}, null));
                    byte[] resumed = commit(mainBack, mainBack.proposeSubchain(12, new byte[][]{bc.pendingAnchor(mainBack, userBack, 0)}, null));
                    expect(Arrays.equals(straight, resumed) && Arrays.equals(mainBack.snapshot(), mainChain.snapshot()), "a restored chain must continue byte for byte");
                    var range = mainBack.archiveRange();
                    expect(range != null && range.first() == 1 && range.last() == 3, "archive range " + range);
                    expect(Arrays.equals(mainBack.archivedBlock(3), straight) && mainBack.archivedCertificate(3).length > 0, "archived block");
                    expect(mainBack.pruneArchive(3) == 2 && mainBack.archiveRange().first() == 3, "prune");

                    byte[] trailing = Arrays.copyOf(mainSnapshot, mainSnapshot.length + 1);
                    refused(() -> bc.restoreChain(mainGenesis, 0, server.publicKey(), server.secret(), trailing), null);
                    refused(() -> bc.restoreChain(userGenesis, 0, server.publicKey(), server.secret(), mainSnapshot), "another genesis");
                    expect(mainBack.childRecord(7001).tipHeight() == 2, "record tip after restore");
                    byte[] root = bc.headerSubchainRoot(mainBack.tip());
                    byte[] proof = mainBack.childProof(7001);
                    expect(bc.verifyChildProof(root, proof).chainId() == 7001, "child proof after restore");
                    proof[proof.length - 1] ^= 1;
                    refused(() -> bc.verifyChildProof(root, proof), null);
                    refused(() -> mainBack.childRecord(9), null);
                }

                byte[] newKey = Arrays.copyOfRange(stranger, 32, 64);
                byte[] rotate = f.sign(bc.opSetRegistrars(0, 0, 1, List.of(new Aviary.Registrar(Aviary.SIG_ED25519, newKey))), 7, mainHash);
                commit(mainChain, mainChain.proposeSubchain(13, null, new byte[][]{rotate}));
                expect(mainChain.registryCounts().authorityEpoch() == 1, "set registrars bumps the epoch");
                var rec = mainChain.childRecord(7001);
                byte[] staleClose = f.sign(bc.opClose(0, 0, 7001, rec.tipHeight(), rec.tipHash()), 7, mainHash);
                refused(() -> mainChain.proposeSubchain(14, null, new byte[][]{staleClose}), null);
                byte[] closeOp = bc.opClose(1, 0, 7001, rec.tipHeight(), rec.tipHash());
                byte[] closeSigned = bc.signOp(closeOp, 7, mainHash, 0, Aviary.SIG_ED25519, stranger);
                expect(closeSigned.length > closeOp.length, "approval appended");
                commit(mainChain, mainChain.proposeSubchain(14, null, new byte[][]{closeSigned}));
                var closed = mainChain.childRecord(7001);
                counts = mainChain.registryCounts();
                expect(closed.status() == Aviary.CHILD_CLOSED && counts.open() == 0 && counts.records() == 1, "closed under the new registrar");
            }
        }
    }

    private static void treeSnapshot(Aviary bc) {
        var rootKey = bc.newBLSValidator(filled(0x03));
        var childKey = bc.newBLSValidator(filled(0x13));
        java.util.function.Supplier<Aviary.TreeNode> spec = () -> {
            var child = new Aviary.TreeNode(42).validators(new byte[][]{childKey.publicKey()}, new byte[][]{childKey.secret()}, new long[]{1});
            child.blueprint = bc.new Blueprint("bft", "account", "", null);
            var root = new Aviary.TreeNode(1).validators(new byte[][]{rootKey.publicKey()}, new byte[][]{rootKey.secret()}, new long[]{1}).child(child);
            root.blueprint = bc.new Blueprint("pos", "account", "round-robin", null);
            return root;
        };
        java.util.function.BiFunction<Aviary.ChainTree, Long, byte[]> round = (tree, ts) -> {
            byte[] a = tree.produce(42, ts), b = tree.produce(1, ts);
            byte[] both = Arrays.copyOf(a, a.length + b.length);
            System.arraycopy(b, 0, both, a.length, b.length);
            return both;
        };
        try (var straight = bc.buildChainTree(spec.get())) {
            byte[] snapshot;
            try (var first = bc.buildChainTree(spec.get())) {
                expect(Arrays.equals(round.apply(straight, 10L), round.apply(first, 10L)), "two trees from one spec agree");
                snapshot = first.snapshot();
            }
            try (var second = bc.restoreChainTree(spec.get(), snapshot)) {
                expect(Arrays.equals(second.snapshot(), snapshot) && Arrays.equals(round.apply(straight, 11L), round.apply(second, 11L)), "a restored tree must continue byte for byte");
            }
            byte[] trailing = Arrays.copyOf(snapshot, snapshot.length + 1);
            refused(() -> bc.restoreChainTree(spec.get(), trailing), null);
        }
    }
}
