import time
import pandas as pd
import re
import os
import json
from modeler import chat_client, prompt_generator
from comparator import taint_comparator
from comparator import memory_comparator
from postchecker import basic_checker
from tree_sitter import Language, Parser
import tree_sitter_taint as tstaint
import tree_sitter_memory as tsmemory
import random
import datetime
import logging

# logger config
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

adapter = logging.LoggerAdapter(logger, {'route' : None})

# stdout handler
stream_handler = logging.StreamHandler()
stream_handler.setLevel(logging.DEBUG)
stream_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
logger.addHandler(stream_handler)

paramTypePool = ['int', 'char', 'double', 'int *']
parmNamePool = []
for i in range(100):
  parmNamePool.append('param' + str(i))

class RouteFilter(logging.Filter):
  def __init__(self, route):
    super().__init__()
    self.route = route

  def filter(self, record):
    route = getattr(record, 'route', None)
    return route == self.route or route is None


# extract summary from the model reply
# TODO process fail
def summary_extract(reply):
  if reply == 'request error!':
    return ''
  pattern = r'```final_summary(.*?)```'
  matches = re.findall(pattern, reply, re.DOTALL)
  if matches:
    return matches[-1]
  else:
    pattern = r'final_summary{(.*?)}'
    matches = re.findall(pattern, reply, re.DOTALL)
    if matches:
      return '{' + matches[-1] + '}'
    else:
      logger.error('正则匹配失败，请检查模型输出：\n{}'.format(reply))
      return '{}'

def generate_summary(client):
  reply = client.get_reply()
  summary = summary_extract(reply)
  return summary, reply

def grammar_check(summary, type):
  checker = None
  if type == 'taint':
    checker = basic_checker.TaintChecker()
  elif type == 'memory':
    checker = basic_checker.MemoryChecker()
  else:
    # TODO
    pass
  if checker is not None:
    tree = checker.parse(summary)
    return checker.grammar_check(summary, tree)
  
def add_param_check(func, count):
  l, r, parameters = basic_checker.extract_parameters(func)
  paramNames = random.sample(parmNamePool, count)
  for i in range(count):
    param = random.choice(paramTypePool) + ' ' + paramNames[i]
    parameters.append(param)
  # reconstruct function declaration
  filled_func = func[:l + 1]
  for parameter in parameters:
    filled_func += parameter + ','
  filled_func = filled_func[:-1] + func[r:]
  return filled_func


def swap_check(func, summary, type):
  checker = None
  if type == 'taint':
    checker = basic_checker.TaintChecker()
  elif type == 'memory':
    checker = basic_checker.MemoryChecker()
  else:
    pass
  if checker is not None:
    return checker.swap_check(func, summary)


def fill_client(client, prompts):
  prompt_str = ''
  for prompt in prompts:
    for content in prompt:
      prompt_str += content + '\n'
  client.add_user_message(client.base_prompt + '\n' + prompt_str)
  return prompt_str

