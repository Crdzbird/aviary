# Aviary JVM binding (Java / Kotlin)

A Java Foreign Function & Memory (FFM / Panama) binding over the Aviary C
ABI. Drives the full node — create a chain, accounts, transfers, produce blocks
(BFT), stream events, read metrics, verify light-client proofs — plus
EVM/Bitcoin/Solana interop. Callable from **Java and Kotlin**.

Requires JDK 22+ (FFM is stable since 22).

## Build & run (dev, against the host dylib)

```bash
# from this directory; build the native lib first: (cd ../../.. && cmake --build build)
javac src/aviary/*.java -d out
LIB="$PWD/../../../build/libaviary_c.dylib"

# Java
java --enable-native-access=ALL-UNNAMED -cp out -Daviary.path="$LIB" aviary.Main

# Kotlin
kotlinc Demo.kt -cp out -include-runtime -d demo.jar
java --enable-native-access=ALL-UNNAMED -cp out:demo.jar -Daviary.path="$LIB" DemoKt
```

The native library is resolved by `NativeLoader`: `-Daviary.path` / env
`AVIARY_DYLIB` (dev), else a bundled `/native/<os>-<arch>/` resource, else
the repo `build/` dir. The dev dylib's embedded rpath resolves `libcryptolib_c`
automatically; a packaged JAR should ship both natives side by side.

## Surface

`Aviary` (AutoCloseable) → `version`, `cryptoBackend`, `createNode`,
`ethAddress`/`btcAddress`/`solAddress`, `verifyStateProof`. `Aviary.Node`
→ `newAccount`, `credit`, `submitTransfer`, `produceBlock`, `height`/`balance`/
`nonce`/`mempoolSize`, `stateRoot`, `drainEvents`, `metrics`, `proveAccount`.
