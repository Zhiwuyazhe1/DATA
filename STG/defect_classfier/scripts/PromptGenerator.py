import re
import os
import json
import logging

logger = logging.getLogger(__name__)
import feature_mapping as fm
FEATURE_EXPL_FILE = os.path.join(os.path.dirname(__file__), '特征说明_local.xlsx')
# 不需要被混淆的函数名
system_function = ['size', 'sizeof', 'malloc', 'alloc', 'calloc', 'memcpy', 'memmove', 'memset', 'memcmp',
                   'strlen', 'strcpy', 'strncpy', 'strcat', 'strncat', 'strcmp', 'strncmp', 'strchr', 'strrchr',
                   'strstr', 'strtok',
                   'printf', 'scanf', 'puts', 'gets', 'fprintf', 'fscanf',
                   'fopen', 'fclose', 'fread', 'fwrite', 'fseek', 'ftell', 'rewind',
                   'sqrt', 'pow', 'sin', 'cos', 'tan', 'log', 'log10', 'ceil', 'floor', 'fabs',
                   'free', 'join', 'thread']

PROMPT_BASE = '''
你是一位资深的静态代码分析专家，对静态分析工具底层原理十分熟悉，有能力判断静态分析工具出现失效的底层原因。
你的任务是根据提供的代码片段、静态分析特征和工具诊断报告，分析缺陷归类以及如果这个缺陷被静态分析报告出现失效（误报或漏报），可能是静态分析工具存在什么底层原因导致的。
输出仅返回底层原因标签，按照JSON格式返回，遵循如下格式{"type_label":"xxx","cause_labels":["xxx","xxx","xxx"]}。
'''

PROMPT_LABELS = '''
### 失效原因分类体系含义说明
1. 前端解析与建模：
   - 常量识别和传播失效：未识别字面量/简单常量表达式，导致规则匹配失败
   - 语法/宏解析错误：宏/扩展语法导致中间表示偏离原始语义
   - 外部库/API逻辑建模缺失：对第三方库 / 系统库的函数摘要缺失，导致数据流在调用点断裂
   - 隐式控制流构建不全：调用图不全，遗漏回调/函数指针等非显式路径
2. 路径敏感与符号推理：
   - 路径可行性判定失效：对简单条件的路径可达性判断错误。
   - 符号表达式求解失效：因表达式过于复杂，无法判定条件真假或计算可能值域。
   - 循环次数/递归深度受限：循环展开或递归分析深度不足，无法检测需要多次循环才可达的缺陷
3. 堆与指针分析：
   - 指针分析能力不足：无法精确推导指针的指向关系与别名关系，导致指针赋值、解引用、跳转等操作的控制流与数据流分析失真。
   - 堆对象抽象过粗：堆对象抽象过粗：将同一分配点 / 同类型的多个堆对象实例映射为单个抽象节点，无法区分不同堆对象实例的状态变化（如指针指向的不同堆内存块）
   - 域敏感度缺失：域敏感度缺失：仅按顶层对象抽象，无法区分同一对象的不同成员 / 下标（如结构体字段、数组下标），导致字段读写、下标操作语义推断错误
4. 数据流与类型语义：
   - 流敏感度不足：忽略语句顺序，无法追踪非连续变量状态
   - 上下文敏感度不足：跨函数数据流交叉污染，产生虚假依赖
   - 基础类型建模不准确：类型位宽/隐式提升/运算域建模偏差，算术/边界误判
   - 函数指针类型建模失效：函数指针的类型匹配、签名校验、调用目标推导建模错误
   - 过程间数据流断裂：函数调用或全局变量导致数据流异常/丢失
5. 并发与同步分析：
   - 数据竞争检测失效：未识别无同步的并发读写
   - 同步原语建模缺失：锁/原子操作语义理解不足，误判线程时序约束
   - 线程交错爆炸：线程调度搜索覆盖不足，漏报深层并发轨迹
'''

