// swift-tools-version: 5.9
// Blockchainer Flutter plugin — Swift Package Manager manifest (macOS).
//
// FFI plugin: the BlockchainerC binary target is a DYNAMIC framework wrapping
// the prebuilt Blockchainer native library. Declaring it as a dependency makes
// SPM link and embed it into the host app; Dart resolves the C ABI at runtime.
//
// Blockchainer's cryptolib_* symbols resolve at runtime against the CryptoLibC
// framework embedded by the cryptolib_flutter plugin — this package depends on
// cryptolib_flutter and does NOT bundle a cryptolib copy.
import PackageDescription

let package = Package(
    name: "blockchainer_flutter",
    platforms: [
        .macOS("10.15"),
    ],
    products: [
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
