#include "tree_sitter/parser.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define LANGUAGE_VERSION 14
#define STATE_COUNT 39
#define LARGE_STATE_COUNT 2
#define SYMBOL_COUNT 36
#define ALIAS_COUNT 0
#define TOKEN_COUNT 24
#define EXTERNAL_TOKEN_COUNT 0
#define FIELD_COUNT 12
#define MAX_ALIAS_SEQUENCE_LENGTH 6
#define PRODUCTION_ID_COUNT 8

enum ts_symbol_identifiers {
  anon_sym_LBRACE = 1,
  anon_sym_RBRACE = 2,
  anon_sym_LT = 3,
  anon_sym_DASH1 = 4,
  anon_sym_0 = 5,
  anon_sym_1 = 6,
  anon_sym_2 = 7,
  anon_sym_3 = 8,
  anon_sym_4 = 9,
  anon_sym_5 = 10,
  anon_sym_6 = 11,
  anon_sym_7 = 12,
  anon_sym_8 = 13,
  anon_sym_9 = 14,
  anon_sym_GT = 15,
  anon_sym_PIPE_PIPE = 16,
  anon_sym_setSink = 17,
  anon_sym_LPAREN = 18,
  anon_sym_RPAREN = 19,
  anon_sym_transitive = 20,
  anon_sym_COMMA = 21,
  anon_sym_sanitize = 22,
  anon_sym_swapTaint = 23,
  sym_source_file = 24,
  sym_taint_summary = 25,
  sym_side_effects = 26,
  sym_taint_side_effect = 27,
  sym_key = 28,
  sym_keys = 29,
  sym_set_sink = 30,
  sym_transitive = 31,
  sym_sanitize = 32,
  sym_swap_taint = 33,
  aux_sym_side_effects_repeat1 = 34,
  aux_sym_keys_repeat1 = 35,
};

static const char * const ts_symbol_names[] = {
  [ts_builtin_sym_end] = "end",
  [anon_sym_LBRACE] = "{",
  [anon_sym_RBRACE] = "}",
  [anon_sym_LT] = "<",
  [anon_sym_DASH1] = "-1",
  [anon_sym_0] = "0",
  [anon_sym_1] = "1",
  [anon_sym_2] = "2",
  [anon_sym_3] = "3",
  [anon_sym_4] = "4",
  [anon_sym_5] = "5",
  [anon_sym_6] = "6",
  [anon_sym_7] = "7",
  [anon_sym_8] = "8",
  [anon_sym_9] = "9",
  [anon_sym_GT] = ">",
  [anon_sym_PIPE_PIPE] = "||",
  [anon_sym_setSink] = "setSink",
  [anon_sym_LPAREN] = "(",
  [anon_sym_RPAREN] = ")",
  [anon_sym_transitive] = "transitive",
  [anon_sym_COMMA] = ",",
  [anon_sym_sanitize] = "sanitize",
  [anon_sym_swapTaint] = "swapTaint",
  [sym_source_file] = "source_file",
  [sym_taint_summary] = "taint_summary",
  [sym_side_effects] = "side_effects",
  [sym_taint_side_effect] = "taint_side_effect",
  [sym_key] = "key",
  [sym_keys] = "keys",
  [sym_set_sink] = "set_sink",
  [sym_transitive] = "transitive",
  [sym_sanitize] = "sanitize",
  [sym_swap_taint] = "swap_taint",
  [aux_sym_side_effects_repeat1] = "side_effects_repeat1",
  [aux_sym_keys_repeat1] = "keys_repeat1",
};

