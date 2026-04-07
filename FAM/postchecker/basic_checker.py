from tree_sitter import Language, Parser
import tree_sitter_taint as tstaint
import tree_sitter_memory as tsmemory
import random
import re
import os

TAINT_LANGUAGE = Language(tstaint.language())
MEMORY_LANGUAGE = Language(tsmemory.language())

# DSL EBNF
# ------------------------------------------------------------------------------------------------------
# TaintSummary ::= "{" SideEffects "}"

# Key             ::= "<-1>" | "<0>" | "<1>" | "<2>" | "<3>" | "<4>" | "<5>" | "<6>" | "<7>" | "<8>" | "<9>"

# Keys            ::= Key { "||" Key }

# SideEffects     ::= { TaintSideEffect }

# TaintSideEffect ::= "setSink" "(" Key ")"
#                 | "transitive" "(" Key "," Keys ")"
#                 | "sanitize" "(" Key ")"
#                 | "swapTaint" "(" Key "," Key ")"
# ------------------------------------------------------------------------------------------------------
def extract_func_body(func):
  # find single ':', not '::'
  pattern = r'(noexcept)|(throw)|(requires)|(__attribute__)'
  # find first pattern
  m = re.search(pattern, func)
  if m:
    func = func[0 : m.start()]
  m = re.finditer(r'(?<!:):(?!:)', func)
  for m in re.finditer(r'(?<!:):(?!:)', func):
    func = func[0 : m.start()]
  return func
def extract_parameters(func):
  # TODO handle error
  # TODO remove \n and \t and space
  func = extract_func_body(func)
  left_pos =  func.find('(')
  right_pos = func.rfind(')')
  param_str = func[left_pos+1 : right_pos]
  parameters = []
  # , not in (), {}, <>
  stack = []
  comma_pos = []
  for i, char in enumerate(param_str):
    if char == '(' or char == '{' or char == '<':
      stack.append((i, char))
    elif char == ')' or char == '}' or char == '>':
      stack.pop()
    elif char == ',':
      if len(stack) == 0:
        comma_pos.append(i)

  for i, pos in enumerate(comma_pos):
    if i == 0:
      parameters.append(param_str[0:pos])
    else:
      parameters.append(param_str[comma_pos[i-1]+1:pos])
  # last parameter or empty
  if len(comma_pos) == 0:
    empty = True
    for char in param_str:
      if char != ' ':
        empty = False
        break
    if not empty:
      parameters.append(param_str)
  else:
    parameters.append(param_str[comma_pos[-1]+1:])
  return left_pos, right_pos, parameters

def swap_parameters(func, keys):
  l, r, parameters = extract_parameters(func)
  # swap parameters
  parameters[keys[0] - 1], parameters[keys[1] - 1] = parameters[keys[1] - 1], parameters[keys[0] - 1]
  # reconstruct function declaration
  swapped_func = func[:l + 1]
  for parameter in parameters:
    swapped_func += parameter + ','
  swapped_func = swapped_func[:-1] + func[r:]
  return swapped_func

def swap_summary_keys(summary, keys):
  key_0 = keys[0]
  key_1 = keys[1]
  pattern_key_0 = r'<{}>'.format(key_0)
  pattern_key_1 = r'<{}>'.format(key_1)
  temp_key_0 = r'<temp0>'
  temp_key_1 = r'<temp1>'
  summary = re.sub(pattern_key_0, temp_key_0, summary)
  summary = re.sub(pattern_key_1, temp_key_1, summary)
  summary = re.sub(temp_key_0, pattern_key_1, summary)
  summary = re.sub(temp_key_1, pattern_key_0, summary)
  return summary

class TaintChecker:
  def __init__(self):
    self.taint_parser = Parser(TAINT_LANGUAGE)
    # f = open('taint.dot', 'w')
    # self.tree.print_dot_graph(f)
    # f.close()
  def parse(self, taint_summary):
    return self.taint_parser.parse(bytes(taint_summary, 'utf-8'))

  def swap_check(self, func, taint_summary):
    l, r, parameters = extract_parameters(func)
    cnt = len(parameters)
    if cnt < 2:
      print("not enough parameters")
      return
    keys = random.sample(range(1, cnt + 1), 2)
    swapped_func = swap_parameters(func, keys)
    swapped_summary = swap_summary_keys(taint_summary, keys)
    return swapped_func, swapped_summary

  def grammar_check(self, taint_summary, tree):
    cursor = tree.walk()
    error_list = []
    # dfs find errors
    while True:
      if cursor.goto_first_child():
        self.check_node(taint_summary, cursor.node, error_list)
      else:
        while True:
          if cursor.goto_next_sibling():
            self.check_node(taint_summary, cursor.node, error_list)
            break
          if not cursor.goto_parent():
            return error_list
  
  def check_node(self, taint_summary, node, error_list):
    if node.type in ("ERROR", "MISSING"):
      # error text retrieve
      error_text = taint_summary[node.start_byte:node.end_byte]
      error_list.append(error_text)
      # TODO right grammar retrieve
      if node.parent.type == 'taint_summary':
        print()

class MemoryChecker:
  def __init__(self):
    self.memory_parser = Parser(MEMORY_LANGUAGE)
    # f = open('memory.dot', 'w')
    # self.tree.print_dot_graph(f)
    # f.close()

  def parse(self, memory_summary):
    return self.memory_parser.parse(bytes(memory_summary, 'utf-8'))
  
  def swap_check(self, func, memory_summary):
    l, r, parameters = extract_parameters(func)
    cnt = len(parameters)
    if cnt < 2:
      print("not enough parameters")
      return
    keys = random.sample(range(1, cnt + 1), 2)
    swapped_func = swap_parameters(func, keys)
    swapped_summary = swap_summary_keys(memory_summary, keys)
    return swapped_func, swapped_summary

  def grammar_check(self, memory_summary, tree):
    cursor = tree.walk()
    error_list = []
    # dfs find errors
    while True:
      if cursor.goto_first_child():
        self.check_node(memory_summary, cursor.node, error_list)
      else:
        while True:
          if cursor.goto_next_sibling():
            self.check_node(memory_summary, cursor.node, error_list)
            break
          if not cursor.goto_parent():
            return error_list

  def check_node(self, memory_summary, node, error_list):
    if node.type in ("ERROR", "MISSING"):
      # error text retrieve
      error_text = memory_summary[node.start_byte:node.end_byte]
      error_list.append(error_text)


if __name__ == '__main__':
  # f = open('test.txt', 'r')
  # summary = f.read()
  # f.close()
  # checker = TaintChecker()
  # func = 'template< class InputIt > iterator insert( const_iterator pos, InputIt first, InputIt last, Test(int a, int b, Fuck(int c, int d)));'
  # checker.swap_check(func, summary)
  # tree = checker.parse(summary)
  # checker.grammar_check(summary, tree)
  # checker = MemoryChecker()
  # tree = checker.parse(summary)
  # checker.grammar_check(summary, tree)
  # checker.swap_check(func, summary)
  pass
  