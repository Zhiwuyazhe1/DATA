import XCTest
import SwiftTreeSitter
import TreeSitterMemory

final class TreeSitterMemoryTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_memory())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Memory grammar")
    }
}
