// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "aviary",
    platforms: [
        .iOS("13.0"),
    ],
    products: [
        .library(name: "aviary", targets: ["aviary"]),
    ],
    dependencies: [],
    targets: [
        .binaryTarget(
            name: "AviaryC",
            path: "AviaryC.xcframework"
        ),
        .target(
            name: "aviary",
            dependencies: ["AviaryC"]
        ),
    ]
)
