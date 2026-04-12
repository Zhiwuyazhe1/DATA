import os
import json
import logging
import Tool
logger = logging.getLogger(__name__)

from collections import defaultdict
class DefectCase:
    """表示一个缺陷用例"""
    
    def __init__(self, defect_type: str, description: str, line_number: int, 
                 directory: str, entry: str, functions: set, files:set, is_cross_file: bool, 
                 is_cross_function: bool, cc_dir:str, id:str, warning_func = "", trace=[], compile_commands = [], info_path ='', labels = set(), executed_lines = {}):
        """
        初始化缺陷用例对象
        
        Args:
            defect_type: 缺陷类型描述
            description: 缺陷详细描述
            line_number: 告警行号
            directory: 所属目录
            functions: 经过的函数列表set
            is_cross_file: 是否跨文件
            is_cross_function: 是否跨函数
        """
        self.defect_type = defect_type
        self.description = description
        self.line_number = line_number
        self.warning_func = warning_func
        self.directory = directory
        self.entry = entry
        self.functions = functions
        self.is_cross_file = is_cross_file
        self.is_cross_function = is_cross_function
        self.trace = trace
        self.compile_commands = compile_commands
        self.cc_dir = cc_dir
        self.files = files
        self.id = id
        self.info_path = info_path  # 缺陷信息文件路径
        self.labels = labels # 缺陷标签集合
        self.executed_lines = executed_lines # 相关代码行
        self.codes = '' # trace中涉及的代码片段
        self.other_info = {} # 其他信息字典
        
    def __str__(self):
        return (f"\nDefectCase({self.defect_type}, entry:{self.entry}, at line {self.line_number}, "
                f"include functions={self.functions}, in {self.directory})\n")

    def __repr__(self):
        return self.__str__()
    
    @classmethod
    def from_json(cls, json_file_path: str, cc_dir: str = None, case_id: str = None):
        """
        从JSON文件加载缺陷用例
        
        Args:
            json_file_path: JSON文件路径
            cc_dir: 编译命令输出目录，如果为None则使用默认路径
            case_id: 缺陷用例ID，如果为None则使用文件名作为ID
            
        Returns:
            DefectCase: 加载的缺陷用例对象
        """
        with open(json_file_path, 'r', encoding='utf-8') as f:
            loaded_data = json.load(f)
        
        # 处理JSON可能是列表或字典的情况
        if isinstance(loaded_data, list):
            if len(loaded_data) == 0:
                raise ValueError(f"JSON文件 {json_file_path} 为空列表")
            data = loaded_data[0]  # 取列表中的第一个元素
        else:
            data = loaded_data
        
        # 生成或获取case_id
        if case_id is None:
            case_id = os.path.splitext(os.path.basename(os.path.dirname(json_file_path)))[0]
        
        # 生成cc_dir
        if cc_dir is None:
            cc_dir = os.path.join(os.path.dirname(data.get("info_path", "")),'compile_commands.json')
        
        # 从trace中提取涉及的文件
        files = set()
        if 'files' in data:
            for file_item in data['files']:
                files.add(file_item)
        elif "trace" in data:
            for trace_item in data["trace"]:
                if "file_path" in trace_item:
                    files.add(trace_item["file_path"])
        
        # 已处理的字段集合
        processed_fields = {
            "bug_type", "description", "line_number", "warning_func", 
            "entry_function", "entry", "directory", "functions", 
            "is_single_file", "is_single_function", "trace", "info_path",
            "files", "labels", "checker", "codes", "executed_lines"
        }
        
        # 创建DefectCase对象
        defect_case = cls(
            defect_type=data.get("bug_type", ""),
            description=data.get("description", ""),
            line_number=data.get("line_number", 0),
            entry=data.get("entry", data.get("warning_func", "")),
            directory=data.get("directory", ""),
            functions=data.get("functions", set()),
            is_cross_file=not data.get("is_single_file", True),  # 取反逻辑
            is_cross_function=not data.get("is_single_function", True),  # 取反逻辑
            trace=data.get("trace", []),
            compile_commands=[],
            cc_dir=cc_dir,
            files=files,
            id=case_id,
            info_path = json_file_path,
            labels = data.get("labels", set()),
            executed_lines = data.get("executed_lines", {})
        )
        
        # 存储未处理的字段到other_info
        for key, value in data.items():
            if key not in processed_fields:
                defect_case.other_info[key] = value
        
        return defect_case
    
    def to_dict(self):
        """将缺陷用例对象转换为字典格式"""
        result = {
            "bug_type": self.defect_type, # 兼容
            "checker":"", # 兼容
            "description": self.description,
            "warning_func" : self.warning_func,
            "line_number": self.line_number,
            "directory": self.directory,
            "entry_function": self.entry,
            "functions": list(self.functions),
            "is_single_file": not self.is_cross_file, # 兼容
            "is_single_function": not self.is_cross_function, # 兼容
            "trace": self.trace,
            "info_path": self.info_path,
            "files" : list(self.files),
            "labels" : list(self.labels),
            "codes" : self.codes,
            "executed_lines":self.executed_lines
        }
        
        # 将other_info中的字段也添加到结果中
        result.update(self.other_info)
        
        return result
    
    def out_to_json(self,output_path=None):
        if not output_path:
            output_path = "/home/nishikino/benchmarks/defect_scenario_c/natureScripts/test.json"
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, 'w') as write_f:
            write_f.write(json.dumps(self.to_dict(), default=lambda obj:obj.__dict__, indent=4, ensure_ascii=False))

    def parse_trace(self, context=3):
        """
        根据 trace 提取缺陷相关代码（无变量、无多余信息）
        context: 每条 trace 行前后保留几行上下文
        """
        trace_list = self.trace
        # 1. 聚合文件-行号 + 记录TRACE顺序ID（从1开始）+ 函数信息
        file_line_trace_map = defaultdict(lambda: defaultdict(list))  # {文件路径: {行号: [trace_id1, trace_id2...]}}
        trace_func_map = {}  # {trace_id: function_name}
        
        for trace_id, item in enumerate(trace_list, 1):  # trace_id从1开始计数
            file_path = item["file_path"]
            line_num = item["line"]
            func_name = item.get("function", "unknown")  # 提取函数信息
            
            file_line_trace_map[file_path][line_num].append(trace_id)
            trace_func_map[trace_id] = func_name

        # 2. 生成LLM友好的代码片段
        llm_input = []
        llm_input.append("说明：[TRACE-N]表示第N个trace点（按执行顺序）")
        for file_path, line_trace_ids in file_line_trace_map.items():
            if not os.path.exists(file_path):
                # llm_input.append(f"【文件不存在】{file_path}")
                continue

            # 读取源码（兼容UTF-8/GBK编码）
            try:
                with open(file_path, "r", encoding="utf-8") as f:
                    code_lines = f.readlines()
            except UnicodeDecodeError:
                with open(file_path, "r", encoding="gbk") as f:
                    code_lines = f.readlines()

            # 提取所有trace行号
            trace_lines = set(line_trace_ids.keys())
            total_code_lines = len(code_lines)

            expand_lines = set()
            for executed_file, lines in self.executed_lines.items():
                if executed_file == file_path:
                    expand_lines.update(lines)

            expand_lines = sorted(expand_lines)

            # 拼接LLM输入文本（带TRACE ID）
            file_name = os.path.basename(file_path)
            llm_input.append(f"\n=== 文件{file_name}  缺陷代码片段===")
            llm_input.append("-" * 60)
            
            code_snippet = []
            prev_trace_id = None
            prev_func = None
            
            for ln in expand_lines:
                code = code_lines[ln - 1].rstrip("\n").strip()
                if ln in trace_lines:
                    # 拼接TRACE ID（如[TRACE-1,3]）
                    trace_ids = sorted(line_trace_ids[ln])
                    trace_label = f"[TRACE-{','.join(map(str, trace_ids))}]"
                    curr_trace_id = trace_ids[0]
                    curr_func = trace_func_map.get(curr_trace_id, "unknown")
                    
                    # 检测函数切换（进入新函数时加标记）
                    if prev_func != curr_func:
                        code_snippet.append(f"---→ 函数: {curr_func}")
                    
                    code_snippet.append(f"{ln:4d} {trace_label:<12} | {code}")
                    prev_trace_id = curr_trace_id
                    prev_func = curr_func
                else:
                    code_snippet.append(f"{ln:4d} {'':<12} | {code}")

            llm_input.append("\n".join(code_snippet))
            llm_input.append("-" * 60)

        # 合并为最终文本
        final_llm_text = "\n".join(llm_input)
        self.codes = final_llm_text
