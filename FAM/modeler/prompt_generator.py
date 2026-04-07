import pandas as pd
from modeler import retriever

class PromptGenerator:
  def __init__(self, type, config, logger):
    self.type = type
    self.grammar = self.read_file(config[type + '_grammar_file'])
    self.domain_knowledge = self.read_file(config[type + '_knowledge_file'])
    self.retriever = retriever.Retriever(config['rag_database_file'])
    self.retriever.load_transformer()
    self.fixed_list = pd.DataFrame()
    self.logger = logger
    # fill fixed database examples
    if config['fixed_rag_file'] is None or config['fixed_rag_file'] == '':  
      return
    try:
      self.fixed_list = pd.read_csv(config['fixed_rag_file'])
    except FileNotFoundError:
      print(r'{} not found!'.format(config['fixed_rag_file']))
      return
    except Exception as e:
      print('Error reading file:', e)
      return

  def read_file(self, path):
    try:
      f = open(path)
      lines = f.readlines()
      f.close()
      res = ""
      for line in lines:
        res = res + line
      return res
    except FileNotFoundError:
      print(r'{} not found!'.format(path))
      exit(1)
    except Exception as e:
      print('Error reading file:', e)
      exit(1)
  
  
  def set_input(self, row):
    self.input = row

  def dump_prompt(self, k = 10):
    prompt = [self.grammar, self.domain_knowledge]
    prompt.append('以上为我们的DSL文法结构和相关的领域知识，下面我将提供一些摘要例子，你可以从中学习，请你严格按照领域知识和例子直接输出最终生成的摘要即可')
    self.examples = self.retriever.retrieve(self.input, k)
    for example in self.examples:
      serilize_exp = 'Q: 我将提供给你API的声明、docstring和源码，需要你按照我给你提供的格式生成摘要。首先，你要判断该函数是否为成员函数，如果不为成员函数则不需要使用<0>, 其次，将函数声明中的参数按顺序进行编码<1><2>..., 函数声明中的参数顺序可能和docstring文档中的参数顺序是不一致的，你要按照函数声明中的参数顺序来编码！并且不要在摘要中添加注释！ 最后，根据领域知识进行分析，直接输出最终摘要即可，无需思考过程'
      if not pd.isna(example['namespace']):
        serilize_exp += '命名空间: ' + example['namespace'] + '\n'
      if not pd.isna(example['class']):
        serilize_exp += '是' + example['class'] + '的成员函数\n'
      else:
        serilize_exp += '不是成员函数\n'
      serilize_exp += '函数声明：' + example['func'] + '\n'
      serilize_exp += '函数docstring: ' + example['docstring']
      # if 'code' in example:
      #   serilize_exp += example['code']
      serilize_exp += 'A: ```final_summary'
      serilize_exp += example[self.type + '_summary']
      serilize_exp += '```'
      prompt.append(serilize_exp)

    input_prompt = 'Q: 我将提供给你API的声明、docstring和源码，需要你按照我给你提供的格式生成摘要。首先，你要判断该函数是否为成员函数，如果不为成员函数则不需要使用<0>, 其次，将函数声明中的参数按顺序进行编码<1><2>..., 函数声明中的参数顺序可能和docstring文档中的参数顺序是不一致的，你要按照函数声明中的参数顺序来编码，并输出参数、返回值和编码的对应关系，最后，根据领域知识进行分析，直接输出最终摘要即可，无需思考过程'
    input_prompt += '命名空间：' + self.input['namespace'] + '\n'
    if pd.isna(self.input['class']):
      input_prompt += '不是成员函数\n'
    else:
      input_prompt += '是' + self.input['class'] + '的成员函数\n'
    input_prompt += '函数声明：' + self.input['func'] + '\n'
    input_prompt += '函数docstring:' + self.input['docstring'] + '\n'
    prompt.append(input_prompt)
    return prompt

  def generate_input_prompt(self, row):
    input_prompt = 'Q: 我将提供给你API的声明、docstring和源码，需要你按照我给你提供的格式生成摘要。首先，你要判断该函数是否为成员函数，如果不为成员函数则不需要使用<0>, 其次，将函数声明中的参数按顺序进行编码<1><2>..., 函数声明中的参数顺序可能和docstring文档中的参数顺序是不一致的，你要按照函数声明中的参数顺序来编码，并输出参数、返回值和编码的对应关系，最后，根据领域知识进行分析，生成摘要，然后按照提供给你的领域知识对生成的摘要进行复盘，没有问题后直接输出最终摘要即可'
    input_prompt += '命名空间：' + row['namespace'] + '\n'
    if pd.isna(row['class']):
      input_prompt += '不是成员函数\n'
    else:
      input_prompt += '是' + row['class'] + '的成员函数\n'
    input_prompt += '函数声明：' + row['func'] + '\n'
    input_prompt += '函数docstring:' + row['docstring'] + '\n'
    return [input_prompt]

  def generate_prompt(self, row, k):
    knowledge_query = '以上为我们的DSL文法结构和相关的领域知识，下面我将提供一些摘要例子，你可以从中学习'
    knowledge_prompt = [self.grammar, self.domain_knowledge, knowledge_query]
    examples_prompt = []
    for index, example in self.fixed_list.iterrows():
      serilize_exp = 'Q: 我将提供给你API的声明、docstring和源码，需要你按照我给你提供的格式生成摘要。首先，你要判断该函数是否为成员函数，如果不为成员函数则不需要使用<0>, 其次，将函数声明中的参数按顺序进行编码<1><2>..., 函数声明中的参数顺序可能和docstring文档中的参数顺序是不一致的，你要按照函数声明中的参数顺序来编码！ 最后，根据领域知识进行分析，直接输出最终摘要即可，无需思考过程'
      if not pd.isna(example['namespace']):
        serilize_exp += '命名空间: ' + example['namespace'] + '\n'
      if not pd.isna(example['class']):
        serilize_exp += '是' + example['class'] + '的成员函数\n'
      else:
        serilize_exp += '不是成员函数\n'
      serilize_exp += '函数声明：' + example['func'] + '\n'
      serilize_exp += '函数docstring: ' + example['docstring']
      # if 'code' in example:
      #   serilize_exp += example['code']
      serilize_exp += 'A: ```final_summary'
      serilize_exp += example[self.type + '_summary']
      serilize_exp += '```'
      examples_prompt.append(serilize_exp)
    
    examples = self.retriever.retrieve(row, k)
    for example in examples:
      serilize_exp = 'Q: 我将提供给你API的声明、docstring和源码，需要你按照我给你提供的格式生成摘要。首先，你要判断该函数是否为成员函数，如果不为成员函数则不需要使用<0>, 其次，将函数声明中的参数按顺序进行编码<1><2>..., 函数声明中的参数顺序可能和docstring文档中的参数顺序是不一致的，你要按照函数声明中的参数顺序来编码！ 最后，根据领域知识进行分析，直接输出最终摘要即可，无需思考过程'
      if not pd.isna(example['namespace']):
        serilize_exp += '命名空间: ' + example['namespace'] + '\n'
      if not pd.isna(example['class']):
        serilize_exp += '是' + example['class'] + '的成员函数\n'
      else:
        serilize_exp += '不是成员函数\n'
      serilize_exp += '函数声明：' + example['func'] + '\n'
      serilize_exp += '函数docstring: ' + example['docstring']
      # if 'code' in example:
      #   serilize_exp += example['code']
      serilize_exp += 'A: ```final_summary'
      serilize_exp += example[self.type + '_summary']
      serilize_exp += '```'
      examples_prompt.append(serilize_exp)
    
    return knowledge_prompt, examples_prompt, self.generate_input_prompt(row)

  def map_similarity_score(self, cos_sim):
    # 先简单使用线性映射，后面再尝试改为幂函数映射
    # score = 0.0
    # f = lambda x, x1, x2, y1, y2: (x - x1) * ((y2 - y1) / (x2 - x1)) + y1
    # if cos_sim >= 0.7:
    #   score = f(cos_sim, 0.7, 1.0, 0.9, 1.0)
    # elif cos_sim >= 0.5:
    #   score = f(cos_sim, 0.5, 0.7, 0.7, 0.9)
    # return score
    return cos_sim

  def generate_prompt_with_score(self, row, k):
    knowledge_query = '以上为我们的DSL文法结构和相关的领域知识，下面我将提供一些摘要例子，你可以从中学习'
    knowledge_prompt = [self.grammar, self.domain_knowledge, knowledge_query]
    examples_prompt = []
    for index, example in self.fixed_list.iterrows():
      serilize_exp = 'Q: 我将提供给你API的声明、docstring和源码，需要你按照我给你提供的格式生成摘要.'
      if not pd.isna(example['namespace']):
        serilize_exp += '命名空间: ' + example['namespace'] + '\n'
      if not pd.isna(example['class']):
        serilize_exp += '是' + example['class'] + '的成员函数\n'
      else:
        serilize_exp += '不是成员函数\n'
      serilize_exp += '函数声明：' + example['func'] + '\n'
      serilize_exp += '函数docstring: ' + example['docstring']
      # if 'code' in example:
      #   serilize_exp += example['code']
      serilize_exp += 'A: ```final_summary'
      serilize_exp += example[self.type + '_summary']
      serilize_exp += '```'
      examples_prompt.append(serilize_exp)
    
    query_embedding = self.retriever.get_embedding(self.retriever.build_a_doc(row))
    examples = self.retriever.retrieve(row, 60)
    embeddings = []
    cos_sims = []
    for example in examples:
      embedding = self.retriever.get_embedding(self.retriever.build_a_doc(example))
      embeddings.append(embedding)
      cos_sim = self.retriever.get_cos_sim(query_embedding, embedding)
      cos_sims.append(cos_sim)
    # sort examples by cos_sim
    # cos_sim 高于0.7认为强相关
    # cos_sim 高于0.55认为相关
    cos_sim_threshold = 0.0
    cos_sim_strong_threshold = 0.7
    examples_dict = {}
    examples_map = {}
    for i in range(len(examples)):
      t_e = tuple(examples[i])
      examples_map[t_e] = examples[i]
      examples_dict[t_e] = cos_sims[i]
    sorted_examples_dict = sorted(examples_dict.items(), key=lambda x: x[1], reverse=True)
    count = 0
    score_examples = []
    p = 0
    scores = []
    
    for example, cos_sim in sorted_examples_dict:
      example = examples_map[example]
      if count >= k or cos_sim < cos_sim_threshold:
        break
      count += 1
      score_examples.append(self.map_similarity_score(cos_sim))
      serilize_exp = 'Q: 我将提供给你API的声明、docstring和源码，需要你按照我给你提供的格式生成摘要。首先，你要判断该函数是否为成员函数，如果不为成员函数则不需要使用<0>, 其次，将函数声明中的参数按顺序进行编码<1><2>..., 函数声明中的参数顺序可能和docstring文档中的参数顺序是不一致的，你要按照函数声明中的参数顺序来编码！ 最后，根据领域知识进行分析,直接输出最终摘要即可，无需思考过程'
      if not pd.isna(example['namespace']):
        serilize_exp += '命名空间: ' + example['namespace'] + '\n'
      if not pd.isna(example['class']):
        serilize_exp += '是' + example['class'] + '的成员函数\n'
      else:
        serilize_exp += '不是成员函数\n'
      serilize_exp += '函数声明：' + example['func'] + '\n'
      serilize_exp += '函数docstring: ' + example['docstring']
      # if 'code' in example:
      #   serilize_exp += example['code']
      serilize_exp += 'A: ```final_summary'
      serilize_exp += example[self.type + '_summary']
      serilize_exp += '```'
      examples_prompt.insert(0, serilize_exp)
      scores.insert(0, (cos_sim, self.map_similarity_score(cos_sim)))
    if count == 0:
      p = 1.0
    w = sum(score_examples)
    s_base = 0.1
    s_e = 0
    for i in range(len(score_examples)):
      s_e += score_examples[i] * score_examples[i] / w
    self.logger.debug(f'count: {count}, p: {p}, w: {w}, s_base: {s_base}, s_e: {s_e}', extra = {'route' : self.type})
    score = s_base + (1 - s_base) * (max(s_e - p, 0.0))
    
    return knowledge_prompt, examples_prompt, self.generate_input_prompt(row), score, scores