// swift-tools-version:5.3
import PackageDescription

let package = Package(
    name: "TreeSitterMemory",
    products: [
        .library(name: "TreeSitterMemory", targets: ["TreeSitterMemory"]),
    ],
    dependencies: [
        .package(url: "https://github.com/ChimeHQ/SwiftTreeSitter", from: "0.8.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterMemory",
            dependencies: [],
            path: ".",
            sources: [
                "src/parser.c",
                // NOTE: if your language has an external scanner, add it here.
            ],
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterMemoryTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterMemory",
            ],
            path: "bindings/swift/TreeSitterMemoryTests"
        )
    ],
    cLanguageStandard: .c11
)
