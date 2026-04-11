from tree_sitter import Language, Parser
import tree_sitter_memory as tsmemory
import pandas as pd

MEMORY_LANGUAGE = Language(tsmemory.language())

class MemoryComparator:
  def __init__(self):
    self.parser = Parser(MEMORY_LANGUAGE)

  def serialize_object(self, node):
    type_node = node.children[0]
    key = None
    match type_node.type:
      case 'key':
        key = 'key' + type_node.child_by_field_name('number').text.decode('utf-8')
      case 'key_attribute':
        key = ('key' + type_node.child_by_field_name('key')
                      .child_by_field_name('number').text.decode('utf-8'),
                type_node.child_by_field_name('attribute').text.decode('utf-8'))
      case 'constant_value':
        key = type_node.text.decode('utf-8')
      case '_':
        print('Unknown object type')
    return key

  def serialize_binary_expression(self, node):
    op = node.child_by_field_name('operator').text.decode('utf-8')
    left = node.child_by_field_name('left')
    right = node.child_by_field_name('right')
    left_value = self.serialize_expression(left)
    right_value = self.serialize_expression(right)
    left_list = []
    right_list = []
    expr = ()
    if op == '+' or op == '*':
      if isinstance(left_value, tuple) and left_value[0] == op:
        for item in left_value[1]:
          left_list.append(item)
      if isinstance(right_value, tuple) and right_value[0] == op:
        for item in right_value[1]:
          right_list.append(item)
      if len(left_list) > 0 and len(right_list) > 0:
        expr = (op, frozenset(left_list + right_list))
      elif len(left_list) > 0:
        expr = (op, frozenset(left_list + [right_value]))
      elif len(right_list) > 0:
        expr = (op, frozenset(right_list + [left_value]))
      else:
        expr = (op, frozenset([left_value, right_value]))
    elif op == '-' or op == '/':
        expr = (op, left_value, right_value)
    return expr
  # expression == single_value
  def serialize_expression(self, node):
    type_node = node.children[0]
    # print(type_node.type)
    if type_node.type == 'binary_expr':
      return self.serialize_binary_expression(type_node)
    elif type_node.type == 'object':
      return self.serialize_object(type_node)

  def serialize_value(self, node):
    type_node = node.children[0]
    value = None
    if type_node.type == 'single_value':
      value =  self.serialize_expression(type_node)
    elif type_node.type == 'range_value':
      range_left = self.serialize_expression(type_node.child_by_field_name('left'))
      range_right = self.serialize_expression(type_node.child_by_field_name('right'))
      value = (range_left, range_right)
    return value
  
  def serialize_values(self, node):
    values = []
    for child in node.children:
      if child.type == 'value':
        value = self.serialize_value(child)
        values.append(value)
    return frozenset(values)
  
  def serialize_effect(self, node):
    operation_node = node.child_by_field_name('operation')
    operation = operation_node.type
    serialized_effect = ()
    match operation:
      case 'assign':
        key = self.serialize_object(operation_node.child_by_field_name('left'))
        values = self.serialize_values(operation_node.child_by_field_name('right'))
        serialized_effect = (operation, key, values)
      case 'malloc':
        object = operation_node.child_by_field_name('object')
        key = self.serialize_object(object)
        possibility = operation_node.child_by_field_name('possibility').text.decode('utf-8')
        serialized_effect = (operation, key, possibility)
      case 'free':
        object = operation_node.child_by_field_name('object')
        key = self.serialize_object(object)
        possibility = operation_node.child_by_field_name('possibility').text.decode('utf-8')
        serialized_effect = (operation, key, possibility)
      case 'overflow':
        key = self.serialize_object(operation_node.child_by_field_name('object').children[0])
        range_node = operation_node.child_by_field_name('range_value')
        range_left = self.serialize_expression(range_node.child_by_field_name('left'))
        range_right = self.serialize_expression(range_node.child_by_field_name('right'))
        serialized_effect = (operation, key, range_left, range_right)
      case 'expansion':
        key = int(operation_node.child_by_field_name('key')
                  .child_by_field_name('number').text.decode('utf-8'))
        serialized_effect = (operation, key)
      case 'swap':
        key = int(operation_node.child_by_field_name('left')
                  .child_by_field_name('number').text.decode('utf-8'))
        key2 = int(operation_node.child_by_field_name('right')
                   .child_by_field_name('number').text.decode('utf-8'))
        serialized_effect = (operation, frozenset([key, key2]))
      case _:
        print('Unknown operation')
    return serialized_effect
  
  def dfs(self, tree):
    cursor = tree.walk()
    effects = []
    while True:
      if cursor.goto_first_child():
        if cursor.node.type == 'memory_side_effect':
          effects.append(self.serialize_effect(cursor.node))
      else:
        while True:
          if cursor.goto_next_sibling():
            if cursor.node.type == 'memory_side_effect':
              effects.append(self.serialize_effect(cursor.node))
            break
          if not cursor.goto_parent():
            return effects
  
  def extract_relation_from_tuple(self, value, read_params):
    if value[0] == '*' or value[0] == '+':
      self.extract_relation_from_set(value[1], read_params)
    elif value[0] == '-' or value[0] == '/':
      self.extract_relation_from_tuple(value[1], read_params)
      self.extract_relation_from_tuple(value[2], read_params)
    elif isinstance(value[0], str) and value[0].find('key') == 0:
      read_params.append(value)

  def extract_relation_from_set(self, value, read_params):
    for item in value:
      if isinstance(item, frozenset):
        self.extract_relation_from_set(item, read_params)
      elif isinstance(item, tuple):
        self.extract_relation_from_tuple(item, read_params)
      elif isinstance(item, str) and item.find('key') == 0:
        read_params.append(item)
  
  def extract_relation(self, operation):
    read_params = []
    write_params = []
    match operation[0]:
      case 'assign':
        write_params.append(operation[1])
        read_params.append(operation[1])
        self.extract_relation_from_set(operation[2], read_params)
      case 'malloc':
        read_params.append(operation[1])
        write_params.append(operation[1])
      case 'free':
        read_params.append(operation[1])
        write_params.append(operation[1])
      case 'overflow':
        # TODO
        pass
      case 'swap':
        for key in operation[1]:
          read_params.append(key)
          write_params.append(key)
      case _:
        print('no relation or unknown operation')
    return frozenset(read_params), frozenset(write_params)
  
  def topological_sort(self, operations):
    edges = []
    read_param_table = {}
    write_param_table = {}
    for operation in operations:
      read_params, write_params = self.extract_relation(operation)
      for param in read_params:
        # get RAW & WAW relations
        if isinstance(param, tuple):
          if param in write_param_table:
            edges.append((write_param_table[param], operation))
          elif param[0] in write_param_table:
            edges.append((write_param_table[param[0]], operation))
        else:
          found = False
          if (param, 'field') in write_param_table:
            edges.append((write_param_table[(param, 'field')], operation))
            found = True
          if (param, 'size') in write_param_table:
            edges.append((write_param_table[(param, 'size')], operation))
            found = True
          if not found and param in write_param_table:
            edges.append((write_param_table[param], operation))
      
      for param in write_params:
        # get WAR relations
        if isinstance(param, tuple):
          if param in read_param_table:
            edges.append((read_param_table[param], operation))
          elif param[0] in read_param_table:
            edges.append((read_param_table[param[0]], operation))
        else:
          found = False
          if (param, 'field') in read_param_table:
            edges.append((read_param_table[(param, 'field')], operation))
            found = True
          if (param, 'size') in read_param_table:
            edges.append((read_param_table[(param, 'size')], operation))
            found = True
          if not found and param in read_param_table:
            edges.append((read_param_table[param], operation))
      
      # remove old read params
      for param in read_params:
        if isinstance(param, str):
          if (param, 'size') in read_param_table.keys():
            read_param_table.pop((param, 'size'))
          if (param, 'field') in read_param_table.keys():
            read_param_table.pop((param, 'field'))
        read_param_table[param] = operation
      # remove old write params
      for param in write_params:
        if isinstance(param, str):
          if (param, 'size') in write_param_table.keys():
            write_param_table.pop((param, 'size'))
          if (param, 'field') in write_param_table.keys():
            write_param_table.pop((param, 'field'))
        write_param_table[param] = operation

    return frozenset(edges)

  def compare_operations(self, gt, pd):
    set_gt = set(gt)
    set_pd = set(pd)
    same = 0
    if (set_gt == set_pd):
      same = 1
    tp = 0
    fp = 0
    for effect in pd:
      if effect in set_gt:
        tp += 1
      else:
        fp += 1
    fn = len(gt) - tp
    return tp, fp, fn, same

  def compare_order(self, gt, pd):
    set_pd = set(pd)
    gt_edges = self.topological_sort(gt)
    pd_edges = self.topological_sort(pd)
    wrong_edges = []
    missing_edges = []
    for edge in pd_edges:
      if (edge[1], edge[0]) in gt_edges:
          wrong_edges.append(edge)
    for edge in gt_edges:
      if edge[0] in set_pd and edge[1] in set_pd:
        if edge not in pd_edges:
          missing_edges.append(edge)
    print('wrong edges: ', wrong_edges)
    print('missing edges: ', missing_edges)



  def compare_summary(self, gt_tree, pd_tree):
    gt = self.dfs(gt_tree)
    pd = self.dfs(pd_tree)
    self.compare_order(gt, pd)
    return self.compare_operations(gt, pd)

if __name__ == '__main__':
  gt_df = pd.read_csv('test_gt.csv')
  pd_df = pd.read_csv('test_pd.csv')
  comparator = MemoryComparator()
  if len(gt_df) != len(pd_df):
    print('length not equal')
    exit(1)
  total_tp = 0
  total_fp = 0
  total_fn = 0
  total_same = 0
  for index, row in gt_df.iterrows():
    gt_memory_summary = row['memory_summary']
    pd_memory_summary = pd_df.iloc[index]['memory_summary']
    
    if pd.isnull(pd_memory_summary):
      print('empty string', index)
      continue
    gt_tree = comparator.parser.parse(bytes(gt_memory_summary, 'utf-8'))
    pd_tree = comparator.parser.parse(bytes(pd_memory_summary, 'utf-8'))
    tp, fp, fn, same = comparator.compare_summary(gt_tree, pd_tree)
    total_tp += tp
    total_fp += fp
    total_fn += fn
    total_same += same
  
  print('TP: ', total_tp)
  print('FP: ', total_fp)
  print('FN: ', total_fn)
  print('Same: ', total_same, '/', len(gt_df))
  print('Precision: ', total_tp / (total_tp + total_fp))
  print('Recall: ', total_tp / (total_tp + total_fn))