PROMPT_TASK = '''
### 任务说明
请遵循以下思维链分步推理：
1. 分析代码：结合输入特征和trace，理解代码逻辑、数据流、控制流等静态分析特征
2. 推断缺陷类型：从预定义的6类基础缺陷类型中选出最匹配的一项（内存安全、逻辑错误、资源泄漏、并发安全、类型安全、安全漏洞）。
3. 推理失效原因：假设这是一个误报，或静态分析工具漏报了这个缺陷，请从静态分析工具的工作原理出发，从失效原因分类体系中选择1-3个最相关的底层原因标签。
4. 输出：仅返回底层原因标签，按照JSON格式返回，遵循如下c格式{"type_label":"xxx","cause_labels":["xxx","xxx","xxx"]}。
'''

PROMPT_SHOTS = '''
### 示例1
#输入 ##features
['存在 2个if结构', '最深嵌套深度为2', '存在 3个内存分配', '存在普通赋值', '存在声明同时初始化', '存在条件内赋值', '使用强制类型转换', '使用匿名自定义类型', '使用typedef struct{} （typedef匿名）', '使用联合体', '在堆上分配内存转换成某类型对象超过一个(指针）', '相关函数内，对【与告警点具有依赖关系的变量】赋值超过两次', '缺陷路径经过条件分支内部']
## trace
------------------------------------------------------------
   2              | extern void invalid_memory_access_003 (){
   3              | invalid_memory_access_003_uni_001 *u = (invalid_memory_access_003_uni_001 *)malloc(5 * sizeof(invalid_memory_access_003_uni_001));
   4              | invalid_memory_access_003_uni_001 *p = ((void *)0);
   5 [TRACE-1]    | if (u != ((void *)0)) {
   6 [TRACE-2]    | u->s1 = (invalid_memory_access_003_s_001 *)malloc(sizeof(invalid_memory_access_003_s_001));
   7 [TRACE-3]    | if (u->s1 != ((void *)0))
   8              | u->s1->a = (int *)malloc(5 * sizeof(int));
   9              | p = u;
  10 [TRACE-4]    | p->s1->a[0] = 1;
  11              | }
  12              | }
------------------------------------------------------------
## bug_type
Dereference of null pointer
## bug_description
Access to field 'a' results in a dereference of a null pointer (loaded from field 's1')
# 输出：{'type_label': '内存安全', 'cause_labels': ['指针分析能力不足', '域敏感度缺失', '流敏感度不足']}

### 示例2
#输入： ##features
['最深嵌套深度为1', '存在 1个内存分配', '使用了二级指针', '多次调用同一函数（带参数）']
## trace
------------------------------------------------------------
   1              | #include "header_func.h"
   2 [TRACE-2]    | extern void memory_leak_001_func_001 (int len, char **stringPtr){
   3 [TRACE-3]    | char *p = malloc(sizeof(char) * (len + 1));
   4              | *stringPtr = p;
   5              | }
   6              | extern void memory_leak_001 (){
   7              | char *str = "This is a string";
   8              | char *str1;
   9 [TRACE-1,4]  | memory_leak_001_func_001(strlen(str), &str1);
  10              | strcpy(str1, str);
  11 [TRACE-5]    | }
------------------------------------------------------------
## bug_type
Memory leak
## bug_description
Potential leak of memory pointed to by 'str1'
#输出：{'type_label': '内存安全', 'cause_labels': ['过程间数据流断裂', '指针分析能力不足']}

'''


SYSTEM_PROMPT = PROMPT_BASE.strip() + "\n" + PROMPT_LABELS.strip() + "\n" + PROMPT_TASK.strip() + "\n" + PROMPT_SHOTS.strip()

PROMPT_END = '''
# 注意
    务必严格按照标签取值范围给出回复，使用json格式。输出"type_label"和"cause_labels"两个字段。如有用例无法匹配至任何标签，请返回空列表。请认真阅读并理解不同标签的区别。
'''