static const TSSymbol ts_symbol_map[] = {
  [ts_builtin_sym_end] = ts_builtin_sym_end,
  [anon_sym_LBRACE] = anon_sym_LBRACE,
  [anon_sym_RBRACE] = anon_sym_RBRACE,
  [anon_sym_LT] = anon_sym_LT,
  [anon_sym_DASH1] = anon_sym_DASH1,
  [anon_sym_0] = anon_sym_0,
  [anon_sym_1] = anon_sym_1,
  [anon_sym_2] = anon_sym_2,
  [anon_sym_3] = anon_sym_3,
  [anon_sym_4] = anon_sym_4,
  [anon_sym_5] = anon_sym_5,
  [anon_sym_6] = anon_sym_6,
  [anon_sym_7] = anon_sym_7,
  [anon_sym_8] = anon_sym_8,
  [anon_sym_9] = anon_sym_9,
  [anon_sym_GT] = anon_sym_GT,
  [anon_sym_PIPE_PIPE] = anon_sym_PIPE_PIPE,
  [anon_sym_setSink] = anon_sym_setSink,
  [anon_sym_LPAREN] = anon_sym_LPAREN,
  [anon_sym_RPAREN] = anon_sym_RPAREN,
  [anon_sym_transitive] = anon_sym_transitive,
  [anon_sym_COMMA] = anon_sym_COMMA,
  [anon_sym_sanitize] = anon_sym_sanitize,
  [anon_sym_swapTaint] = anon_sym_swapTaint,
  [sym_source_file] = sym_source_file,
  [sym_taint_summary] = sym_taint_summary,
  [sym_side_effects] = sym_side_effects,
  [sym_taint_side_effect] = sym_taint_side_effect,
  [sym_key] = sym_key,
  [sym_keys] = sym_keys,
  [sym_set_sink] = sym_set_sink,
  [sym_transitive] = sym_transitive,
  [sym_sanitize] = sym_sanitize,
  [sym_swap_taint] = sym_swap_taint,
  [aux_sym_side_effects_repeat1] = aux_sym_side_effects_repeat1,
  [aux_sym_keys_repeat1] = aux_sym_keys_repeat1,
};

static const TSSymbolMetadata ts_symbol_metadata[] = {
  [ts_builtin_sym_end] = {
    .visible = false,
    .named = true,
  },
  [anon_sym_LBRACE] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_RBRACE] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_LT] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_DASH1] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_0] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_1] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_2] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_3] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_4] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_5] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_6] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_7] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_8] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_9] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_GT] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_PIPE_PIPE] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_setSink] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_LPAREN] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_RPAREN] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_transitive] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_COMMA] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_sanitize] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_swapTaint] = {
    .visible = true,
    .named = false,
  },
  [sym_source_file] = {
    .visible = true,
    .named = true,
  },
  [sym_taint_summary] = {
    .visible = true,
    .named = true,
  },
  [sym_side_effects] = {
    .visible = true,
    .named = true,
  },
  [sym_taint_side_effect] = {
    .visible = true,
    .named = true,
  },
  [sym_key] = {
    .visible = true,
    .named = true,
  },
  [sym_keys] = {
    .visible = true,
    .named = true,
  },
  [sym_set_sink] = {
    .visible = true,
    .named = true,
  },
  [sym_transitive] = {
    .visible = true,
    .named = true,
  },
  [sym_sanitize] = {
    .visible = true,
    .named = true,
  },
  [sym_swap_taint] = {
    .visible = true,
    .named = true,
  },
  [aux_sym_side_effects_repeat1] = {
    .visible = false,
    .named = false,
  },
  [aux_sym_keys_repeat1] = {
    .visible = false,
    .named = false,
  },
};

enum ts_field_identifiers {
  field_close_angle = 1,
  field_close_paren = 2,
  field_comma = 3,
  field_key = 4,
  field_left = 5,
  field_number = 6,
  field_open_angle = 7,
  field_open_paren = 8,
  field_operation = 9,
  field_operation_name = 10,
  field_or = 11,
  field_right = 12,
};

static const char * const ts_field_names[] = {
  [0] = NULL,
  [field_close_angle] = "close_angle",
  [field_close_paren] = "close_paren",
  [field_comma] = "comma",
  [field_key] = "key",
  [field_left] = "left",
  [field_number] = "number",
  [field_open_angle] = "open_angle",
  [field_open_paren] = "open_paren",
  [field_operation] = "operation",
  [field_operation_name] = "operation_name",
  [field_or] = "or",
  [field_right] = "right",
};

static const TSFieldMapSlice ts_field_map_slices[PRODUCTION_ID_COUNT] = {
  [1] = {.index = 0, .length = 1},
  [2] = {.index = 1, .length = 4},
  [3] = {.index = 5, .length = 3},
  [4] = {.index = 8, .length = 1},
  [5] = {.index = 9, .length = 6},
  [6] = {.index = 15, .length = 1},
  [7] = {.index = 16, .length = 2},
};

