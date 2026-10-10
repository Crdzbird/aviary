// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "blockchainer_lite",
    platforms: [
        .iOS("13.0"),
    ],
    products: [
        .library(name: "blockchainer-lite", targets: ["blockchainer_lite"]),
    ],
    dependencies: [],
    targets: [
        .binaryTarget(
            name: "Blockchainer",
            path: "Blockchainer.xcframework"
        ),
        .target(
            name: "blockchainer_lite",
            dependencies: ["Blockchainer"]
        ),
    ]
)
