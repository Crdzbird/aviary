// Kotlin demo — the same Java FFM binding, called idiomatically from Kotlin.
// Build:  kotlinc Demo.kt -cp out -include-runtime -d demo.jar
// Run:    java --enable-native-access=ALL-UNNAMED -cp out:demo.jar DemoKt
import aviary.Aviary

fun main() {
    Aviary().use { bc ->
        println("aviary ${bc.version()} (crypto backend ${bc.cryptoBackend()})\n")

        val node = bc.createNode(7)
        val alice = node.newAccount()
        val bob = node.newAccount()
        node.credit(alice.address, 1000)

        node.submitTransfer(alice.secret, bob.address, 250, 0)
        val height = node.produceBlock(1)
        println("height=$height  alice=${node.balance(alice.address)}  bob=${node.balance(bob.address)}")

        println("\nevents: ${node.drainEvents()}")
        val proof = node.proveAccount(alice.address)
        println("proof verifies: ${bc.verifyStateProof(node.stateRoot(), proof)}")

        // Interop addresses from Kotlin.
        val ethKey = "ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
            .chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        println("ETH: ${bc.ethAddress(ethKey)}")
        println("SOL: ${bc.solAddress(alice.secret)}")

        node.close()
        println("\nALL GOOD (Kotlin)")
    }
}