def post_check_score(row, client, config, type, generator):
  # generator = prompt_generator.PromptGenerator(type, config, logger)
  # TODO config k
  # knowledge_prompt, examples_prompt, input_prompt = generator.generate_prompt(row, config['similar_example_num'])
  knowledge_prompt, examples_prompt, input_prompt, score, scores = generator.generate_prompt_with_score(row, config['similar_example_num'])
  append_prompt = []
  input_str = fill_client(client, [knowledge_prompt, examples_prompt, input_prompt])
  for index, example in enumerate(examples_prompt):
    logger.debug('example {}: {}'.format(index, example), extra={'route': type})
    logger.debug('example score: {}, after map: {}'.format(scores[index][0], scores[index][1]), extra={'route': type})
  logger.debug('input score: {}'.format(score), extra={'route': type})
  logger.debug('input prompt: {}'.format(input_str), extra={'route': type})
  summary, reply = generate_summary(client)
  logger.debug('model reply: {}'.format(reply), extra={'route': type})
  logger.debug('grammar checker info:', extra={'route': type})

  # grammar check
  grammar_error = False
  if (config['post_checkers']['grammar_checker']['enable'] == True):
    retry = config['post_checkers']['grammar_checker']['retry_times']
    for i in range(retry):
      logger.debug('grammar check round {}/{}'.format(i, retry), extra={'route': type})
      errors = grammar_check(summary, type)
      if (len(errors) == 0):
        logger.debug('grammar check pass', extra={'route': type})
        break
      else:
        message = '以下的摘要串存在错误，请你根据错误的串和正确的文法进行修改，直接输出修改后的摘要即可，无需思考过程：\n'
        for error in errors:
          message += error + '\n'
        client.add_user_message(message)
        logger.debug('grammar check error:\n {}'.format(message), extra={'route': type})
        summary, reply = generate_summary(client)
        logger.debug('model reply: {}'.format(reply), extra={'route': type})
        if i == retry - 1:
          errors = grammar_check(summary, type)
          if (len(errors) != 0):
            logger.error('grammar check failed, please check the model output')
            grammar_error = True
            score = 0.0
  filled_func = row['func']
  # add_param_check
  if (config['post_checkers']['add_param_checker']['enable'] == True):
    logger.debug('add param checker info:', extra={'route': type})
    filled_func = add_param_check(row['func'], config['post_checkers']['add_param_checker']['count'])
    logger.debug('original func: {}'.format(row['func']), extra={'route': type})
    logger.debug('filled func: {}'.format(filled_func), extra={'route': type})

  # swap check
  if (config['post_checkers']['swap_checker']['enable'] == True):
    logger.debug('swap checker info:', extra={'route': type})
    retry = config['post_checkers']['swap_checker']['retry_times']
    l, r, parameters = basic_checker.extract_parameters(filled_func)
    cnt = len(parameters)
    if cnt < 2:
      logger.info('not enough parameters, use add param check fill')
      filled_func = add_param_check(row['func'], 2)
      cnt = cnt + 2
      # return summary, reply, score
    if grammar_error:
      logger.info('grammar check failed, skip swap check')
      return summary, reply, score
    
    keys = random.sample(range(1, cnt + 1), 2)
    swapped_func = basic_checker.swap_parameters(filled_func, keys)
    logger.debug('original func: {}'.format(row['func']), extra={'route': type})
    logger.debug('filled func: {}'.format(filled_func), extra={'route': type})
    logger.debug('swapped func: {}'.format(swapped_func), extra={'route': type})
    swapped_summary_gt = basic_checker.swap_summary_keys(summary, keys)
    swapped_row = row.copy()
    swapped_row['func'] = swapped_func
    client.remove_all_messages()
    fill_client(client, [knowledge_prompt, examples_prompt, generator.generate_input_prompt(swapped_row)])
    swapped_summary_pd, swapped_reply = generate_summary(client)
    swapped_append_prompt = []


    comparator = None
    if type == 'taint':
      comparator = taint_comparator.TaintComparator()
    else:
      comparator = memory_comparator.MemoryComparator()
    
    for i in range(retry):
      tp, fp, fn, same = compare_summary(comparator, swapped_summary_gt, swapped_summary_pd)
      logger.debug('swap check round {}/{}'.format(i, retry), extra={'route': type})
      logger.debug('gt: {}'.format(swapped_summary_gt), extra={'route': type})
      logger.debug('pd: {}'.format(swapped_summary_pd), extra={'route': type})
      logger.debug('swap check result: tp: {}, fp: {}, fn: {}, same: {}'.format(tp, fp, fn, same), extra={'route': type})
      if same == 1:
        logger.debug('swap check pass', extra={'route': type})
        break
      
      score = max(score - 0.2, 0.01)
      # NOT SAME
      logger.debug('swap check error', extra={'route': type})
      message_str = '你与另一个模型生成的摘要不一致，请你检查后重新生成摘要\n'
      # message_str += '交换后的函数声明：' + swapped_row['func'] + '\n'
      message_str += '你生成的摘要为：' + summary + '\n'
      message_str += '另一个模型针对交换后的参数生成的摘要：' + \
        basic_checker.swap_summary_keys(swapped_summary_pd, keys) + '\n'
      append_prompt.append(message_str)
      swapped_message_str = '你与另一个模型生成的摘要不一致，请你检查后重新生成摘要\n'
      swapped_message_str += '你生成的摘要为:' + swapped_summary_pd + '\n'
      swapped_message_str += '另一个模型生成的摘要为:' + swapped_summary_gt + '\n'
      swapped_append_prompt.append(swapped_message_str)
      client.remove_all_messages()
      fill_client(client, [knowledge_prompt, examples_prompt, input_prompt, append_prompt])
      summary, reply = generate_summary(client)
      logger.debug('model reply: {}'.format(reply), extra={'route': type})
      swapped_summary_gt = basic_checker.swap_summary_keys(summary, keys)
      client.remove_all_messages()
      fill_client(client, [knowledge_prompt, examples_prompt, generator.generate_input_prompt(swapped_row), swapped_append_prompt])
      swapped_summary_pd, swapped_reply = generate_summary(client)
      if i == retry - 1:
        tp, fp, fn, same = compare_summary(comparator, swapped_summary_gt, swapped_summary_pd)
        logger.debug('swap check round {}/{}'.format(retry, retry), extra={'route': type})
        logger.debug('gt: {}'.format(swapped_summary_gt), extra={'route': type})
        logger.debug('pd: {}'.format(swapped_summary_pd), extra={'route': type})
        logger.debug('swap check result: tp: {}, fp: {}, fn: {}, same: {}'.format(tp, fp, fn, same), extra={'route': type})
        if same == 1:
          logger.debug('swap check pass', extra={'route': type})
        else:
          logger.debug('swap check not pass', extra={'route': type})
          score = max(score - 0.5, 0.01)

  return summary, reply, score

