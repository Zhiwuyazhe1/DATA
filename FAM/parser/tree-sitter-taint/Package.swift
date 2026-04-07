// swift-tools-version:5.3

import Foundation
import PackageDescription

var sources = ["src/parser.c"]
if FileManager.default.fileExists(atPath: "src/scanner.c") {
    sources.append("src/scanner.c")
}

let package = Package(
    name: "TreeSitterTaint",
    products: [
        .library(name: "TreeSitterTaint", targets: ["TreeSitterTaint"]),
    ],
    dependencies: [
        .package(url: "https://github.com/tree-sitter/swift-tree-sitter", from: "0.8.0"),
    ],
    targets: [
        .target(
            name: "TreeSitterTaint",
            dependencies: [],
            path: ".",
            sources: sources,
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        ),
        .testTarget(
            name: "TreeSitterTaintTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterTaint",
            ],
            path: "bindings/swift/TreeSitterTaintTests"
        )
    ],
    cLanguageStandard: .c11
)
