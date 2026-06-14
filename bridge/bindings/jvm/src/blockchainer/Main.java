package blockchainer;

import java.util.HexFormat;

/** Java demo: drives the full stack. Run with --enable-native-access=ALL-UNNAMED. */
public final class Main {
    public static void main(String[] args) {
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
            System.out.println("\nALL GOOD");
        }
    }
}