def post_check_(row, client, config, type):
  generator = prompt_generator.PromptGenerator(type, config, logger)
  # TODO config k
  # knowledge_prompt, examples_prompt, input_prompt = generator.generate_prompt(row, config['similar_example_num'])
  knowledge_prompt, examples_prompt, input_prompt, score, scores = generator.generate_prompt_with_score(row, config['similar_example_num'])
  append_prompt = []
  fill_client(client, [knowledge_prompt, examples_prompt, input_prompt])
  for index, example in enumerate(examples_prompt):
    logger.debug('example {}: {}'.format(index, example), extra={'route': type})
    logger.debug('example score: {}, after map: {}'.format(scores[index][0], scores[index][1]), extra={'route': type})
  logger.debug('input score: {}'.format(score), extra={'route': type})
  summary, reply = generate_summary(client)
  logger.debug('model reply: {}'.format(reply), extra={'route': type})
  logger.debug('grammar checker info:', extra={'route': type})
  if (config['post_checkers']['grammar_checker']['enable'] == True):
    retry = config['post_checkers']['grammar_checker']['retry_times']
    for i in range(retry):
      logger.debug('grammar check round {}/{}'.format(i, retry), extra={'route': type})
      errors = grammar_check(summary, type)
      if (len(errors) == 0):
        logger.debug('grammar check pass', extra={'route': type})
        break
      else:
        message = '以下的摘要串存在错误，请你根据错误的串和正确的文法进行修改：\n'
        for error in errors:
          message += error + '\n'
        client.add_user_message(message)
        logger.debug('grammar check error:\n {}'.format(message), extra={'route': type})
        summary, reply = generate_summary(client)
        logger.debug('model reply: {}'.format(reply), extra={'route': type})
  logger.debug('swap checker info:', extra={'route': type})
  if (config['post_checkers']['swap_checker']['enable'] == True):
    retry = config['post_checkers']['swap_checker']['retry_times']
    l, r, parameters = basic_checker.extract_parameters(row['func'])
    cnt = len(parameters)
    if cnt < 2:
      logger.info('not enough parameters, skip swap check')
      return summary, reply, score
    
    keys = random.sample(range(1, cnt + 1), 2)
    swapped_func = basic_checker.swap_parameters(row['func'], keys)
    logger.debug('original func: {}'.format(row['func']), extra={'route': type})
    logger.debug('swapped func: {}'.format(swapped_func), extra={'route': type})
    swapped_summary_gt = basic_checker.swap_summary_keys(summary, keys)
    swapped_row = row.copy()
    swapped_row['func'] = swapped_func
    client.remove_all_messages()
    fill_client(client, [knowledge_prompt, examples_prompt, generator.generate_input_prompt(swapped_row)])
    swapped_summary_pd, swapped_reply = generate_summary(client)
    swapped_append_prompt = []

    for i in range(retry):
      comparator = None
      parser = None
      if type == 'taint':
        comparator = taint_comparator.TaintComparator()
        parser = Parser(basic_checker.TAINT_LANGUAGE)
      else:
        comparator = memory_comparator.MemoryComparator()
        parser = Parser(basic_checker.MEMORY_LANGUAGE)
      gt_tree = parser.parse(bytes(swapped_summary_gt, 'utf-8'))
      pd_tree = parser.parse(bytes(swapped_summary_pd, 'utf-8'))
      tp, fp, fn, same = comparator.compare_summary(gt_tree, pd_tree)
      logger.debug('swap check round {}/{}'.format(i, retry), extra={'route': type})
      logger.debug('gt: {}'.format(swapped_summary_gt), extra={'route': type})
      logger.debug('pd: {}'.format(swapped_summary_pd), extra={'route': type})
      logger.debug('swap check result: tp: {}, fp: {}, fn: {}, same: {}'.format(tp, fp, fn, same), extra={'route': type})
      if same == 1:
        logger.debug('swap check pass', extra={'route': type})
        break
      
      # NOT SAME
      logger.debug('swap check error', extra={'route': type})
      message_str = '你与另一个模型生成的摘要不一致，请你检查后重新生成摘要\n'
      # message_str += '交换后的函数声明：' + swapped_row['func'] + '\n'
      message_str += '你生成的摘要为：' + summary + '\n'
      message_str += '另一个模型针对交换后的参数生成的摘要：' + \
        basic_checker.swap_summary_keys(swapped_summary_pd, keys) + '\n'
      append_prompt.append(message_str)
      swapped_message_str = '你与另一个模型生成的摘要不一致，请你检查后重新生成摘要\n'
      swapped_message_str += '你生成的摘要为:' + swapped_summary_pd + '\n'
      swapped_message_str += '另一个模型生成的摘要为:' + swapped_summary_gt + '\n'
      swapped_append_prompt.append(swapped_message_str)
      client.remove_all_messages()
      fill_client(client, [knowledge_prompt, examples_prompt, input_prompt, append_prompt])
      summary, reply = generate_summary(client)
      logger.debug('model reply: {}'.format(reply), extra={'route': type})
      swapped_summary_gt = basic_checker.swap_summary_keys(summary, keys)
      client.remove_all_messages()
      fill_client(client, [knowledge_prompt, examples_prompt, generator.generate_input_prompt(swapped_row), swapped_append_prompt])
      swapped_summary_pd, swapped_reply = generate_summary(client)
      
  return summary, reply, score

