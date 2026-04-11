import XCTest
import SwiftTreeSitter
import TreeSitterTaint

final class TreeSitterTaintTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_taint())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Taint grammar")
    }
}
