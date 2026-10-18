package aviary;

import java.util.HexFormat;

/** Java demo: drives the full stack. Run with --enable-native-access=ALL-UNNAMED. */
public final class Main {
    public static void main(String[] args) throws Exception {
        try (Aviary bc = new Aviary()) {
            System.out.printf("aviary %s (crypto backend %s)%n%n", bc.version(), bc.cryptoBackend());

            Aviary.Node node = bc.createNode(42);
            Aviary.Account alice = node.newAccount();
            Aviary.Account bob = node.newAccount();
            node.credit(alice.address(), 1000);
            System.out.println("chain 42 created; alice funded " + node.balance(alice.address()));

            node.submitTransfer(alice.secret(), bob.address(), 100, 0);
            node.submitTransfer(alice.secret(), bob.address(), 50, 1);
            System.out.println("submitted 2 transfers; mempool=" + node.mempoolSize());
            System.out.println("produced block -> height=" + node.produceBlock(1));
            System.out.println("  alice=" + node.balance(alice.address()) + "  bob=" + node.balance(bob.address()));

            System.out.println("\n-- streamed events --\n" + node.drainEvents());
            System.out.println("-- metrics --");
            for (String line : node.metrics().split("\n"))
                if (!line.isEmpty() && !line.startsWith("#")) System.out.println("  " + line);

            byte[] proof = node.proveAccount(alice.address());
            System.out.println("\nlight-client proof of alice verifies: " + bc.verifyStateProof(node.stateRoot(), proof));

            HexFormat hex = HexFormat.of();
            byte[] ethKey = hex.parseHex("ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80");
            byte[] btcKey = hex.parseHex("0000000000000000000000000000000000000000000000000000000000000001");
            System.out.println("\n-- interop --");
            System.out.println("  ETH: " + bc.ethAddress(ethKey));
            System.out.println("  BTC: " + bc.btcAddress(btcKey));
            System.out.println("  SOL: " + bc.solAddress(alice.secret()));

            node.close();
            // Portable account export - the three tiers, all fail-closed.
            var hy = node.newAccountHybrid();
            byte[] env1 = bc.sealAccountPassphrase(hy.publicKey(), hy.secret(), "backup", 3, 1 << 16);
            if (!java.util.Arrays.equals(bc.openAccountPassphrase(env1, "backup").secret(), hy.secret()))
                throw new IllegalStateException("passphrase round-trip");
            boolean wrongPass = false;
            try { bc.openAccountPassphrase(env1, "nope"); } catch (RuntimeException e) { wrongPass = true; }
            if (!wrongPass) throw new IllegalStateException("wrong passphrase must fail");

            var dev = bc.hybridKemKeypair();
            byte[] env2 = bc.sealAccountTo(hy.publicKey(), hy.secret(), dev.publicKey());
            if (!java.util.Arrays.equals(bc.openAccountTo(env2, dev.secret()).secret(), hy.secret()))
                throw new IllegalStateException("recipient round-trip");

            byte[] env3 = bc.sealAccountSignedTo(hy.publicKey(), hy.secret(), dev.publicKey(), hy.secret());
            if (!java.util.Arrays.equals(bc.openAccountSignedFrom(env3, dev.secret(), hy.publicKey()).secret(), hy.secret()))
                throw new IllegalStateException("signed round-trip");
            var stranger = bc.hybridKemKeypair();
            boolean wrongSigner = false;
            try { bc.openAccountSignedFrom(env3, dev.secret(), stranger.publicKey()); }
            catch (RuntimeException e) { wrongSigner = true; }
            if (!wrongSigner) throw new IllegalStateException("wrong signer must fail");
            System.out.println("portable tiers OK (passphrase + recipient + signed, all fail-closed)");

            // Threshold (FROST 2-of-3): the aggregate is a standard Ed25519 signature.
            var cards = bc.createThresholdAccount(3, 2);
            if (cards.length != 3) throw new IllegalStateException("expected 3 share cards");
            byte[] groupPK = cards[0].groupPublicKey();
            byte[] tmsg = "threshold-signed transfer".getBytes(java.nio.charset.StandardCharsets.UTF_8);
            byte[] cm0 = bc.commit(cards[0].bytes()), cm2 = bc.commit(cards[2].bytes());
            byte[] com0 = java.util.Arrays.copyOfRange(cm0, 64, 130);
            byte[] com2 = java.util.Arrays.copyOfRange(cm2, 64, 130);
            byte[] non0 = java.util.Arrays.copyOfRange(cm0, 0, 64);
            byte[] non2 = java.util.Arrays.copyOfRange(cm2, 0, 64);
            byte[] tsig;
            try (var cer = bc.newCeremony(groupPK, tmsg, new byte[][]{com2, com0})) {
                byte[] s0 = cer.sign(cards[0].bytes(), non0);
                byte[] s2 = cer.sign(cards[2].bytes(), non2);
                if (!cer.verifyShare(cards[0].index(), cards[0].publicShare(), s0))
                    throw new IllegalStateException("honest share not attributed");
                byte[] bad = s2.clone(); bad[5] ^= 1;
                if (cer.verifyShare(cards[2].index(), cards[2].publicShare(), bad))
                    throw new IllegalStateException("corrupt share not detected");
                tsig = cer.aggregate(new int[]{cards[0].index(), cards[2].index()}, new byte[][]{s0, s2});
            }
            if (tsig.length != 64) throw new IllegalStateException("aggregate not 64 bytes");
            byte[] spki = new byte[12 + 32];
            System.arraycopy(new byte[]{0x30,0x2a,0x30,0x05,0x06,0x03,0x2b,0x65,0x70,0x03,0x21,0x00}, 0, spki, 0, 12);
            System.arraycopy(groupPK, 0, spki, 12, 32);
            var pub = java.security.KeyFactory.getInstance("Ed25519")
                .generatePublic(new java.security.spec.X509EncodedKeySpec(spki));
            var ver = java.security.Signature.getInstance("Ed25519");
            ver.initVerify(pub); ver.update(tmsg);
            if (!ver.verify(tsig)) throw new IllegalStateException("threshold aggregate is not a valid Ed25519 signature");
            // Sealed share round-trip.
            byte[] tenv = bc.sealShare(cards[1].bytes(), "share-pass", 3, 1 << 16);
            if (!java.util.Arrays.equals(bc.unsealShare(tenv, "share-pass"), cards[1].bytes()))
                throw new IllegalStateException("sealed share round-trip");
            System.out.println("threshold OK (2-of-3 -> Ed25519, attribution, sealed)");

            // AssuranceProfile tiers + SealedMempool (anti-MEV).
            var hardP = bc.describeAssurance(2);
            if (!hardP.ratcheted() || !hardP.vrf() || !hardP.thresholdCustody() || hardP.sealedMempool())
                throw new IllegalStateException("hardened tiers wrong");
            var fortP = bc.describeAssurance(3);
            if (!fortP.sealedMempool()) throw new IllegalStateException("fortress should seal the mempool");
            var kem = bc.hybridKemKeypair();
            byte[] tx = bc.encodeTx(2, 7, 1, 300, new byte[]{1}, new byte[32], new byte[64]);
            try (var pool = bc.newSealedMempool(64)) {
                if (!pool.add(bc.sealTx(tx, kem.publicKey()))) throw new IllegalStateException("add rejected");
                if (pool.reveal(kem.secret(), 10).length != 1) throw new IllegalStateException("proposer reveal");
                if (bc.newSealedMempool(64) != null && pool.reveal(bc.hybridKemKeypair().secret(), 10).length != 0)
                    throw new IllegalStateException("a non-proposer opened the pool");
            }
            System.out.println("assurance + sealed-mempool OK");

            // CredentialGate (anonymous membership admission, Option A - unlinkable).
            var issuer = bc.newBBSIssuer(new byte[32]);
            if (issuer.publicKey().length != 96 || issuer.secret().length != 32)
                throw new IllegalStateException("bad BBS issuer key sizes");
            var credPolicy = bc.new CredentialPolicy(7, issuer.publicKey(),
                    "aviary.credential.v1".getBytes(java.nio.charset.StandardCharsets.UTF_8), 4, 2);
            if (credPolicy.hash().length != 32) throw new IllegalStateException("policy hash size");
            byte[] memberSecret = new byte[32];
            java.util.Arrays.fill(memberSecret, (byte) 0xAB);
            long credNow = 1_700_000_000L;
            byte[] cred = credPolicy.issue(issuer.secret(), memberSecret, 3, 4_102_444_800L, 1);
            byte[] chal = new byte[32];
            java.util.Arrays.fill(chal, (byte) 0x01);
            try (var credGate = bc.newCredentialGate(credPolicy)) {
                byte[] pres = credPolicy.present(cred, 5, "submit", chal);
                if (!credGate.admit(5, "submit", chal, pres, credNow))
                    throw new IllegalStateException("valid presentation rejected");
                byte[] pres2 = credPolicy.present(cred, 5, "submit", chal);
                if (java.util.Arrays.equals(pres, pres2))
                    throw new IllegalStateException("presentations not unlinkable");
                if (credGate.admit(6, "submit", chal, pres, credNow))
                    throw new IllegalStateException("wrong epoch admitted");
                byte[] low = credPolicy.present(credPolicy.issue(issuer.secret(), memberSecret, 1, 4_102_444_800L, 1), 5, "submit", chal);
                if (credGate.admit(5, "submit", chal, low, credNow))
                    throw new IllegalStateException("below-min-tier admitted");
                credGate.revokeIssuerEpoch(1);
                if (credGate.admit(5, "submit", chal, pres, credNow))
                    throw new IllegalStateException("revoked epoch admitted");
                if (credGate.admit(5, "submit", chal, "garbage".getBytes(), credNow))
                    throw new IllegalStateException("malformed presentation admitted");
            }
            System.out.println("credential-gate OK");

            // CredentialGate Option B (pseudonymous - per-epoch Sybil resistance).
            var pseudoPolicy = bc.new CredentialPolicy(7, issuer.publicKey(),
                    "aviary.credential.v2".getBytes(java.nio.charset.StandardCharsets.UTF_8), 4, 2, 1);
            byte[] ms = new byte[32];
            java.util.Arrays.fill(ms, (byte) 0x77);
            byte[] pmc = pseudoPolicy.membershipCommit(ms);
            byte[] pbi = pseudoPolicy.blindIssue(issuer.secret(), Aviary.extractCommitment(pmc), 3, 4_102_444_800L, 1);
            byte[] pcred = bc.finalizeMembership(pmc, pbi);
            byte[] chB = new byte[32];
            java.util.Arrays.fill(chB, (byte) 1);
            byte[] chB2 = new byte[32];
            java.util.Arrays.fill(chB2, (byte) 2);
            try (var pgate = bc.newCredentialGate(pseudoPolicy)) {
                byte[] nymA = pgate.admitPseudonymous(5, "gossip", chB, pseudoPolicy.presentPseudonymous(pcred, 5, "gossip", chB), credNow);
                if (nymA == null || nymA.length != 48) throw new IllegalStateException("pseudonym admit");
                byte[] nym2 = pgate.admitPseudonymous(5, "gossip", chB2, pseudoPolicy.presentPseudonymous(pcred, 5, "gossip", chB2), credNow);
                if (!java.util.Arrays.equals(nymA, nym2)) throw new IllegalStateException("same member+epoch+scope must link");
                if (!pgate.noteAdmission(5, "gossip", nymA) || pgate.noteAdmission(5, "gossip", nym2))
                    throw new IllegalStateException("Sybil cap");
                byte[] nymE = pgate.admitPseudonymous(6, "gossip", chB, pseudoPolicy.presentPseudonymous(pcred, 6, "gossip", chB), credNow);
                if (nymE == null || java.util.Arrays.equals(nymE, nymA)) throw new IllegalStateException("different epoch must unlink");
            }
            System.out.println("credential-gate pseudonymous OK");

            // Composable chains: declare a mixture, commit it, assemble, commit a block.
            if (!bc.engineIds().contains("pos") || !bc.stateModelIds().contains("account") || !bc.proposerIds().contains("round-robin"))
                throw new IllegalStateException("registry discovery");
            var val = bc.newBLSValidator(new byte[32]);
            byte[] addr = new byte[32]; java.util.Arrays.fill(addr, (byte) 0x11);
            byte[] gen = bc.buildGenesis(7, 1, 1, 0, new byte[][]{addr}, new long[]{1000}, new byte[][]{val.publicKey()}, new long[]{1});
            var posBp = bc.new Blueprint("pos", "account", "round-robin", null);
            byte[] mixedGen = bc.commitBlueprint(gen, posBp);
            if (java.util.Arrays.equals(bc.genesisHash(gen), bc.genesisHash(mixedGen))) throw new IllegalStateException("mixture must change genesis hash");
            try (var ch = bc.assembleChain(mixedGen, 0, val.publicKey(), val.secret())) {
                if (ch == null) throw new IllegalStateException("PoS chain should assemble");
                byte[] blk = ch.propose(2);
                if (!ch.submit(blk, ch.seal(blk)) || ch.height() != 1) throw new IllegalStateException("PoS block did not commit");
            }
            byte[] target = new byte[32]; java.util.Arrays.fill(target, (byte) 0xFF);
            try (var pc = bc.assembleChain(bc.commitBlueprint(gen, bc.new Blueprint("pow", "account", "", target)), 0, val.publicKey(), val.secret())) {
                byte[] cand = pc.propose(2);
                if (!pc.submit(pc.mine(cand), null) || pc.height() != 1) throw new IllegalStateException("PoW block did not commit");
            }
            if (bc.assembleChain(bc.commitBlueprint(gen, bc.new Blueprint("no-such-engine", "account", "", null)), 0, val.publicKey(), val.secret()) != null)
                throw new IllegalStateException("unknown engine must be rejected");
            System.out.println("chain-blueprint OK");

            // Foreign components: a Java-implemented engine + proposer join the mixture.
            final int[] calls = {0, 0};
            var javaEngine = new Aviary.ForeignEngine() {
                byte[] tag(byte[] b) { byte[] p = "java-just:".getBytes(); int n = Math.min(16, b.length); byte[] t = new byte[p.length + n];
                    System.arraycopy(p, 0, t, 0, p.length); System.arraycopy(b, 0, t, p.length, n); return t; }
                public byte[] justify(byte[] block) { calls[0]++; return tag(block); }
                public boolean verify(byte[] block, byte[] just) { calls[1]++; return java.util.Arrays.equals(just, tag(block)); }
            };
            if (!bc.registerEngine("java-engine", javaEngine, false) || !bc.registerProposer("java-first", (hh, n) -> 0))
                throw new IllegalStateException("register foreign");
            if (!bc.engineIds().contains("java-engine")) throw new IllegalStateException("java-engine not discoverable");
            try (var fc = bc.assembleChain(bc.commitBlueprint(gen, bc.new Blueprint("java-engine", "account", "", null)), 0, val.publicKey(), val.secret())) {
                byte[] fb = fc.propose(2);
                byte[] fj = fc.seal(fb);  // -> Java justify
                if (!new String(fj, 0, 10).equals("java-just:")) throw new IllegalStateException("seal via Java justify");
                byte[] bad = fj.clone(); bad[bad.length - 1] ^= 1;
                if (fc.submit(fb, bad) || fc.height() != 0) throw new IllegalStateException("tampered justification must be refused, ledger untouched");
                if (!fc.submit(fb, fj) || fc.height() != 1) throw new IllegalStateException("Java-justified block did not commit");
                if (calls[0] < 1 || calls[1] < 2) throw new IllegalStateException("callbacks did not cross the boundary");
            }
            try (var pc2 = bc.assembleChain(bc.commitBlueprint(gen, bc.new Blueprint("pos", "account", "java-first", null)), 0, val.publicKey(), val.secret())) {
                byte[] pb = pc2.propose(2);
                if (!pc2.submit(pb, pc2.seal(pb)) || pc2.height() != 1) throw new IllegalStateException("PoS with a Java proposer did not commit");
            }
            System.out.println("foreign-components OK");

            // Subchains: a BFT child anchored by a parent; a balance on the child proven from the parent header.
            {
                var cv = bc.newBLSValidator(java.util.Arrays.copyOf(new byte[]{0x71}, 32));
                var pv = bc.newBLSValidator(java.util.Arrays.copyOf(new byte[]{0x72}, 32));
                byte[] aliceS = new byte[32]; java.util.Arrays.fill(aliceS, (byte) 0xA1);
                var bft = bc.new Blueprint("bft", "account", "", null);
                byte[] childGen = bc.commitBlueprint(bc.buildGenesis(42, 1, 1, 0, new byte[][]{aliceS}, new long[]{1000}, new byte[][]{cv.publicKey()}, new long[]{1}), bft);
                byte[] childHeader = bc.genesisHeader(childGen);
                byte[] parentGen = bc.commitBlueprint(bc.buildGenesis(1, 1, 1, 0, new byte[0][], new long[0], new byte[][]{pv.publicKey()}, new long[]{1}), bft);
                byte[] reg = bc.encodeChildRegistration(42, childHeader, new byte[][]{cv.publicKey()}, new long[]{1}, 0);
                byte[] policy = bc.encodeSubchainPolicy(1, 1, 0, 4, new byte[][]{reg});
                byte[] plainHash = bc.genesisHash(parentGen);
                parentGen = bc.commitSubchainPolicy(parentGen, policy);
                if (java.util.Arrays.equals(plainHash, bc.genesisHash(parentGen))) throw new IllegalStateException("policy must change the parent genesis hash");
                try { bc.commitSubchainPolicy(parentGen, bc.encodeSubchainPolicy(1, 1, 0, 0, new byte[][]{bc.encodeChildRegistration(1, childHeader, new byte[][]{cv.publicKey()}, new long[]{1}, 0)})); throw new IllegalStateException("own-chain child must be refused"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                byte[] bound = bc.commitParentBinding(childGen, 1, bc.genesisHash(parentGen));
                if (java.util.Arrays.equals(bc.genesisHash(bound), bc.genesisHash(childGen)) || !java.util.Arrays.equals(bc.genesisHeader(bound), childHeader)) throw new IllegalStateException("binding must change the hash, not the header");
                try (var child = bc.assembleChain(bound, 0, cv.publicKey(), cv.secret()); var parent = bc.assembleChain(parentGen, 0, pv.publicKey(), pv.secret())) {
                    if (child == null || parent == null) throw new IllegalStateException("assemble");
                    byte[][] certified = new byte[2][];
                    for (int ts = 2; ts <= 3; ts++) {
                        byte[] blk = child.propose(ts); byte[] cert = child.certify(blk);
                        if (!child.submit(blk, cert)) throw new IllegalStateException("child submit");
                        certified[ts - 2] = bc.certifiedHeader(bc.blockHeader(blk), cert);
                    }
                    if (child.height() != 2 || parent.childTip(42).length == 0) throw new IllegalStateException("child state");
                    try { parent.childTip(99); throw new IllegalStateException("unknown child must fail"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                    byte[] anchor = bc.encodeAnchor(42, certified);
                    byte[] pblk = parent.proposeAnchored(2, new byte[][]{anchor});
                    if (!parent.submit(pblk, parent.certify(pblk)) || parent.height() != 1) throw new IllegalStateException("parent anchored");
                    byte[] tampered = certified[1].clone(); tampered[tampered.length - 1] ^= 1;
                    try { parent.proposeAnchored(3, new byte[][]{bc.encodeAnchor(42, new byte[][]{tampered})}); throw new IllegalStateException("tampered anchor must be refused"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                    try { parent.proposeAnchored(3, new byte[][]{anchor}); throw new IllegalStateException("stale anchor must be refused"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                    byte[] root = bc.headerSubchainRoot(bc.blockHeader(pblk));
                    byte[] scProof = bc.encodeSubchainProof(new byte[][]{parent.subchainHop(42)}, child.proveAccount(aliceS));
                    if (!bc.verifySubchainProof(root, scProof)) throw new IllegalStateException("cross-chain proof must verify");
                    byte[] wrong = new byte[32]; java.util.Arrays.fill(wrong, (byte) 1);
                    byte[] bad = scProof.clone(); bad[bad.length - 1] ^= 1;
                    if (bc.verifySubchainProof(wrong, scProof) || bc.verifySubchainProof(root, bad) || bc.verifySubchainProof(root, bc.encodeSubchainProof(new byte[0][], child.proveAccount(aliceS))))
                        throw new IllegalStateException("tampered / rootless proofs must fail");
                    byte[] blk3 = child.propose(4); byte[] cert3 = child.certify(blk3); child.submit(blk3, cert3);
                    byte[] pblk2 = parent.proposeAnchored(3, new byte[][]{bc.encodeAnchor(42, new byte[][]{bc.certifiedHeader(bc.blockHeader(blk3), cert3)})});
                    if (!parent.submit(pblk2, parent.certify(pblk2))) throw new IllegalStateException("parent block 2");
                    byte[] root2 = bc.headerSubchainRoot(bc.blockHeader(pblk2));
                    if (bc.verifySubchainProof(root2, scProof) || !bc.verifySubchainProof(root, scProof)) throw new IllegalStateException("stale proof semantics");
                    if (!bc.verifySubchainProof(root2, bc.encodeSubchainProof(new byte[][]{parent.subchainHop(42)}, child.proveAccount(aliceS)))) throw new IllegalStateException("fresh proof");
                }
            }
            System.out.println("subchains OK");

            // Validator rotation: grow, certify under each set, rotate out, PoS refuses.
            {
                var me = bc.newBLSValidator(java.util.Arrays.copyOf(new byte[]{(byte) 0x81}, 32));
                var other = bc.newBLSValidator(java.util.Arrays.copyOf(new byte[]{(byte) 0x82}, 32));
                var bft = bc.new Blueprint("bft", "account", "", null);
                byte[] rgen = bc.commitBlueprint(bc.buildGenesis(5, 1, 1, 0, new byte[0][], new long[0], new byte[][]{me.publicKey()}, new long[]{1}), bft);
                try (var chain = bc.assembleChain(rgen, 0, me.publicKey(), me.secret())) {
                    if (chain.validators().size() != 1) throw new IllegalStateException("initial set");
                    if (!chain.scheduleValidators(new byte[][]{me.publicKey(), other.publicKey()}, new long[]{3, 1})) throw new IllegalStateException("schedule");
                    byte[] b1 = chain.propose(2); if (!chain.submit(b1, chain.certify(b1))) throw new IllegalStateException("rotation block");
                    var vs = chain.validators();
                    if (vs.size() != 2 || vs.get(0).power() != 3 || !java.util.Arrays.equals(vs.get(1).publicKey(), other.publicKey())) throw new IllegalStateException("set after rotation");
                    byte[] b2 = chain.propose(3); if (!chain.submit(b2, chain.certify(b2)) || chain.height() != 2) throw new IllegalStateException("block under the new set");
                    if (chain.scheduleValidators(new byte[0][], new long[0]) || chain.scheduleValidators(new byte[][]{me.publicKey()}, new long[]{0})) throw new IllegalStateException("malformed sets must be refused");
                    if (!chain.scheduleValidators(new byte[][]{other.publicKey()}, new long[]{1})) throw new IllegalStateException("schedule leave");
                    byte[] b3 = chain.propose(4); if (!chain.submit(b3, chain.certify(b3)) || chain.height() != 3) throw new IllegalStateException("leaving block");
                    try { chain.certify(chain.propose(5)); throw new IllegalStateException("a node that left must not certify"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                }
                try (var pc = bc.assembleChain(bc.commitBlueprint(bc.buildGenesis(6, 1, 1, 0, new byte[0][], new long[0], new byte[][]{me.publicKey()}, new long[]{1}), bc.new Blueprint("pos", "account", "round-robin", null)), 0, me.publicKey(), me.secret())) {
                    if (pc.scheduleValidators(new byte[][]{me.publicKey()}, new long[]{1})) throw new IllegalStateException("PoS must refuse");
                }
            }
            System.out.println("validator-rotation OK");

            // Light-client overlap rule: power moves A -> B; a client trusting only A bisects through the rotation.
            {
                var va = bc.newBLSValidator(java.util.Arrays.copyOf(new byte[]{(byte) 0x91}, 32));
                var vb = bc.newBLSValidator(java.util.Arrays.copyOf(new byte[]{(byte) 0x92}, 32));
                var bft = bc.new Blueprint("bft", "account", "", null);
                var genesisSet = java.util.List.of(new Aviary.ValidatorEntry(va.publicKey(), 1));
                byte[] lgen = bc.commitBlueprint(bc.buildGenesis(8, 1, 1, 0, new byte[0][], new long[0], new byte[][]{va.publicKey()}, new long[]{1}), bft);
                byte[] gh = bc.genesisHeader(lgen);
                try (var A = bc.assembleChain(lgen, 0, va.publicKey(), va.secret()); var B = bc.assembleChain(lgen, 0, vb.publicKey(), vb.secret())) {
                    var cps = new java.util.HashMap<Long, Aviary.Checkpoint>();
                    if (!A.scheduleValidators(new byte[][]{va.publicKey(), vb.publicKey()}, new long[]{1, 3})) throw new IllegalStateException("schedule");
                    byte[] b1 = A.propose(2); byte[] c1 = A.certify(b1);
                    if (!A.submit(b1, c1) || !B.submit(b1, c1)) throw new IllegalStateException("rotation block");
                    cps.put(1L, new Aviary.Checkpoint(bc.blockHeader(b1), c1, genesisSet));
                    var newSet = B.validators();
                    for (long ts = 3; ts <= 4; ts++) {
                        byte[] blk = B.propose(ts); byte[] cert = B.certify(blk);
                        if (!B.submit(blk, cert) || !A.submit(blk, cert)) throw new IllegalStateException("B block");
                        cps.put(ts - 1, new Aviary.Checkpoint(bc.blockHeader(blk), cert, newSet));
                    }
                    try (var lc = bc.newLightClient(8, genesisSet, gh)) {
                        if (lc.acceptSkipping(cps.get(3L)) != Aviary.SKIP_INSUFFICIENT_OVERLAP) throw new IllegalStateException("no-overlap skip must report thin overlap");
                        if (lc.acceptSkipping(new Aviary.Checkpoint(cps.get(3L).header(), cps.get(3L).certificate(), genesisSet)) != Aviary.SKIP_REFUSED) throw new IllegalStateException("wrong set must be refused");
                        byte[] badCert = cps.get(3L).certificate().clone(); badCert[badCert.length - 1] ^= 1;
                        if (lc.acceptSkipping(new Aviary.Checkpoint(cps.get(3L).header(), badCert, newSet)) != Aviary.SKIP_REFUSED || lc.height() != 0) throw new IllegalStateException("tampered cert must be refused");
                        final int[] fetched = {0};
                        lc.syncTo(cps.get(3L), 3, hh -> { fetched[0]++; return cps.get(hh); });
                        if (lc.height() != 3 || fetched[0] == 0 || lc.validators().size() != 2 || lc.validators().get(1).power() != 3) throw new IllegalStateException("bisection sync");
                        byte[] b4 = B.propose(5); byte[] c4 = B.certify(b4); B.submit(b4, c4);
                        if (!lc.accept(bc.blockHeader(b4), c4) || lc.height() != 4) throw new IllegalStateException("sequential accept after a sync");
                    }
                    try (var lc2 = bc.newLightClient(8, genesisSet, gh)) {
                        lc2.setMaxSkip(1);
                        if (lc2.acceptSkipping(cps.get(2L)) != Aviary.SKIP_REFUSED) throw new IllegalStateException("max_skip must refuse a 2-block jump");
                        lc2.setMaxSkip(0);
                        try { lc2.syncTo(cps.get(3L), 3, hh -> null); throw new IllegalStateException("unavailable checkpoint must fail"); } catch (IllegalStateException e) { if (!e.getMessage().contains("unavailable")) throw e; }
                    }
                    try { bc.newLightClient(8, java.util.List.of(), gh); throw new IllegalStateException("empty set must be refused"); } catch (IllegalArgumentException expected) {}
                }
            }
            System.out.println("light-client-skip OK");

            // ChainTree: root <- child <- grandchild (+ sibling) built together, produced upward, proven from the root, hooks from Java.
            {
                java.util.function.Function<Integer, byte[][]> kp = seed -> { var v = bc.newBLSValidator(java.util.Arrays.copyOf(new byte[]{seed.byteValue()}, 32)); return new byte[][]{v.publicKey(), v.secret()}; };
                byte[] aliceT = new byte[32]; java.util.Arrays.fill(aliceT, (byte) 0xA1);
                var pos = bc.new Blueprint("pos", "account", "round-robin", null);
                byte[][] k1 = kp.apply(0x01), k10 = kp.apply(0x10), k11 = kp.apply(0x11), k12 = kp.apply(0x12), k20 = kp.apply(0x20), k21 = kp.apply(0x21), k30 = kp.apply(0x30);
                java.util.function.Supplier<Aviary.TreeNode> mkRoot = () -> {
                    var gc = new Aviary.TreeNode(420).validators(new byte[][]{k20[0], k21[0]}, new byte[][]{k20[1], k21[1]}, new long[]{1, 1}).accounts(new byte[][]{aliceT}, new long[]{1000});
                    var ch = new Aviary.TreeNode(42).validators(new byte[][]{k10[0], k11[0], k12[0]}, new byte[][]{k10[1], k11[1], k12[1]}, new long[]{1, 1, 1}).child(gc); ch.maxHeadersPerAnchor = 4;
                    var sib = new Aviary.TreeNode(43).validators(new byte[][]{k30[0]}, new byte[][]{k30[1]}, new long[]{1});
                    var r = new Aviary.TreeNode(1).validators(new byte[][]{k1[0]}, new byte[][]{k1[1]}, new long[]{1}).child(ch).child(sib); r.blueprint = pos; return r;
                };
                try { var d = mkRoot.get(); d.children.get(1).chainId = 42; bc.buildChainTree(d); throw new IllegalStateException("duplicate id must be refused"); } catch (IllegalArgumentException expected) {}
                try { var d = mkRoot.get(); d.children.get(0).blueprint = pos; bc.buildChainTree(d); throw new IllegalStateException("PoS child must be refused"); } catch (IllegalArgumentException expected) {}
                try { var d = mkRoot.get(); d.blueprint = bc.new Blueprint("no-such-engine", "account", "", null); bc.buildChainTree(d); throw new IllegalStateException("unknown engine must be refused"); } catch (IllegalArgumentException expected) {}
                try (var tree = bc.buildChainTree(mkRoot.get())) {
                    if (!java.util.Arrays.equals(tree.chainIds(), new long[]{1, 42, 420, 43})) throw new IllegalStateException("pre-order ids");
                    var finalized = new java.util.HashMap<Long, Integer>();
                    tree.onFinalized = (id, blk) -> finalized.merge(id, 1, Integer::sum);
                    tree.produce(420, 2); tree.produce(420, 3);
                    if (tree.pendingAnchors(42) != 1 || tree.pendingAnchors(1) != 0) throw new IllegalStateException("pending anchors");
                    tree.produce(42, 2); tree.produce(43, 2);
                    byte[] rootBlk = tree.produce(1, 2);
                    if (tree.height(1) != 1 || tree.height(42) != 1 || tree.height(420) != 2 || tree.pendingAnchors(1) != 0) throw new IllegalStateException("heights");
                    if (finalized.get(420L) != 2 || finalized.get(42L) != 1 || finalized.get(1L) != 1 || finalized.get(43L) != 1) throw new IllegalStateException("finalized hook: " + finalized);
                    try { tree.produce(7, 3); throw new IllegalStateException("unknown chain must fail"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                    byte[] tproof = tree.proveAccount(420, aliceT);
                    if (!tree.verify(tproof) || !bc.verifySubchainProof(bc.headerSubchainRoot(bc.blockHeader(rootBlk)), tproof)) throw new IllegalStateException("grandchild proven from the root");
                    byte[] bad = tproof.clone(); bad[bad.length - 1] ^= 1; if (tree.verify(bad)) throw new IllegalStateException("tampered proof must fail");
                    if (bc.genesisHeader(tree.genesis(42)).length == 0 || tree.tip(420).length == 0) throw new IllegalStateException("genesis/tip");
                    final int[] tcalls = {0}; tree.txSource = (id, ts) -> { tcalls[0]++; return new byte[0][]; };
                    tree.produce(420, 4); if (tcalls[0] != 1) throw new IllegalStateException("tx source from Java");
                }
            }
            System.out.println("chain-tree OK");

            // Persistence: snapshot, restore, keep producing.
            {
                Aviary.Node src = bc.createNode(0x7701);
                Aviary.Account a = src.newAccount(), b = src.newAccount();
                src.credit(a.address(), 500);
                src.submitTransfer(a.secret(), b.address(), 120, 0); src.produceBlock(1);
                src.submitTransfer(a.secret(), b.address(), 30, 1); src.produceBlock(2);
                try (Aviary.Node back = bc.restoreNode(src.snapshot())) {
                    if (back.height() != 2 || back.balance(b.address()) != 150 || back.nonce(a.address()) != 2) throw new IllegalStateException("restored state");
                    if (!java.util.Arrays.equals(back.stateRoot(), src.stateRoot()) || !java.util.Arrays.equals(back.blockAt(1), src.blockAt(1))) throw new IllegalStateException("restored roots");
                    try (Aviary.Node audited = bc.restoreNode(src.snapshot(), true)) {
                        if (!java.util.Arrays.equals(audited.stateRoot(), back.stateRoot())) throw new IllegalStateException("replayed restore agrees");
                    }
                    if (back.nextNonce(a.address()) != 2) throw new IllegalStateException("next nonce");
                    back.submitTransfer(a.secret(), b.address(), 1, back.nextNonce(a.address()));
                    if (back.nextNonce(a.address()) != 3) throw new IllegalStateException("next nonce advances past pending");
                    back.submitTransfer(a.secret(), b.address(), 1, back.nextNonce(a.address()));
                    if (back.produceBlock(3) != 3) throw new IllegalStateException("restored node keeps producing");
                }
                try { bc.restoreNode(new byte[]{1, 2, 3}); throw new IllegalStateException("garbage snapshot accepted"); } catch (IllegalArgumentException expected) {}
                src.close();
            }
            System.out.println("persistence OK");

            // Custody signing: the secret never leaves the engine.
            {
                Aviary.Node n = bc.createNode(0x7702);
                Aviary.Account a = n.newAccount(), b = n.newAccount();
                Aviary.KeyStore ks = bc.createKeyStore("hunter2");
                if (!java.util.Arrays.equals(ks.addAccount(a.publicKey(), a.secret()), a.address()) || !ks.has(a.address()) || ks.has(b.address())) throw new IllegalStateException("store inventory");
                if (!java.util.Arrays.equals(ks.publicKey(a.address()), a.publicKey()) || ks.addresses().length != 1) throw new IllegalStateException("public key");
                n.credit(a.address(), 1000);
                n.submitTransferFromStore(ks, a.address(), b.address(), 100, 0, 0);
                n.submitTx(ks.signTransfer(0x7702, a.address(), b.address(), 1, 1, 3));
                n.produceBlock(1);
                if (n.balance(b.address()) != 101 || n.balance(a.address()) != 1000 - 100 - 1 - 3) throw new IllegalStateException("custody transfers");
                ks.lock();
                try { n.submitTransferFromStore(ks, a.address(), b.address(), 1, 2, 0); throw new IllegalStateException("locked store signed"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                ks.unlock("hunter2");
                try { ks.signTransfer(0x7702, b.address(), a.address(), 1, 0, 0); throw new IllegalStateException("unknown account signed"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                ks.close(); n.close();
            }
            System.out.println("custody signing OK");

            // Application records: a chain that stores objects instead of balances.
            {
                Aviary.PortableAccount v = bc.newBLSValidator(new byte[]{0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70});
                byte[] genesis = bc.commitBlueprint(
                        bc.buildGenesis(31, 1, 1, 2, new byte[0][], new long[0], new byte[][]{v.publicKey()}, new long[]{1}),
                        bc.new Blueprint("bft", "record", "", new byte[0]));
                Aviary.Chain rchain = bc.assembleChain(genesis, 0, v.publicKey(), v.secret());
                if (rchain.chainId() != 31) throw new IllegalStateException("chain id");
                Aviary.Node rn = bc.createNode(1);
                Aviary.Account ra = rn.newAccount(), rb = rn.newAccount();
                byte[] rkey = new byte[32]; java.util.Arrays.fill(rkey, (byte) 0x42);
                rchain.write(ra.secret(), rkey, Aviary.RecordOp.PUT, "{\"status\":\"in service\"}".getBytes(java.nio.charset.StandardCharsets.UTF_8));
                rchain.write(ra.secret(), rkey, Aviary.RecordOp.PUT, "{\"status\":\"serviced\"}".getBytes(java.nio.charset.StandardCharsets.UTF_8));
                if (rchain.pendingCount() != 2 || rchain.record(rkey) != null) throw new IllegalStateException("queue");
                boolean refused = false;
                try { rchain.write(rb.secret(), rkey, Aviary.RecordOp.PUT, new byte[]{1}); } catch (RuntimeException e) { refused = String.valueOf(e.getMessage()).contains("not the record owner"); }
                if (!refused) throw new IllegalStateException("non-owner must be refused");
                byte[] rblk = rchain.propose(2);
                if (!rchain.submit(rblk, rchain.certify(rblk)) || rchain.pendingCount() != 0) throw new IllegalStateException("record block");
                Aviary.RecordEntry rec = rchain.record(rkey);
                if (rec.version() != 2 || !java.util.Arrays.equals(rec.owner(), ra.address())) throw new IllegalStateException("record " + rec);
                byte[] rproof = rchain.proveRecord(rkey);
                if (!bc.verifyStateProof(rchain.stateRoot(), rproof) || bc.recordFromProof(rproof).version() != 2) throw new IllegalStateException("record proof");
                rchain.write(ra.secret(), rkey, Aviary.RecordOp.TRANSFER, rb.address());
                rblk = rchain.propose(3); rchain.submit(rblk, rchain.certify(rblk));
                if (!java.util.Arrays.equals(rchain.record(rkey).owner(), rb.address())) throw new IllegalStateException("transfer");
                rchain.write(rb.secret(), rkey, Aviary.RecordOp.DELETE, new byte[0]);
                rblk = rchain.propose(4); rchain.submit(rblk, rchain.certify(rblk));
                if (rchain.record(rkey) != null) throw new IllegalStateException("delete");
                rchain.close(); rn.close();
                Aviary.Chain sc = bc.assembleChain(genesis, 0, v.publicKey(), v.secret());
                Aviary.Node sn = bc.createNode(2);
                Aviary.Account ed = sn.newAccount();
                Aviary.KeyStore store = bc.createKeyStore("pass");
                byte[] edAddr = store.addAccount(ed.publicKey(), ed.secret());
                sc.writeFromStore(store, edAddr, rkey, Aviary.RecordOp.PUT, new byte[]{7});
                if (!sc.drainEvents().contains("tx_admitted")) throw new IllegalStateException("admission event");
                byte[] sblk = sc.propose(2);
                if (!sc.submit(sblk, sc.certify(sblk)) || !java.util.Arrays.equals(sc.record(rkey).owner(), edAddr)) throw new IllegalStateException("record from store");
                if (!sc.drainEvents().contains("tx_removed")) throw new IllegalStateException("removal event");
                store.close(); sn.close(); sc.close();
                byte[] issuerKm = new byte[32]; java.util.Arrays.fill(issuerKm, (byte) 0x11);
                var admIssuer = bc.newBBSIssuer(issuerKm);
                var admPolicy = bc.new CredentialPolicy(7, admIssuer.publicKey(), "aviary.credential.v1".getBytes(java.nio.charset.StandardCharsets.UTF_8), 4, 2);
                byte[] member = new byte[32]; java.util.Arrays.fill(member, (byte) 0x77);
                byte[] admCred = admPolicy.issue(admIssuer.secret(), member, 3, 4102444800L, 1);
                try (var admGate = bc.newCredentialGate(admPolicy)) {
                    Aviary.AdmissionChallenge ch = admGate.admissionChallenge(5);
                    byte[] answer = admPolicy.admissionAnswer(admCred, ch.wire());
                    if (!admGate.admissionGranted(5, ch.nonce(), answer, 1700000000L) || admGate.admissionGranted(6, ch.nonce(), answer, 1700000000L)) throw new IllegalStateException("admission");
                }
                System.out.println("records OK");
            }

            // Push events through the native hook.
            {
                Aviary.Node n = bc.createNode(0x7703);
                var seen = new java.util.ArrayList<String>();
                n.setEventHook(seen::add);
                Aviary.Account a = n.newAccount(), b = n.newAccount();
                n.credit(a.address(), 10);
                n.submitTransfer(a.secret(), b.address(), 1, 0); n.produceBlock(1);
                if (seen.size() != 3 || !seen.get(0).contains("tx_admitted") || !seen.get(1).contains("block_finalized")) throw new IllegalStateException("hook events " + seen);
                if (!n.drainEvents().equals("[]")) throw new IllegalStateException("buffer while hooked");
                n.setEventHook(null);
                n.submitTransfer(a.secret(), b.address(), 1, 1);
                if (!n.drainEvents().contains("tx_admitted")) throw new IllegalStateException("buffering after unhook");
                n.close();
            }
            System.out.println("event hook OK");

            // A deterministic four validator line mesh with an observer, then a secure session.
            {
                Aviary.PortableAccount[] keys = new Aviary.PortableAccount[4];
                byte[][] pks = new byte[4][]; long[] powers = {1, 1, 1, 1};
                for (int i = 0; i < 4; i++) { byte[] ikm = new byte[32]; java.util.Arrays.fill(ikm, (byte) (0x60 + i)); keys[i] = bc.newBLSValidator(ikm); pks[i] = keys[i].publicKey(); }
                byte[] aliceM = new byte[32]; java.util.Arrays.fill(aliceM, (byte) 0x11);
                byte[] genesis = bc.buildGenesis(42, 1, 1, 0, new byte[][]{aliceM}, new long[]{1000}, pks, powers);
                var built = new java.util.ArrayList<Aviary.NetNode>();
                for (int i = 0; i < 4; i++) built.add(bc.newNetNode(genesis, i, keys[i], 1));
                built.add(bc.newNetNode(genesis, Aviary.NET_OBSERVER, null, 1));
                Aviary.PeerMesh mesh = new Aviary.PeerMesh(built);
                for (int i = 0; i < 4; i++) mesh.link(i, i + 1);
                var nodes = mesh.nodes;
                java.util.function.Supplier<Boolean> committed = () -> nodes.subList(0, 4).stream().allMatch(Aviary.NetNode::committed);
                mesh.start();
                if (!mesh.settle(0, 1, 2, 3)) throw new IllegalStateException("mesh settle");
                if (!committed.get() || nodes.stream().anyMatch(nn -> nn.height() != 1)) throw new IllegalStateException("mesh height 1");
                if (!java.util.Arrays.equals(nodes.get(0).tip(), nodes.get(4).tip()) || nodes.get(4).balance(aliceM) != 1000) throw new IllegalStateException("observer follows");
                if (nodes.get(0).replayLog().length == 0) throw new IllegalStateException("replay log");
                if (!bc.verifyStateProof(nodes.get(2).stateRoot(), nodes.get(2).proveAccount(aliceM))) throw new IllegalStateException("net proof");
                mesh.advance();
                if (!mesh.settle(0, 1, 2, 3)) throw new IllegalStateException("mesh settle 2");
                if (nodes.stream().anyMatch(nn -> nn.height() != 2)) throw new IllegalStateException("mesh height 2");
                try { nodes.get(0).setTransactions(new byte[][]{{1, 2}}); throw new IllegalStateException("malformed tx accepted"); } catch (IllegalArgumentException expected) {}
                if (!nodes.get(2).drainEvents().contains("block_finalized")) throw new IllegalStateException("net events");
                String nm = nodes.get(2).metrics();
                if (!nm.contains("aviary_gossip_received_total") || !nm.contains("aviary_chain_height")) throw new IllegalStateException("net metrics " + nm);
                var pushed = new java.util.ArrayList<String>();
                nodes.get(0).setEventHook(pushed::add);
                nodes.get(0).setEventHook(null);
                mesh.close();

                Aviary.SecureIdentity ia = bc.generateSecureIdentity(), ib = bc.generateSecureIdentity();
                byte[] ra = bc.signPeerRecord(1, "10.0.0.1", 7001, ia, new byte[0]);
                byte[] rb = bc.signPeerRecord(2, "10.0.0.2", 7002, ib, new byte[0]);
                Aviary.PeerRecordInfo info = bc.readPeerRecord(rb);
                if (info.id() != 2 || !info.host().equals("10.0.0.2") || info.port() != 7002 || !bc.verifyPeerRecord(ra)) throw new IllegalStateException("peer record");
                byte[] forged = rb.clone(); forged[forged.length - 1] ^= 1;
                if (bc.verifyPeerRecord(forged)) throw new IllegalStateException("forged record verified");
                try (Aviary.AddressBook mine = bc.newAddressBook(); Aviary.AddressBook theirs = bc.newAddressBook(); Aviary.AddressBook evil = bc.newAddressBook()) {
                    mine.add(ra); theirs.add(rb); theirs.add(ra); evil.add(forged);
                    if (mine.merge(theirs.encode()) != 1 || mine.size() != 2 || !java.util.Arrays.equals(mine.record(2), rb) || mine.record(9) != null) throw new IllegalStateException("address book merge");
                    try (Aviary.AddressBook fresh = bc.newAddressBook()) { if (fresh.merge(evil.encode()) != 0) throw new IllegalStateException("forged record merged"); }
                }
                Aviary.NoiseIdentity na = bc.generateNoiseIdentity(), nb = bc.generateNoiseIdentity();
                byte[] prologue = "aviary".getBytes(java.nio.charset.StandardCharsets.UTF_8);
                try (Aviary.NoiseSession ni = bc.initiateNoiseSession(na, prologue); Aviary.NoiseSession nr = bc.acceptNoiseSession(nb, ni.handshakeMessage(), prologue)) {
                    nr.completeResponder(ni.completeInitiator(nr.handshakeMessage()));
                    nr.verifyIdentityProof(ni.identityProof()); ni.verifyIdentityProof(nr.identityProof());
                    if (!ni.open() || !java.util.Arrays.equals(ni.peerIdentity(), nb.sig().publicKey())) throw new IllegalStateException("noise pinning");
                    if (!new String(nr.decrypt(ni.encrypt("hello".getBytes())), java.nio.charset.StandardCharsets.UTF_8).equals("hello")) throw new IllegalStateException("noise round trip");
                }
                try (Aviary.SecureSession init = bc.initiateSecureSession(ib.kem().publicKey(), ia.sig());
                     Aviary.SecureSession resp = bc.acceptSecureSession(ib, init.handshakeMessage())) {
                    init.complete(resp.handshakeMessage());
                    if (!init.open() || !resp.open() || !java.util.Arrays.equals(init.peerIdentity(), ib.sig().publicKey()) || !java.util.Arrays.equals(resp.peerIdentity(), ia.sig().publicKey())) throw new IllegalStateException("session pinning");
                    if (!new String(resp.decrypt(init.encrypt("hello".getBytes())), java.nio.charset.StandardCharsets.UTF_8).equals("hello")) throw new IllegalStateException("round trip");
                    byte[] frame = init.encrypt(new byte[]{1}); frame[frame.length - 1] ^= 1;
                    try { resp.decrypt(frame); throw new IllegalStateException("tampered frame accepted"); } catch (RuntimeException expected) { if (expected instanceof IllegalStateException) throw expected; }
                    try { bc.acceptSecureSession(ia, init.handshakeMessage()); throw new IllegalStateException("wrong identity accepted"); } catch (IllegalArgumentException expected) {}
                }
            }
            System.out.println("network + secure session OK");

            System.out.println("\nALL GOOD");
        }
    }
}