def parse_config(path):
  config = None
  try:
    with open(path, 'r') as f:
      config = json.load(f)
      f.close()
  except FileNotFoundError:
    logger.error(r'{} not found!'.format(path))
    exit(1)
  except Exception as e:
    logger.error('Error reading config file:', e)
    exit(1)
  return config

def compare_summary(comparator, summary_gt, summary_pd):
  tree_gt = comparator.parser.parse(bytes(summary_gt, 'utf-8'))
  tree_pd = comparator.parser.parse(bytes(summary_pd, 'utf-8'))
  return comparator.compare_summary(tree_gt, tree_pd)

def register_file_logger(taint_log, memory_log):
  taint_filter = RouteFilter('taint')
  memory_filter = RouteFilter('memory')
  taint_file_handler = logging.FileHandler(taint_log)
  taint_file_handler.addFilter(taint_filter)
  memory_file_handler = logging.FileHandler(memory_log)
  memory_file_handler.addFilter(memory_filter)
  taint_file_handler.setLevel(logging.DEBUG)
  memory_file_handler.setLevel(logging.DEBUG)
  formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
  taint_file_handler.setFormatter(formatter)
  memory_file_handler.setFormatter(formatter)
  logger.addHandler(taint_file_handler)
  logger.addHandler(memory_file_handler)