static const TSFieldMapEntry ts_field_map_entries[] = {
  [0] =
    {field_operation, 0},
  [1] =
    {field_close_paren, 3},
    {field_key, 2},
    {field_open_paren, 1},
    {field_operation_name, 0},
  [5] =
    {field_close_angle, 2},
    {field_number, 1},
    {field_open_angle, 0},
  [8] =
    {field_or, 1, .inherited = true},
  [9] =
    {field_close_paren, 5},
    {field_comma, 3},
    {field_left, 2},
    {field_open_paren, 1},
    {field_operation_name, 0},
    {field_right, 4},
  [15] =
    {field_or, 0},
  [16] =
    {field_or, 0, .inherited = true},
    {field_or, 1, .inherited = true},
};

static const TSSymbol ts_alias_sequences[PRODUCTION_ID_COUNT][MAX_ALIAS_SEQUENCE_LENGTH] = {
  [0] = {0},
};

static const uint16_t ts_non_terminal_alias_map[] = {
  0,
};

static const TSStateId ts_primary_state_ids[STATE_COUNT] = {
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
  [4] = 4,
  [5] = 5,
  [6] = 6,
  [7] = 7,
  [8] = 8,
  [9] = 9,
  [10] = 10,
  [11] = 11,
  [12] = 12,
  [13] = 13,
  [14] = 14,
  [15] = 15,
  [16] = 16,
  [17] = 17,
  [18] = 18,
  [19] = 19,
  [20] = 20,
  [21] = 21,
  [22] = 22,
  [23] = 23,
  [24] = 24,
  [25] = 25,
  [26] = 26,
  [27] = 27,
  [28] = 28,
  [29] = 29,
  [30] = 30,
  [31] = 31,
  [32] = 32,
  [33] = 33,
  [34] = 34,
  [35] = 35,
  [36] = 36,
  [37] = 37,
  [38] = 38,
};

