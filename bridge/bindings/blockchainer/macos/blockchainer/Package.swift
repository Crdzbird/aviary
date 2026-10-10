// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "blockchainer",
    platforms: [
        .macOS("10.15"),
    ],
    products: [
        .library(name: "blockchainer", targets: ["blockchainer"]),
    ],
    dependencies: [],
    targets: [
        .binaryTarget(
            name: "BlockchainerC",
            path: "BlockchainerC.xcframework"
        ),
        .target(
            name: "blockchainer",
            dependencies: ["BlockchainerC"]
        ),
    ]
)