def compare_results(config, summaries, taint_compare_result, memory_compare_result, compare_file):
  logger.info('compare ground truth & model output')
  taint_com = taint_comparator.TaintComparator()
  memory_com = memory_comparator.MemoryComparator()

  ground_truth_df = None
  try:
    ground_truth_df = pd.read_csv(config['ground_truth_file'])
  except FileNotFoundError:
    logger.error(r'{} not found!'.format(config['ground_truth_file']))
    exit(1)
  except Exception as e:
    logger.error('Error reading ground truth file:', e)
    exit(1)
  taint_tp , taint_fp, taint_fn, taint_same = 0, 0, 0, 0
  memory_tp, memory_fp, memory_fn, memory_same = 0, 0, 0, 0
  if (len(ground_truth_df) != len(summaries)):
    logger.error('Length not equal')
    exit(1)
  compare_lists = []
  compare_columns = ['namespace', 'class', 'func', 
                     'taint_summary_gt', 'taint_summary_pd', 'taint_same', 'taint_score', 'taint_tp', 'taint_fp', 'taint_fn',
                     'memory_summary_gt', 'memory_summary_pd', 'memory_same', 'memory_score', 'memory_tp', 'memory_fp', 'memory_fn']
  for index, row in ground_truth_df.iterrows():
    # taint summary comparison
    compare_list = [row['namespace'], row['class'], row['func']]
    taint_summary_gt = row['taint_summary']
    taint_summary_pd = summaries.iloc[index]['taint_summary']
    tp, fp, fn, same = compare_summary(taint_com, taint_summary_gt, taint_summary_pd)
    taint_tp += tp
    taint_fp += fp
    taint_fn += fn
    taint_same += same
    compare_list.append(taint_summary_gt)
    compare_list.append(taint_summary_pd)
    compare_list.append(same)
    compare_list.append(summaries.iloc[index]['taint_score'])
    compare_list.append(tp)
    compare_list.append(fp)
    compare_list.append(fn)
    
    # memory summary comparison
    memory_summary_gt = row['memory_summary']
    memory_summary_pd = summaries.iloc[index]['memory_summary']
    tp, fp, fn, same = compare_summary(memory_com, memory_summary_gt, memory_summary_pd)
    memory_tp += tp
    memory_fp += fp
    memory_fn += fn
    memory_same += same
    compare_list.append(memory_summary_gt)
    compare_list.append(memory_summary_pd)
    compare_list.append(same)
    compare_list.append(summaries.iloc[index]['memory_score'])
    compare_list.append(tp)
    compare_list.append(fp)
    compare_list.append(fn)
    compare_lists.append(compare_list)
  compare_result = pd.DataFrame(columns=compare_columns, data=compare_lists)
  compare_result.to_csv(compare_file, encoding='utf-8-sig')
  taint_compare_result_file = open(taint_compare_result, 'w')
  taint_compare_result_file.write('tp: {}\n'.format(taint_tp))
  taint_compare_result_file.write('fp: {}\n'.format(taint_fp))
  taint_compare_result_file.write('fn: {}\n'.format(taint_fn))
  taint_compare_result_file.write('same: {}/{}\n'.format(taint_same, len(ground_truth_df)))
  taint_compare_result_file.write('precision: {}\n'.format(taint_tp / (taint_tp + taint_fp)))
  taint_compare_result_file.write('recall: {}\n'.format(taint_tp / (taint_tp + taint_fn)))
  taint_compare_result_file.write('f1: {}\n'.format(2 * taint_tp / (2 * taint_tp + taint_fp + taint_fn)))
  taint_compare_result_file.close()
  memory_compare_result_file = open(memory_compare_result, 'w')
  memory_compare_result_file.write('tp: {}\n'.format(memory_tp))
  memory_compare_result_file.write('fp: {}\n'.format(memory_fp))
  memory_compare_result_file.write('fn: {}\n'.format(memory_fn))
  memory_compare_result_file.write('same: {}/{}\n'.format(memory_same, len(ground_truth_df)))
  memory_compare_result_file.write('precision: {}\n'.format(memory_tp / (memory_tp + memory_fp)))
  memory_compare_result_file.write('recall: {}\n'.format(memory_tp / (memory_tp + memory_fn)))
  memory_compare_result_file.write('f1: {}\n'.format(2 * memory_tp / (2 * memory_tp + memory_fp + memory_fn)))
  memory_compare_result_file.close()