static bool ts_lex(TSLexer *lexer, TSStateId state) {
  START_LEXER();
  eof = lexer->eof(lexer);
  switch (state) {
    case 0:
      if (eof) ADVANCE(31);
      ADVANCE_MAP(
        '(', 49,
        ')', 50,
        ',', 52,
        '-', 1,
        '0', 36,
        '1', 37,
        '2', 38,
        '3', 39,
        '4', 40,
        '5', 41,
        '6', 42,
        '7', 43,
        '8', 44,
        '9', 45,
        '<', 34,
        '>', 46,
        's', 4,
        't', 22,
        '{', 32,
        '|', 30,
        '}', 33,
      );
      if (('\t' <= lookahead && lookahead <= '\r') ||
          lookahead == ' ') SKIP(0);
      END_STATE();
    case 1:
      if (lookahead == '1') ADVANCE(35);
      END_STATE();
    case 2:
      if (lookahead == 'S') ADVANCE(13);
      END_STATE();
    case 3:
      if (lookahead == 'T') ADVANCE(7);
      END_STATE();
    case 4:
      if (lookahead == 'a') ADVANCE(17);
      if (lookahead == 'e') ADVANCE(24);
      if (lookahead == 'w') ADVANCE(5);
      END_STATE();
    case 5:
      if (lookahead == 'a') ADVANCE(21);
      END_STATE();
    case 6:
      if (lookahead == 'a') ADVANCE(18);
      END_STATE();
    case 7:
      if (lookahead == 'a') ADVANCE(14);
      END_STATE();
    case 8:
      if (lookahead == 'e') ADVANCE(53);
      END_STATE();
    case 9:
      if (lookahead == 'e') ADVANCE(51);
      END_STATE();
    case 10:
      if (lookahead == 'i') ADVANCE(29);
      END_STATE();
    case 11:
      if (lookahead == 'i') ADVANCE(28);
      END_STATE();
    case 12:
      if (lookahead == 'i') ADVANCE(26);
      END_STATE();
    case 13:
      if (lookahead == 'i') ADVANCE(19);
      END_STATE();
    case 14:
      if (lookahead == 'i') ADVANCE(20);
      END_STATE();
    case 15:
      if (lookahead == 'i') ADVANCE(27);
      END_STATE();
    case 16:
      if (lookahead == 'k') ADVANCE(48);
      END_STATE();
    case 17:
      if (lookahead == 'n') ADVANCE(12);
      END_STATE();
    case 18:
      if (lookahead == 'n') ADVANCE(23);
      END_STATE();
    case 19:
      if (lookahead == 'n') ADVANCE(16);
      END_STATE();
    case 20:
      if (lookahead == 'n') ADVANCE(25);
      END_STATE();
    case 21:
      if (lookahead == 'p') ADVANCE(3);
      END_STATE();
    case 22:
      if (lookahead == 'r') ADVANCE(6);
      END_STATE();
    case 23:
      if (lookahead == 's') ADVANCE(15);
      END_STATE();
    case 24:
      if (lookahead == 't') ADVANCE(2);
      END_STATE();
    case 25:
      if (lookahead == 't') ADVANCE(54);
      END_STATE();
    case 26:
      if (lookahead == 't') ADVANCE(10);
      END_STATE();
    case 27:
      if (lookahead == 't') ADVANCE(11);
      END_STATE();
    case 28:
      if (lookahead == 'v') ADVANCE(9);
      END_STATE();
    case 29:
      if (lookahead == 'z') ADVANCE(8);
      END_STATE();
    case 30:
      if (lookahead == '|') ADVANCE(47);
      END_STATE();
    case 31:
      ACCEPT_TOKEN(ts_builtin_sym_end);
      END_STATE();
    case 32:
      ACCEPT_TOKEN(anon_sym_LBRACE);
      END_STATE();
    case 33:
      ACCEPT_TOKEN(anon_sym_RBRACE);
      END_STATE();
    case 34:
      ACCEPT_TOKEN(anon_sym_LT);
      END_STATE();
    case 35:
      ACCEPT_TOKEN(anon_sym_DASH1);
      END_STATE();
    case 36:
      ACCEPT_TOKEN(anon_sym_0);
      END_STATE();
    case 37:
      ACCEPT_TOKEN(anon_sym_1);
      END_STATE();
    case 38:
      ACCEPT_TOKEN(anon_sym_2);
      END_STATE();
    case 39:
      ACCEPT_TOKEN(anon_sym_3);
      END_STATE();
    case 40:
      ACCEPT_TOKEN(anon_sym_4);
      END_STATE();
    case 41:
      ACCEPT_TOKEN(anon_sym_5);
      END_STATE();
    case 42:
      ACCEPT_TOKEN(anon_sym_6);
      END_STATE();
    case 43:
      ACCEPT_TOKEN(anon_sym_7);
      END_STATE();
    case 44:
      ACCEPT_TOKEN(anon_sym_8);
      END_STATE();
    case 45:
      ACCEPT_TOKEN(anon_sym_9);
      END_STATE();
    case 46:
      ACCEPT_TOKEN(anon_sym_GT);
      END_STATE();
    case 47:
      ACCEPT_TOKEN(anon_sym_PIPE_PIPE);
      END_STATE();
    case 48:
      ACCEPT_TOKEN(anon_sym_setSink);
      END_STATE();
    case 49:
      ACCEPT_TOKEN(anon_sym_LPAREN);
      END_STATE();
    case 50:
      ACCEPT_TOKEN(anon_sym_RPAREN);
      END_STATE();
    case 51:
      ACCEPT_TOKEN(anon_sym_transitive);
      END_STATE();
    case 52:
      ACCEPT_TOKEN(anon_sym_COMMA);
      END_STATE();
    case 53:
      ACCEPT_TOKEN(anon_sym_sanitize);
      END_STATE();
    case 54:
      ACCEPT_TOKEN(anon_sym_swapTaint);
      END_STATE();
    default:
      return false;
  }
}

