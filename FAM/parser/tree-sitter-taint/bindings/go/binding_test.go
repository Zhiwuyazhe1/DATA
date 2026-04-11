package tree_sitter_taint_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_taint "github.com/tree-sitter/tree-sitter-taint/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_taint.Language())
	if language == nil {
		t.Errorf("Error loading Taint grammar")
	}
}
