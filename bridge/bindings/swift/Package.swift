// swift-tools-version: 5.10
//
// Aviary Swift Package
// 
// A standalone Swift package wrapping Aviary's C ABI with a Swift API and
// an off-main-thread preload (Aviary.preload()).
//
//   - `AviaryC`  - the raw C FFI module (a DYNAMIC framework xcframework,
//     produced by scripts/apple/build_dynamic_xcframework.sh). Its cryptolib_*
//     symbols resolve at runtime against CipherBird.
//   - `Aviary`   - the ergonomic Swift wrapper (Node, interop, preload).
//
// The crypto backend comes from the cryptolib Swift package (depended on, not
// bundled). Local path until cryptolib is published.
//
// Slices: iOS 15+ device (arm64), iOS simulator (arm64), macOS 14+ (arm64).
//
// Usage from another package:
//     dependencies: [ .package(path: "path/to/aviary/bridge/bindings/swift") ]
//     .product(name: "Aviary", package: "Aviary")
// then `import Aviary`.
import PackageDescription

let package = Package(
    name: "Aviary",
    platforms: [
        .iOS(.v15),
        .macOS(.v14),
    ],
    products: [
        .library(name: "Aviary", targets: ["Aviary"]),
        .library(name: "AviaryC", targets: ["AviaryC"]),
    ],
    dependencies: [],
    targets: [
        // The Aviary C ABI as a dynamic-framework binary target.
        .binaryTarget(
            name: "AviaryC",
            path: "../aviary/ios/aviary/AviaryC.xcframework"
        ),
        // CipherBird's dynamic framework, vendored as a binary target so
        // Aviary's runtime cryptolib_* lookups resolve. The cipherbird
        // Flutter plugin ships iOS and macOS as two xcframeworks; run
        // scripts/apple/vendor_cipherbird.sh once to merge them into
        // Vendor/CipherBird.xcframework (gitignored).
        .binaryTarget(
            name: "CipherBird",
            path: "Vendor/CipherBird.xcframework"
        ),
        // Ergonomic Swift wrapper. Links CipherBird so Aviary's runtime
        // cryptolib_* lookups resolve.
        .target(
            name: "Aviary",
            dependencies: [
                "AviaryC",
                "CipherBird",
            ],
            path: "Sources/Aviary"
        ),
        // A tiny demo proving the bridge links + the preload path (macOS).
        .executableTarget(
            name: "AviaryDemo",
            dependencies: ["Aviary"],
            path: "Sources/AviaryDemo"
        ),
    ]
)