static const TSLexMode ts_lex_modes[STATE_COUNT] = {
  [0] = {.lex_state = 0},
  [1] = {.lex_state = 0},
  [2] = {.lex_state = 0},
  [3] = {.lex_state = 0},
  [4] = {.lex_state = 0},
  [5] = {.lex_state = 0},
  [6] = {.lex_state = 0},
  [7] = {.lex_state = 0},
  [8] = {.lex_state = 0},
  [9] = {.lex_state = 0},
  [10] = {.lex_state = 0},
  [11] = {.lex_state = 0},
  [12] = {.lex_state = 0},
  [13] = {.lex_state = 0},
  [14] = {.lex_state = 0},
  [15] = {.lex_state = 0},
  [16] = {.lex_state = 0},
  [17] = {.lex_state = 0},
  [18] = {.lex_state = 0},
  [19] = {.lex_state = 0},
  [20] = {.lex_state = 0},
  [21] = {.lex_state = 0},
  [22] = {.lex_state = 0},
  [23] = {.lex_state = 0},
  [24] = {.lex_state = 0},
  [25] = {.lex_state = 0},
  [26] = {.lex_state = 0},
  [27] = {.lex_state = 0},
  [28] = {.lex_state = 0},
  [29] = {.lex_state = 0},
  [30] = {.lex_state = 0},
  [31] = {.lex_state = 0},
  [32] = {.lex_state = 0},
  [33] = {.lex_state = 0},
  [34] = {.lex_state = 0},
  [35] = {.lex_state = 0},
  [36] = {.lex_state = 0},
  [37] = {.lex_state = 0},
  [38] = {.lex_state = 0},
};

static const uint16_t ts_parse_table[LARGE_STATE_COUNT][SYMBOL_COUNT] = {
  [0] = {
    [ts_builtin_sym_end] = ACTIONS(1),
    [anon_sym_LBRACE] = ACTIONS(1),
    [anon_sym_RBRACE] = ACTIONS(1),
    [anon_sym_LT] = ACTIONS(1),
    [anon_sym_DASH1] = ACTIONS(1),
    [anon_sym_0] = ACTIONS(1),
    [anon_sym_1] = ACTIONS(1),
    [anon_sym_2] = ACTIONS(1),
    [anon_sym_3] = ACTIONS(1),
    [anon_sym_4] = ACTIONS(1),
    [anon_sym_5] = ACTIONS(1),
    [anon_sym_6] = ACTIONS(1),
    [anon_sym_7] = ACTIONS(1),
    [anon_sym_8] = ACTIONS(1),
    [anon_sym_9] = ACTIONS(1),
    [anon_sym_GT] = ACTIONS(1),
    [anon_sym_PIPE_PIPE] = ACTIONS(1),
    [anon_sym_setSink] = ACTIONS(1),
    [anon_sym_LPAREN] = ACTIONS(1),
    [anon_sym_RPAREN] = ACTIONS(1),
    [anon_sym_transitive] = ACTIONS(1),
    [anon_sym_COMMA] = ACTIONS(1),
    [anon_sym_sanitize] = ACTIONS(1),
    [anon_sym_swapTaint] = ACTIONS(1),
  },
  [1] = {
    [sym_source_file] = STATE(37),
    [sym_taint_summary] = STATE(36),
    [anon_sym_LBRACE] = ACTIONS(3),
  },
};

