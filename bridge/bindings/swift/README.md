# Aviary for Swift

The Swift binding wraps the Aviary C ABI: nodes, accounts and transfers,
composable chains, subchains, light clients, chain trees, custody (key stores,
portable and threshold accounts, key store backed signing), node snapshots,
the push event hook, networking (`NetNode` over an application owned
transport, `SecureSession` links) and the EVM, Bitcoin and Solana interop
helpers.

## Install

The package manifest is `Package.swift` at the repository root, so a
dependency is one line:

```swift
.package(url: "https://github.com/Crdzbird/aviary", from: "0.1.0"),
```

with the product `Aviary`. The two binary targets, `AviaryC.xcframework` and
the merged `CipherBird.xcframework`, download from the GitHub release named in
the manifest and are verified by checksum. Platforms: iOS 15 and macOS 12.

## Develop

Inside this repository the manifest uses the xcframework trees in the tree
when `AVIARY_LOCAL_XCFRAMEWORKS` is set:

```bash
bash scripts/apple/vendor_cipherbird.sh
AVIARY_LOCAL_XCFRAMEWORKS=1 swift run AviaryDemo
```

`scripts/apple/package_swift_release.sh swift-vX.Y.Z` zips both xcframeworks
into `dist/swift/`, computes the checksums and writes them into the manifest;
attach the zips to the GitHub release of that name.
