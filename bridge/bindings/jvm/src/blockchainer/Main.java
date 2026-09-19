package blockchainer;

import java.util.HexFormat;

/** Java demo: drives the full stack. Run with --enable-native-access=ALL-UNNAMED. */
public final class Main {
    public static void main(String[] args) throws Exception {
        try (Blockchainer bc = new Blockchainer()) {
            System.out.printf("blockchainer %s (crypto backend %s)%n%n", bc.version(), bc.cryptoBackend());

            Blockchainer.Node node = bc.createNode(42);
            Blockchainer.Account alice = node.newAccount();
            Blockchainer.Account bob = node.newAccount();
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
            // Portable account export — the three tiers, all fail-closed.
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
            System.out.println("threshold OK (2-of-3 → Ed25519, attribution, sealed)");

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

            // CredentialGate (anonymous membership admission, Option A — unlinkable).
            var issuer = bc.newBBSIssuer(new byte[32]);
            if (issuer.publicKey().length != 96 || issuer.secret().length != 32)
                throw new IllegalStateException("bad BBS issuer key sizes");
            var credPolicy = bc.new CredentialPolicy(7, issuer.publicKey(),
                    "blockchainer.credential.v1".getBytes(java.nio.charset.StandardCharsets.UTF_8), 4, 2);
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

            // CredentialGate Option B (pseudonymous — per-epoch Sybil resistance).
            var pseudoPolicy = bc.new CredentialPolicy(7, issuer.publicKey(),
                    "blockchainer.credential.v2".getBytes(java.nio.charset.StandardCharsets.UTF_8), 4, 2, 1);
            byte[] ms = new byte[32];
            java.util.Arrays.fill(ms, (byte) 0x77);
            byte[] pmc = pseudoPolicy.membershipCommit(ms);
            byte[] pbi = pseudoPolicy.blindIssue(issuer.secret(), Blockchainer.extractCommitment(pmc), 3, 4_102_444_800L, 1);
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
            var javaEngine = new Blockchainer.ForeignEngine() {
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
                byte[] fj = fc.seal(fb);  // → Java justify
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

            System.out.println("\nALL GOOD");
        }
    }
}