static const uint16_t ts_small_parse_table[] = {
  [0] = 8,
    ACTIONS(5), 1,
      anon_sym_RBRACE,
    ACTIONS(7), 1,
      anon_sym_setSink,
    ACTIONS(9), 1,
      anon_sym_transitive,
    ACTIONS(11), 1,
      anon_sym_sanitize,
    ACTIONS(13), 1,
      anon_sym_swapTaint,
    STATE(34), 1,
      sym_side_effects,
    STATE(3), 2,
      sym_taint_side_effect,
      aux_sym_side_effects_repeat1,
    STATE(7), 4,
      sym_set_sink,
      sym_transitive,
      sym_sanitize,
      sym_swap_taint,
  [29] = 7,
    ACTIONS(7), 1,
      anon_sym_setSink,
    ACTIONS(9), 1,
      anon_sym_transitive,
    ACTIONS(11), 1,
      anon_sym_sanitize,
    ACTIONS(13), 1,
      anon_sym_swapTaint,
    ACTIONS(15), 1,
      anon_sym_RBRACE,
    STATE(4), 2,
      sym_taint_side_effect,
      aux_sym_side_effects_repeat1,
    STATE(7), 4,
      sym_set_sink,
      sym_transitive,
      sym_sanitize,
      sym_swap_taint,
  [55] = 7,
    ACTIONS(17), 1,
      anon_sym_RBRACE,
    ACTIONS(19), 1,
      anon_sym_setSink,
    ACTIONS(22), 1,
      anon_sym_transitive,
    ACTIONS(25), 1,
      anon_sym_sanitize,
    ACTIONS(28), 1,
      anon_sym_swapTaint,
    STATE(4), 2,
      sym_taint_side_effect,
      aux_sym_side_effects_repeat1,
    STATE(7), 4,
      sym_set_sink,
      sym_transitive,
      sym_sanitize,
      sym_swap_taint,
  [81] = 1,
    ACTIONS(31), 11,
      anon_sym_DASH1,
      anon_sym_0,
      anon_sym_1,
      anon_sym_2,
      anon_sym_3,
      anon_sym_4,
      anon_sym_5,
      anon_sym_6,
      anon_sym_7,
      anon_sym_8,
      anon_sym_9,
  [95] = 1,
    ACTIONS(33), 5,
      anon_sym_RBRACE,
      anon_sym_setSink,
      anon_sym_transitive,
      anon_sym_sanitize,
      anon_sym_swapTaint,
  [103] = 1,
    ACTIONS(35), 5,
      anon_sym_RBRACE,
      anon_sym_setSink,
      anon_sym_transitive,
      anon_sym_sanitize,
      anon_sym_swapTaint,
  [111] = 1,
    ACTIONS(37), 5,
      anon_sym_RBRACE,
      anon_sym_setSink,
      anon_sym_transitive,
      anon_sym_sanitize,
      anon_sym_swapTaint,
  [119] = 1,
    ACTIONS(39), 5,
      anon_sym_RBRACE,
      anon_sym_setSink,
      anon_sym_transitive,
      anon_sym_sanitize,
      anon_sym_swapTaint,
  [127] = 1,
    ACTIONS(41), 5,
      anon_sym_RBRACE,
      anon_sym_setSink,
      anon_sym_transitive,
      anon_sym_sanitize,
      anon_sym_swapTaint,
  [135] = 3,
    ACTIONS(43), 1,
      anon_sym_LT,
    STATE(13), 1,
      sym_key,
    STATE(35), 1,
      sym_keys,
  [145] = 1,
    ACTIONS(45), 3,
      anon_sym_PIPE_PIPE,
      anon_sym_RPAREN,
      anon_sym_COMMA,
  [151] = 3,
    ACTIONS(47), 1,
      anon_sym_PIPE_PIPE,
    ACTIONS(49), 1,
      anon_sym_RPAREN,
    STATE(14), 1,
      aux_sym_keys_repeat1,
  [161] = 3,
    ACTIONS(47), 1,
      anon_sym_PIPE_PIPE,
    ACTIONS(51), 1,
      anon_sym_RPAREN,
    STATE(15), 1,
      aux_sym_keys_repeat1,
  [171] = 3,
    ACTIONS(53), 1,
      anon_sym_PIPE_PIPE,
    ACTIONS(56), 1,
      anon_sym_RPAREN,
    STATE(15), 1,
      aux_sym_keys_repeat1,
  [181] = 2,
    ACTIONS(43), 1,
      anon_sym_LT,
    STATE(38), 1,
      sym_key,
  [188] = 2,
    ACTIONS(43), 1,
      anon_sym_LT,
    STATE(21), 1,
      sym_key,
  [195] = 2,
    ACTIONS(43), 1,
      anon_sym_LT,
    STATE(27), 1,
      sym_key,
  [202] = 2,
    ACTIONS(43), 1,
      anon_sym_LT,
    STATE(28), 1,
      sym_key,
  [209] = 2,
    ACTIONS(43), 1,
      anon_sym_LT,
    STATE(26), 1,
      sym_key,
  [216] = 1,
    ACTIONS(58), 2,
      anon_sym_PIPE_PIPE,
      anon_sym_RPAREN,
  [221] = 2,
    ACTIONS(43), 1,
      anon_sym_LT,
    STATE(29), 1,
      sym_key,
  [228] = 1,
    ACTIONS(60), 1,
      ts_builtin_sym_end,
  [232] = 1,
    ACTIONS(62), 1,
      ts_builtin_sym_end,
  [236] = 1,
    ACTIONS(64), 1,
      anon_sym_LPAREN,
  [240] = 1,
    ACTIONS(66), 1,
      anon_sym_RPAREN,
  [244] = 1,
    ACTIONS(68), 1,
      anon_sym_COMMA,
  [248] = 1,
    ACTIONS(70), 1,
      anon_sym_RPAREN,
  [252] = 1,
    ACTIONS(72), 1,
      anon_sym_COMMA,
  [256] = 1,
    ACTIONS(74), 1,
      anon_sym_GT,
  [260] = 1,
    ACTIONS(76), 1,
      anon_sym_LPAREN,
  [264] = 1,
    ACTIONS(78), 1,
      anon_sym_LPAREN,
  [268] = 1,
    ACTIONS(80), 1,
      anon_sym_LPAREN,
  [272] = 1,
    ACTIONS(82), 1,
      anon_sym_RBRACE,
  [276] = 1,
    ACTIONS(84), 1,
      anon_sym_RPAREN,
  [280] = 1,
    ACTIONS(86), 1,
      ts_builtin_sym_end,
  [284] = 1,
    ACTIONS(88), 1,
      ts_builtin_sym_end,
  [288] = 1,
    ACTIONS(90), 1,
      anon_sym_RPAREN,
};

