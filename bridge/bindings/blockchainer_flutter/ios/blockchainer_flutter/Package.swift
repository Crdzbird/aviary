// swift-tools-version: 5.9
// Blockchainer Flutter plugin — Swift Package Manager manifest (iOS).
//
// FFI plugin: the BlockchainerC binary target is a DYNAMIC framework wrapping
// the prebuilt Blockchainer native library. Declaring it as a dependency makes
// Swift Package Manager link and embed it into the host app; Dart resolves the
// C ABI at runtime via dart:ffi (DynamicLibrary.process()).
//
// Blockchainer's cryptolib_* symbols are left undefined and resolve at runtime
// against the CryptoLibC framework that the cryptolib_flutter plugin embeds, so
// this package does NOT bundle a cryptolib copy — it depends on cryptolib_flutter.
import PackageDescription

let package = Package(
    name: "blockchainer_flutter",
    platforms: [
        .iOS("13.0"),
    ],
    products: [
        // Flutter's generated package references the product by its hyphenated
        // plugin name.
        .library(name: "blockchainer-flutter", targets: ["blockchainer_flutter"]),
    ],
    dependencies: [],
    targets: [
        .binaryTarget(
            name: "BlockchainerC",
            path: "BlockchainerC.xcframework"
        ),
        .target(
            name: "blockchainer_flutter",
            dependencies: ["BlockchainerC"]
        ),
    ]
)