class PromptGenerator:
    # 特徵映射 + 原始特徵 + 缺陷類型
    def __init__(self, input_dir):
        """
            input_dir: json文件目录
        """
        self.input_dir = input_dir
        self.file_list = set()  # 文件列表
        self.feature_mapping = {}  # 文件名 - 特徵（準備輸入給llm）
        self.type_mapping = {}  # 文件名 - 缺陷類型（準備輸入給llm）
        self.trace_mapping = {}  # 文件名 - trace（準備輸入給llm）
        self.codes_mapping = {}  # 文件名 - 代码片段（准备输入给llm）
        self.description_mapping = {} # 文件名 - 缺陷描述（准备输入给llm）
        pass

    # 未使用
    def trace_replace(self, filename):
        """
        对 trace 进行函数名混淆处理
        """
        
        trace_str = self.trace_mapping[filename].replace('\\n', '\n\t').replace('\\t', '\t')
        # 混淆函数名
        function_count = 0
        function_mapping = {}

        def replace_function(match):
            nonlocal function_count
            function_name = match.group(1)
            if function_name in system_function:  # 系统函数不修改
                return function_name
            if function_name in function_mapping:
                # 如果函数名已经存在于映射中，直接返回之前的混淆结果
                return function_mapping[function_name]
            else:
                function_count += 1
                # 生成新的混淆结果
                obfuscated_name = f"Function{function_count}"
                # 将函数名和混淆结果添加到映射中
                function_mapping[function_name] = obfuscated_name
                return obfuscated_name

        # 扩展正则匹配，处理 每个路径点所在的函数 和 stmt 中的函数调用 
        ''' 正则匹配对于C++问题较大，包括构造器和函数作为参数这样的情况， 可能需要在sseq混淆'''
        # trace_str = re.sub(r'(\w+)\(', lambda m: f"{replace_function(m)}(", trace_str)
        # trace_str = re.sub(r' in ([^ \t\n]+)', lambda m: f" in {replace_function(m)}", trace_str)
        trace_str = trace_str.replace('isInBranch:0 ','').replace('isInBranch:1','在分支中')
        trace_str = trace_str.replace('isInBranch:2','在一定不执行的分支').replace('isInBranch:3','在一定执行的分支')
        trace_str = trace_str.replace('isInLoop:0 ','').replace('isInLoop:1','在循环中')
        trace_str = trace_str.replace('isInMacro:0 ','').replace('isInMacro:1','在宏中')
        trace_str = trace_str.replace('isFPCall:0','').replace('isFPCall:1','使用函数指针')
        trace_str = trace_str.replace('isWarning:0','').replace('isWarning:1','缺陷发生位置')

        def replace_octal_with_chinese(original_str):
            # 正则匹配八进制转义序列（\ followed by 3 digits）
            def octal_replace(match):
                # 提取八进制数字部分（去掉前面的\）
                octal_code = match.group(1)
                # 转换为字节，再解码为UTF-8字符
                try:
                    return bytes([int(octal_code, 8)]).decode('utf-8')
                except:
                    # 转换失败则保留原始序列
                    return match.group(0)
        
            # 替换所有八进制转义序列
            return re.sub(r'\\(\d{3})', octal_replace, original_str)
    
        return replace_octal_with_chinese(trace_str)

    def feature_convertor(self):
        """
        调用 feature_to_expl_list 方法，将传入的 JSON 文件转换为特征解释列表。

        存入 self.feature_mapping、type_mapping
        """
        fm.read_xlsx(FEATURE_EXPL_FILE)
        # 遍历指定目录及其子目录下的所有文件和文件夹
        for root, dirs, files in os.walk(self.input_dir):
            for file in files:
                # 只处理以 .json 结尾的文件
                if file.endswith('.json'):
                    json_path = os.path.join(root, file)
                    try:
                        defect_type, feature_list, trace, codes, description = fm.feature_to_expl_list(json_path)
                        self.feature_mapping[json_path] = feature_list
                        self.description_mapping[json_path] = description
                        self.type_mapping[json_path] = defect_type
                        self.trace_mapping[json_path] = trace
                        self.codes_mapping[json_path] = codes
                        self.file_list.add(json_path)
                        # print(f"Processed {json_path}: "
                        #       f"defect_type={defect_type}, feature_list={feature_list}")
                        if feature_list is None:
                            logger.warning(f"无法从文件 {json_path} 中提取特征列表")
                    except Exception as e:
                        logger.error(f"处理 JSON 文件 {json_path} 时发生错误: {e}")

    def generate_prompt(self, filename):
        """
        根据 feature_mapping 和 type_mapping 生成最终的 prompt。
        """
        
        logger.info(f'Generate {filename} \'s prompt.')
        if filename not in self.file_list:
            logger.warning(f"WARNING: 文件 {filename} 未生成特征")
            return ""
        defect_type = self.type_mapping.get(filename, '未知类型')
        description = self.description_mapping.get(filename, '无描述')
        # 构建 prompt
        PROMPT_FEATURES = str(self.feature_mapping.get(filename, []))
        PROMPT_CODES = self.codes_mapping.get(filename, '')

        # 弃用，改为codes存储的代码片段
        # PROMPT_TRACE = self.trace_replace(filename) # 获取混淆后的 trace
        # prompt  = PROMPT_BASE + PROMPT_FEATURES + "\n# trace\n" + PROMPT_TRACE +  PROMPT_CODES + "\n# 缺陷类型\n" + defect_type + "\n" + PROMPT_END

        prompt  = "\n#输入 ##features\n" + PROMPT_FEATURES + "\n## trace\n" + PROMPT_CODES + "\n## bug_type\n" + defect_type + "\n## bug_description\n"+ description + "\n" + PROMPT_END
        return prompt

    def get_type_from_file(self, file_path):
        """
        获取文件对应的缺陷类型
        """
        return self.type_mapping.get(file_path, "")
    def parse_mapping_file(self, mapping_path):
        """
        解析格式为 [A][B]->[C] 的文档，构建嵌套字典映射

        参数:
            mapping_path (str): 文档路径

        返回:
            dict: 嵌套字典 filename_mapping，结构为 filename_mapping[A][B] = C

        example: [0-14-..._True_False.txt][#report-14]->[/home/.../json_output/report-dXa6v7_2.json]

        """
        filename_mapping = {}

        try:
            with open(mapping_path, 'r', encoding='utf-8') as file:
                for line_num, line in enumerate(file, 1):
                    line = line.strip()
                    if not line:  # 跳过空行
                        continue

                    # 解析行格式 [A][B]->[C]
                    if ']->[' not in line or not line.startswith('[') or not line.endswith(']'):
                        logger.warning(f"警告: 第 {line_num} 行格式不正确: {line}")
                        continue

                    # 分割左右两部分
                    left_part, right_part = line.split(']->[', 1)
                    right_part = right_part[:-1]  # 移除右半部分末尾的 ']'

                    # 确保左半部分包含两个部分 [A][B]
                    if left_part.count('][') != 1 or not left_part.startswith('['):
                        logger.warning(f"警告: 第 {line_num} 行左半部分格式不正确: {left_part}")
                        continue

                    a_part = left_part[1:left_part.index('][')]  # 提取 A
                    b_part = left_part[left_part.index('][') + 2:]  # 提取 B

                    # 更新嵌套字典
                    if a_part not in filename_mapping:
                        filename_mapping[a_part] = {}
                    filename_mapping[a_part][b_part] = right_part

        except FileNotFoundError:
            logger.error(f"错误: 文件 {mapping_path} 不存在")
            return {}
        except Exception as e:
            logger.error(f"错误: 处理文件时发生异常: {e}")
            return {}

        return filename_mapping


if __name__ == "__main__":
    defect_feature_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), '01.w_Defects_output', 'features')
    promptGenerator = PromptGenerator(defect_feature_dir)
    promptGenerator.feature_convertor()  # 特徵映射
    print(PROMPT_BASE + '\n' + promptGenerator.generate_prompt(
        os.path.join(defect_feature_dir, 'features_CWD-1031_9.json')))