static const uint32_t ts_small_parse_table_map[] = {
  [SMALL_STATE(2)] = 0,
  [SMALL_STATE(3)] = 29,
  [SMALL_STATE(4)] = 55,
  [SMALL_STATE(5)] = 81,
  [SMALL_STATE(6)] = 95,
  [SMALL_STATE(7)] = 103,
  [SMALL_STATE(8)] = 111,
  [SMALL_STATE(9)] = 119,
  [SMALL_STATE(10)] = 127,
  [SMALL_STATE(11)] = 135,
  [SMALL_STATE(12)] = 145,
  [SMALL_STATE(13)] = 151,
  [SMALL_STATE(14)] = 161,
  [SMALL_STATE(15)] = 171,
  [SMALL_STATE(16)] = 181,
  [SMALL_STATE(17)] = 188,
  [SMALL_STATE(18)] = 195,
  [SMALL_STATE(19)] = 202,
  [SMALL_STATE(20)] = 209,
  [SMALL_STATE(21)] = 216,
  [SMALL_STATE(22)] = 221,
  [SMALL_STATE(23)] = 228,
  [SMALL_STATE(24)] = 232,
  [SMALL_STATE(25)] = 236,
  [SMALL_STATE(26)] = 240,
  [SMALL_STATE(27)] = 244,
  [SMALL_STATE(28)] = 248,
  [SMALL_STATE(29)] = 252,
  [SMALL_STATE(30)] = 256,
  [SMALL_STATE(31)] = 260,
  [SMALL_STATE(32)] = 264,
  [SMALL_STATE(33)] = 268,
  [SMALL_STATE(34)] = 272,
  [SMALL_STATE(35)] = 276,
  [SMALL_STATE(36)] = 280,
  [SMALL_STATE(37)] = 284,
  [SMALL_STATE(38)] = 288,
};

