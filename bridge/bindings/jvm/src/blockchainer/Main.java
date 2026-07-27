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

            System.out.println("\nALL GOOD");
        }
    }
}