def expriment_drive():
  config = parse_config('./configs/config.json')
  client = chat_client.LLMClient(config['llm_config'])
  test_file = config['test_data_file']
  test_df = None
  # init output dir
  output_path = './output'
  output_folder = os.path.exists(output_path)
  if not output_folder:
    os.makedirs(output_path)
  # expriment results dir
  date_str = datetime.datetime.now().strftime('%Y_%m_%d')
  expriment_path = output_path + '/' + date_str
  entries = os.listdir(output_path)
  id = 0
  for entry in entries:
    if os.path.isdir(os.path.join(output_path, entry)):
      if entry.startswith(date_str):
        id += 1
  expriment_path = output_path + '/' + date_str + '_' + str(id)
  os.makedirs(expriment_path)
  # output file
  output_file = expriment_path + '/' + 'output.csv'
  compare_file = expriment_path + '/' + 'compare.csv'
  taint_log = expriment_path + '/' + 'taint_log.txt'
  memory_log = expriment_path + '/' + 'memory_log.txt'
  taint_compare_result = expriment_path + '/' + 'taint_compare_result.txt'
  memory_compare_result = expriment_path + '/' + 'memory_compare_result.txt'
  # init log file handler
  # taint_log_file = open(taint_log, 'w')
  # memory_log_file = open(memory_log, 'w')
  register_file_logger(taint_log, memory_log)

  summaries = []
  
  output_columns = ['namespace', 'class', 'func', 'docstring', 'taint_summary', 'taint_prompt', 'taint_score', 'taint_model_output',
             'memory_summary', 'memory_prompt', 'memory_score', 'memory_model_output']
  
  output_lists = []
  try:
    test_df = pd.read_csv(test_file)
  except FileNotFoundError:
    logger.error(r'{} not found!'.format(test_file))
    exit(1)
  except Exception as e:
    logger.error('Error reading test data file:', e)
    exit(1)
  total_len = len(test_df)
  taint_generator = prompt_generator.PromptGenerator('taint', config, logger)
  memory_generator = prompt_generator.PromptGenerator('memory', config, logger)
  for index, row in test_df.iterrows():
    # taint summary generation & post_check
    logger.info(f'processing {index}/{total_len} row')
    # TODO update prompt
    taint_summary, taint_reply, taint_score = post_check_score(row, client, config, 'taint',taint_generator)
    client.remove_all_messages()
    # memory summary generation & post_check
    memory_summary, memory_reply, memory_score = post_check_score(row, client, config, 'memory', memory_generator)
    client.remove_all_messages()
    output_lists.append([row['namespace'], row['class'], row['func'], row['docstring'], 
                         taint_summary, '', taint_score, f'"{taint_reply}"', 
                         memory_summary, '', memory_score, f'"{memory_reply}"'])

  summaries = pd.DataFrame(columns=output_columns, data=output_lists)
  summaries.to_csv(output_file, encoding='utf-8-sig')

  # compare ground truth & model output
  compare_results(config, summaries, taint_compare_result, memory_compare_result, compare_file)

if __name__ == '__main__':
  expriment_drive()