static const TSParseActionEntry ts_parse_actions[] = {
  [0] = {.entry = {.count = 0, .reusable = false}},
  [1] = {.entry = {.count = 1, .reusable = false}}, RECOVER(),
  [3] = {.entry = {.count = 1, .reusable = true}}, SHIFT(2),
  [5] = {.entry = {.count = 1, .reusable = true}}, SHIFT(24),
  [7] = {.entry = {.count = 1, .reusable = true}}, SHIFT(25),
  [9] = {.entry = {.count = 1, .reusable = true}}, SHIFT(31),
  [11] = {.entry = {.count = 1, .reusable = true}}, SHIFT(32),
  [13] = {.entry = {.count = 1, .reusable = true}}, SHIFT(33),
  [15] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_side_effects, 1, 0, 0),
  [17] = {.entry = {.count = 1, .reusable = true}}, REDUCE(aux_sym_side_effects_repeat1, 2, 0, 0),
  [19] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_side_effects_repeat1, 2, 0, 0), SHIFT_REPEAT(25),
  [22] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_side_effects_repeat1, 2, 0, 0), SHIFT_REPEAT(31),
  [25] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_side_effects_repeat1, 2, 0, 0), SHIFT_REPEAT(32),
  [28] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_side_effects_repeat1, 2, 0, 0), SHIFT_REPEAT(33),
  [31] = {.entry = {.count = 1, .reusable = true}}, SHIFT(30),
  [33] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_set_sink, 4, 0, 2),
  [35] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_taint_side_effect, 1, 0, 1),
  [37] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_sanitize, 4, 0, 2),
  [39] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_transitive, 6, 0, 5),
  [41] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_swap_taint, 6, 0, 5),
  [43] = {.entry = {.count = 1, .reusable = true}}, SHIFT(5),
  [45] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_key, 3, 0, 3),
  [47] = {.entry = {.count = 1, .reusable = true}}, SHIFT(17),
  [49] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_keys, 1, 0, 0),
  [51] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_keys, 2, 0, 4),
  [53] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_keys_repeat1, 2, 0, 7), SHIFT_REPEAT(17),
  [56] = {.entry = {.count = 1, .reusable = true}}, REDUCE(aux_sym_keys_repeat1, 2, 0, 7),
  [58] = {.entry = {.count = 1, .reusable = true}}, REDUCE(aux_sym_keys_repeat1, 2, 0, 6),
  [60] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_taint_summary, 3, 0, 0),
  [62] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_taint_summary, 2, 0, 0),
  [64] = {.entry = {.count = 1, .reusable = true}}, SHIFT(20),
  [66] = {.entry = {.count = 1, .reusable = true}}, SHIFT(6),
  [68] = {.entry = {.count = 1, .reusable = true}}, SHIFT(11),
  [70] = {.entry = {.count = 1, .reusable = true}}, SHIFT(8),
  [72] = {.entry = {.count = 1, .reusable = true}}, SHIFT(16),
  [74] = {.entry = {.count = 1, .reusable = true}}, SHIFT(12),
  [76] = {.entry = {.count = 1, .reusable = true}}, SHIFT(18),
  [78] = {.entry = {.count = 1, .reusable = true}}, SHIFT(19),
  [80] = {.entry = {.count = 1, .reusable = true}}, SHIFT(22),
  [82] = {.entry = {.count = 1, .reusable = true}}, SHIFT(23),
  [84] = {.entry = {.count = 1, .reusable = true}}, SHIFT(9),
  [86] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_source_file, 1, 0, 0),
  [88] = {.entry = {.count = 1, .reusable = true}},  ACCEPT_INPUT(),
  [90] = {.entry = {.count = 1, .reusable = true}}, SHIFT(10),
};

#ifdef __cplusplus
extern "C" {
#endif
#ifdef TREE_SITTER_HIDE_SYMBOLS
#define TS_PUBLIC
#elif defined(_WIN32)
#define TS_PUBLIC __declspec(dllexport)
#else
#define TS_PUBLIC __attribute__((visibility("default")))
#endif

TS_PUBLIC const TSLanguage *tree_sitter_taint(void) {
  static const TSLanguage language = {
    .version = LANGUAGE_VERSION,
    .symbol_count = SYMBOL_COUNT,
    .alias_count = ALIAS_COUNT,
    .token_count = TOKEN_COUNT,
    .external_token_count = EXTERNAL_TOKEN_COUNT,
    .state_count = STATE_COUNT,
    .large_state_count = LARGE_STATE_COUNT,
    .production_id_count = PRODUCTION_ID_COUNT,
    .field_count = FIELD_COUNT,
    .max_alias_sequence_length = MAX_ALIAS_SEQUENCE_LENGTH,
    .parse_table = &ts_parse_table[0][0],
    .small_parse_table = ts_small_parse_table,
    .small_parse_table_map = ts_small_parse_table_map,
    .parse_actions = ts_parse_actions,
    .symbol_names = ts_symbol_names,
    .field_names = ts_field_names,
    .field_map_slices = ts_field_map_slices,
    .field_map_entries = ts_field_map_entries,
    .symbol_metadata = ts_symbol_metadata,
    .public_symbol_map = ts_symbol_map,
    .alias_map = ts_non_terminal_alias_map,
    .alias_sequences = &ts_alias_sequences[0][0],
    .lex_modes = ts_lex_modes,
    .lex_fn = ts_lex,
    .primary_state_ids = ts_primary_state_ids,
  };
  return &language;
}
#ifdef __cplusplus
}
#endif
