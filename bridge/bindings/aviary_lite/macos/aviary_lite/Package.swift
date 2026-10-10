// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "aviary_lite",
    platforms: [
        .macOS("12.0"),
    ],
    products: [
        .library(name: "aviary-lite", targets: ["aviary_lite"]),
    ],
    dependencies: [],
    targets: [
        .binaryTarget(
            name: "Aviary",
            path: "Aviary.xcframework"
        ),
        .target(
            name: "aviary_lite",
            dependencies: ["Aviary"]
        ),
    ]
)
