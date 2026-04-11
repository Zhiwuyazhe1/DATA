/**
 * @file taint dsl parser
 * @author bruce <zhiwuyazhe154@gmail.com>
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: "taint",

  // 跳过空白符号
  extras: () => [
    /\s/
  ],

  // 优先级设定
  precedences: $ => [
    [
      'error',
      'token',
      'operation'
    ]
  ],

  rules: {
    source_file: $ => $.taint_summary, // 避免空串通过检查
    taint_summary: $ => seq(
      '{',
      optional($.side_effects),
      '}'
    ),
    
    side_effects: $ => repeat1(
      $.taint_side_effect
    ),

    taint_side_effect: $ => choice(
      // 提高正常操作的优先级
      prec('operation', field('operation', $.set_sink)),
      prec('operation', field('operation', $.transitive)),
      prec('operation', field('operation', $.sanitize)),
      prec('operation', field('operation', $.swap_taint)),
    ),

    // Key 规则
    key: $ => seq(
      field('open_angle', '<'),
      field('number', choice(
        '-1', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      )),
      field('close_angle', '>')
    ),

    keys: $ => seq(
      $.key,
      repeat(
        seq(
          field('or', '||'), 
          $.key
        )
      )
    ),

    // 正确的操作定义
    set_sink: $ => seq(
      field('operation_name', 'setSink'),
      field('open_paren', '('),
      field('key', $.key),
      field('close_paren', ')'),
    ),

    transitive: $ => seq(
      field('operation_name', 'transitive'),
      field('open_paren', '('),
      field('left', $.key),
      field('comma', ','),
      field('right', $.keys),
      field('close_paren', ')'),
    ),

    sanitize: $ => seq(
      field('operation_name', 'sanitize'),
      field('open_paren', '('),
      field('key', $.key),
      field('close_paren', ')'),
    ),

    swap_taint: $ => seq(
      field('operation_name', 'swapTaint'),
      field('open_paren', '('),
      field('left', $.key),
      field('comma', ','),
      field('right', $.key),
      field('close_paren', ')'),
    )
  }
});
