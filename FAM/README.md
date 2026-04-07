# FuncAutoModeler

![Language](https://img.shields.io/badge/language-python-brightgreen)

A tool for automatic function modeling based on large models.

## Prerequisites

- OS: Ubuntu-22.04 LTS 
- node.js v23.10.0
- npm 10.9.2
- tree-sitter-cli =0.24.0
  - Install Command ```npm install -g tree-sitter-cli@0.24.0```  
- python >= 3.12 (Recommended conda environment)
- python lib
  - tree-sitter =0.24.0
  - jieba
  - numpy
  - pandas
  - openai
  - httpx
  - rank-bm25
  - sentence-transformers


## Installation

### parser install

```bash
# Assume we are in the project root directory
pip install ./parser/tree-sitter-memory
pip install ./parser/tree-sitter-taint
```

## Usage

### config file

Write config.json as follows :

```json
{
  "ground_truth_file" : "config/list_v2.csv",
  "rag_database_file" : "config/vector_v1.csv",
  "test_data_file" : "config/list_v2.csv",
  "taint_grammar_file" : "config/taint_grammar.txt",
  "taint_knowledge_file" : "config/taint_knowledge.txt",
  "memory_grammar_file" : "config/memory_grammar.txt",
  "memory_knowledge_file" : "config/memory_knowledge.txt",

  "llm_config" : {
    "api_key": "...",
    "base_url": "...",
    "model": "deepseek-r1",
    "base_prompt": "你是一位精通C++编程语言、静态分析和领域特定语言（DSL）设计的软件架构专家，擅长将c++库函数源码或文档转换为DSL结构的摘要。请根据我提供的DSL结构定义，严格遵循以下要求生成C++库函数摘要。"
  },
  "post_checkers": {
    "grammar_checker" : {
      "enable" : true,
      "retry_times" : 3
    },
    "swap_checker" : {
      "enable" : true,
      "retry_times" : 2
    }
  }
}
```

### expriment results

When the program is running, a directory named year_month_day_number will be generated under output. The program running results are stored in the directory.

- compare.csv  
  - Storing the ground truth and model predictions in a tabular format for easy manual inspection.
- memory_compare_result.txt  
  - Storing precision, recall and same of memory summaries.
- memory_log.txt
  - Storing information when the model generates the memory summary (similar examples provided to the model, output of postchecker)
- output.csv
  - Storing prompts, model outputs, generated summaries.
- taint_compare_result.txt
  - Storing precision, recall and same of taint summaries.
- taint_log.txt
  - Storing information when the model generates the taint summary (similar examples provided to the model, output of postchecker)

### others

We have provided other files in the examples directory. You can refer to these files for specific formats and contents.

- xx_grammar.txt
  - Grammar definition in BNF/EBNF form (for LLM)
- xx_knowledge.txt
  - The semantics of the summary and the relevant domain knowledge

## Test

After Configuration:

``` python3 main.py ```

## Write Summary

### parameter encode

- <-1> 返回值
- <0> this 指针(只对成员函数有效)
- <1> 第一个参数

以此类推

### taint summary

#### instructions

- setSink(\<x>)
  - 将参数\<x>标记为sink点。sink点定义：可能会导致内存分配溢出或者内存访问溢出的整形参数，一般是内存访问的下标，内存分配的大小。
  - 对于char \*这种数组来说，访问长度和访问下标认为是sink点
  - 对于容器来说访问下标认为是sink点，访问长度不认为是sink点
- transitive(\<x>, \<y>)
  - 将\<y>的污点信息覆盖到\<x>的污点信息上。 针对赋值之类的语义使用。
- sanitize(\<x>)
  - 将\<x>的污点信息清除。
  - 一般在析构函数、删除元素之类的操作中使用。
- swapTaint(\<x>, \<y>)
  - 交换\<x>和\<y>的污点信息。 只针对swap函数的语义使用。

#### model

- 容器模型
  1. 插入元素: 采用保守策略，将原容器的污点信息与插入元素的污点信息合并，使用transitive(\<x>, \<x> || \<y>)的方式进行污点传播
  2. resize操作: 采用保守策略，按照插入操作进行建模
  3. 删除元素: 采用保守策略，保留原容器的污点信息
  4. 析构容器: 清空原容器的污点信息，即sanitize(\<x>)
- 返回值模型
  1. 传播污点的返回值：
      - 直接返回自身的成员变量(内存分配器、最大可分配内存等特殊情况除外)
      - 返回容器修改元素的个数（比如删除了多少个元素）
  2. 其余情况则认为不传播。

### memory summary

#### instructions

- assign: \<x> = \<y>
  - 类似c语言的赋值操作
  - 至多支持一个 ‘||’ 运算符
  - 尽可能精确建模
  - 当表达式右侧需要3个及以上的运算符才能精确表达式，尝试用范围进行模糊建模
  - 如果也无法用范围模糊建模，则放弃建模
- malloc(\<x>, may)
  - 标记可能对\<x>申请一块内存
  - 只在明确使用new或者malloc指令申请堆内存时使用，容器有自己的堆内存管理，不建模
- free(\<x>, must)
  - 标记一定会释放<x>所申请的内存
  - 只在明确使用delete或者free指令释放堆内存时使用，容器有自己的堆内存管理，不建模
- overflow(\<x>, \[\<y>, \<z>\])
  - 用于进行迭代器或者下标访问时，检测是否溢出。将\<x>限制在\[\<y>, \<z>\]的区间范围内，超出该范围则认为会发生溢出。
  - 如果函数中明确提及具有边界检查类似的功能，则不需要使用该指令。
- expansion(\<x>)
  - 将\<x>标记为可能发生扩容。只针对连续存储容器可能发生扩容情况时，如vector。
- swap(\<x>, \<y>)
  - 交换\<x>和\<y>内存信息， 只针对\<x>这种参数，不能对\<x>.field使用。 只针对swap函数的语义使用。

#### model

- 抽象属性模型
  - strlen 表示字符串的有效长度，只有类string类型，如string\stringView\char* 可以使用该属性
  - size 表示容器的大小，数组的大小等,这是一种抽象。 对于字符串来说，表示实际字符数，即有效字符串+空字符，对于string这种容器一般认为是strlen + 1，对于char *这种则代表实际数组长度。
  - begin 表示起始迭代器, 专用于容器
  - end 表示结尾迭代器，专用于容器
  - field: 对数组、指针以及类或结构体成员的抽象建模
    - 对于数组而言， int a\[\]， a.field 可以指代a\[0\]，也就是只保留一个元素的索引敏感
    - 对于指针而言， int \*a, a.field 可以指代\*a， 也就是a指向的内存区域
    - 对于对象而言， class a, a.field 可以指代a所有的域
- 常量模型
  - MAX 表示最大值
  - MIN 表示最小值
  - NULL 对于指针而言表示空指针，对于其他类型表示类似析构或者删除的语义
  - NPOS 表示c++标准中npos的语义
- 运算模型
  - 如果表达式可以用3个及以内的key表达，则可以进行精确建模
  - 如果无法精确建模，则尝试构建出其最小值和最大值，用范围进行粗略建模，但要注意边界值是否可以取到
- 容器模型
  - 由于DSL内存建模设计，容器中至多保管一个元素的具体值
  - 添加元素: \<x>.field = \<y>
  - 以first, last这种迭代器形式添加元素： 只保留first指向的元素，即\<x>.field = \<first>.field
  - resize操作: 针对大小精确建模，针对field按照添加元素进行建模
  - 对于复杂的操作，可以拆分成简单的操作分别建模
  - 删除单个元素: \<x>.field = NULL    \<x>.size = \<x>.size - 1
  - 删除迭代器序列(难以确定具体个数): \<x>.field = NULL    \<x>.size = [0, \<x>.size]
  - 析构容器: \<x> = NULL
- 字符串模型
  - 如果对strlen进行了赋值，那么一般size可以赋值为strlen + 1
- 返回值模型
  - 如果为函数声明void，则不需要为返回值赋值
  - 如果存在多条路径，将其合并为一条路径，副作用进行合并，在返回值中可以用||的形式表达不同路径的返回值(但||只能使用一次)

## Docstring

### 函数概括
- 用一句话高度概括函数的功能。

### 参数描述

对于每一个参数，docstring 都应该清晰地说明：

- 名称和类型: 参数的变量名及其数据类型 (例如 char*, int, struct MyData*)。
- 作用：描述该参数的用途
- 取值范围
- 是否为汇点(sink点)


### 返回值描述

- 类型和含义: 返回值的数据类型及其代表的意义
- 取值：具体取值或者取值范围或者特殊值

### 函数详细说明

- 更深入地解释函数的工作原理、算法和目的，例如：
  - 是否执行内存分配 (malloc) 或释放 (free)
  - 溢出风险 (overflow): 是否存在整数或缓冲区溢出的可能性？在什么条件下会溢出？
  - 是否会扩容
  - 赋值操作
  - 谓词的类型转换要求
  - 容器元素是否可修改（特别在谓词中）
  - 元素类型是否有必须满足的运算符重载要求（例如可比较）
  - 函数参数是否允许类型转换（如 string 支持 string_view）
