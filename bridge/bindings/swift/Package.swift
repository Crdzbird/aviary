// swift-tools-version: 5.10
//
// Blockchainer Swift Package
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━
// A standalone Swift package wrapping Blockchainer's C ABI with a Swift API and
// an off-main-thread preload (Blockchainer.preload()).
//
//   • `BlockchainerC`  — the raw C FFI module (a DYNAMIC framework xcframework,
//     produced by scripts/apple/build_dynamic_xcframework.sh). Its cryptolib_*
//     symbols resolve at runtime against CryptoLibC.
//   • `Blockchainer`   — the ergonomic Swift wrapper (Node, interop, preload).
//
// The crypto backend comes from the cryptolib Swift package (depended on, not
// bundled). Local path until cryptolib is published.
//
// Slices: iOS 15+ device (arm64), iOS simulator (arm64), macOS 14+ (arm64).
//
// Usage from another package:
//     dependencies: [ .package(path: "path/to/blockchainer/bridge/bindings/swift") ]
//     .product(name: "Blockchainer", package: "Blockchainer")
// then `import Blockchainer`.
import PackageDescription

let package = Package(
    name: "Blockchainer",
    platforms: [
        .iOS(.v15),
        .macOS(.v14),
    ],
    products: [
        .library(name: "Blockchainer", targets: ["Blockchainer"]),
        .library(name: "BlockchainerC", targets: ["BlockchainerC"]),
    ],
    dependencies: [],
    targets: [
        // The Blockchainer C ABI as a dynamic-framework binary target.
        .binaryTarget(
            name: "BlockchainerC",
            path: "../blockchainer/ios/blockchainer/BlockchainerC.xcframework"
        ),
        // cryptolib's dynamic framework — vendored as a binary target so
        // Blockchainer's runtime cryptolib_* lookups resolve. (Referenced
        // directly rather than via the cryptolib SwiftPM package to avoid the
        // same-leaf-name path-package identity clash; this is the same dynamic
        // CryptoLibC the cryptolib_flutter plugin ships.)
        .binaryTarget(
            name: "CryptoLibC",
            path: "../../../../cryptolib/bridge/bindings/cryptolib_flutter/ios/cryptolib_flutter/CryptoLibC.xcframework"
        ),
        // Ergonomic Swift wrapper. Links CryptoLibC so Blockchainer's runtime
        // cryptolib_* lookups resolve.
        .target(
            name: "Blockchainer",
            dependencies: [
                "BlockchainerC",
                "CryptoLibC",
            ],
            path: "Sources/Blockchainer"
        ),
        // A tiny demo proving the bridge links + the preload path (macOS).
        .executableTarget(
            name: "BlockchainerDemo",
            dependencies: ["Blockchainer"],
            path: "Sources/BlockchainerDemo"
        ),
    ]
)
