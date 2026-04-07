package tree_sitter_memory_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_memory "github.com/tree-sitter/tree-sitter-memory/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_memory.Language())
	if language == nil {
		t.Errorf("Error loading Memory grammar")
	}
}
