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
//     dependencies: [ .package(url: "https://github.com/Crdzbird/aviary", from: "0.1.0") ]
//     .product(name: "Aviary", package: "Aviary")
// then `import Aviary`.
import PackageDescription
import Foundation

// Remote consumers get the xcframeworks from a GitHub release by URL and
// checksum (written by scripts/apple/package_swift_release.sh). A local
// checkout sets AVIARY_LOCAL_XCFRAMEWORKS=1 to use the trees in this repo,
// which is what the demo and CI do. The manifest lives at the repository root
// because SwiftPM resolves git URL dependencies from there.
let releaseBase = "https://github.com/Crdzbird/aviary/releases/download/swift-v0.1.0"
let aviaryChecksum = "08080dcd5f4721b751db71413733870942fcb8a542006efbd6249d229f7f55b8"
let cipherBirdChecksum = "21f626c687696785419c3c4aea21ad008b08ac196f1ef9c513cf2c9513536a13"
let useLocal = ProcessInfo.processInfo.environment["AVIARY_LOCAL_XCFRAMEWORKS"] != nil

let aviaryBinary: Target = useLocal
    ? .binaryTarget(name: "AviaryC", path: "bridge/bindings/aviary/ios/aviary/AviaryC.xcframework")
    : .binaryTarget(name: "AviaryC", url: "\(releaseBase)/AviaryC.xcframework.zip", checksum: aviaryChecksum)
let cipherBirdBinary: Target = useLocal
    ? .binaryTarget(name: "CipherBird", path: "bridge/bindings/swift/Vendor/CipherBird.xcframework")
    : .binaryTarget(name: "CipherBird", url: "\(releaseBase)/CipherBird.xcframework.zip", checksum: cipherBirdChecksum)

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
        aviaryBinary,
        // CipherBird's dynamic framework, vendored as a binary target so
        // Aviary's runtime cryptolib_* lookups resolve. The cipherbird
        // Flutter plugin ships iOS and macOS as two xcframeworks; run
        // scripts/apple/vendor_cipherbird.sh once to merge them into
        // Vendor/CipherBird.xcframework (gitignored).
        cipherBirdBinary,
        // Ergonomic Swift wrapper. Links CipherBird so Aviary's runtime
        // cryptolib_* lookups resolve.
        .target(
            name: "Aviary",
            dependencies: [
                "AviaryC",
                "CipherBird",
            ],
            path: "bridge/bindings/swift/Sources/Aviary",
            resources: [
                .process("PrivacyInfo.xcprivacy"),
            ]
        ),
        // A tiny demo proving the bridge links + the preload path (macOS).
        .executableTarget(
            name: "AviaryDemo",
            dependencies: ["Aviary"],
            path: "bridge/bindings/swift/Sources/AviaryDemo"
        ),
    ]
)
