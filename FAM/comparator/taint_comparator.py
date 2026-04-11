from tree_sitter import Language, Parser
import tree_sitter_taint as tstaint
import pandas as pd
import os

# 

TAINT_LANGUAGE = Language(tstaint.language())

class TaintComparator:
  def __init__(self):
    self.parser = Parser(TAINT_LANGUAGE)
  
  # extract operation info and data dependencies from taint_side_effect node
  def serialize_effect(self, node):
    operation_node = node.child_by_field_name('operation')
    operation = operation_node.type
    serialized_effect = ()
    if operation == 'set_sink':
      key = int(operation_node.child_by_field_name('key')
                .child_by_field_name('number').text.decode('utf-8'))
      serialized_effect = (operation, key)

    elif operation == 'transitive':
      key = int(operation_node.child_by_field_name('left')
                .child_by_field_name('number').text.decode('utf-8'))
      keys_node = operation_node.child_by_field_name('right')
      keys_list = []
      for child in keys_node.children:
        if child.type == 'key':
          keys_list.append(int(child.child_by_field_name('number')
                               .text.decode('utf-8')))
      serialized_effect = (operation, key, frozenset(keys_list))

    elif operation == 'sanitize':
      key = int(operation_node.child_by_field_name('key')
                .child_by_field_name('number').text.decode('utf-8'))
      serialized_effect = (operation, key)

    elif operation == 'swap_taint':
      key = int(operation_node.child_by_field_name('left')
                .child_by_field_name('number').text.decode('utf-8'))
      key2 = int(operation_node.child_by_field_name('right')
                 .child_by_field_name('number').text.decode('utf-8'))
      serialized_effect = (operation, frozenset([key, key2]))

    else:
      print('Unknown operation')
    return serialized_effect
  
  def dfs(self, tree):
    cursor = tree.walk()
    effects = []
    while True:
      if cursor.goto_first_child():
        if cursor.node.type == 'taint_side_effect':
          effects.append(self.serialize_effect(cursor.node))
      else:
        while True:
          if cursor.goto_next_sibling():
            if cursor.node.type == 'taint_side_effect':
              effects.append(self.serialize_effect(cursor.node))
            break
          if not cursor.goto_parent():
            return effects
          
  def extract_relation(self, operation):
    read_params = []
    write_params = []
    match operation[0]:
      case 'transitive':
        write_params.append(operation[1])
        read_params.append(operation[1])
        for key in operation[2]:
          read_params.append(key)
      case 'sanitize':
        write_params.append(operation[1])
        read_params.append(operation[1])
      case 'swap_taint':
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
      # get WAW & RAW relations
      for param in read_params:
        if param in write_param_table:
          edges.append((write_param_table[param], operation))
      # get WAR relations
      for param in write_params:
        if param in read_param_table:
          edges.append((read_param_table[param], operation))
      
      for param in read_params:
        read_param_table[param] = operation
      for param in write_params:
        write_param_table[param] = operation
    return frozenset(edges)

  # compare (ground truth and prediction)
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
    # empty not calculate
    gt = self.dfs(gt_tree)
    pd = self.dfs(pd_tree)
    self.compare_order(gt, pd)
    return self.compare_operations(gt, pd)

  
if __name__ == '__main__':
  # f = open('test.txt', 'r')
  # taint_summary = f.read()
  # f.close()
  gt_df = pd.read_csv('test_gt.csv')
  pd_df = pd.read_csv('test_pd.csv')
  comparator = TaintComparator()
  total_tp = 0
  total_fp = 0
  total_fn = 0
  total_same = 0
  if (len(gt_df) != len(pd_df)):
    print('Length not equal')
    exit(1)
  lists = []
  for index, row in gt_df.iterrows():
    gt_taint_summary = row['taint_summary']
    pd_taint_summary = pd_df.iloc[index]['taint_summary']
    gt_tree = comparator.parser.parse(bytes(gt_taint_summary, 'utf-8'))
    pd_tree = comparator.parser.parse(bytes(pd_taint_summary, 'utf-8'))
    tp, fp, fn, same = comparator.compare_summary(gt_tree, pd_tree)
    lists.append([row['func'], gt_taint_summary, pd_taint_summary, tp, fp, fn, same])
    total_tp += tp
    total_fp += fp
    total_fn += fn
    total_same += same
  
  columns = ['func', 'gt', 'pd', 'tp', 'fp', 'fn', 'same']
  result = pd.DataFrame(columns=columns, data=lists)
  result.to_csv('compare_result.csv', index=False, encoding='utf-8-sig')
  print('TP: ', total_tp)
  print('FP: ', total_fp)
  print('FN: ', total_fn)
  print('Same: ', total_same, '/', len(gt_df))
  print('Precision: ', total_tp / (total_tp + total_fp))
  print('Recall: ', total_tp / (total_tp + total_fn))
  # print('F1: ', 2 * total_tp / (2 * total_tp + total_fp + total_fn))
  # print('Accuracy: ', total_tp / (total_tp + total_fp + total_fn))