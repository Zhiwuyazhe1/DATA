import os
import json
import logging
logger = logging.getLogger(__name__)
import sys
from constants import DSC_PATH
sys.path.append(DSC_PATH)
from DefectCaseInfo import DefectCase
import parse_ast

class DefectInfoCreator:
    def __init__(self, project: str, directory: str):
        self.project = project
        self.directory = directory

    def create_cc_json(self, case_dir, file_list, output_dir) -> str:
        output_file = os.path.join(output_dir, 'compile_commands.json')
        arguments = ["clang", "-c"]
        compile_commands = []
        for file in file_list:
            compile_commands.append({
                "directory": case_dir,
                "arguments": arguments + [file],
                "file": file
            })
        with open(output_file, 'w') as write_f:
            write_f.write(json.dumps(compile_commands, indent=4, ensure_ascii=False))
        return output_file

    def create_defect_info(self, diagnostic, file_list, case_id, output_dir, info_name = 'defect_info.json') -> None:
        case_dir = output_dir
        # step1. 遍历trace
        description = diagnostic.get('description', 'No description')# 获取警报的描述
        checker = diagnostic.get('check_name', 'Unknown')
        bug_type = diagnostic.get('type', 'Unknown')
        warning_function = diagnostic.get('issue_context', 'Unknown')
        executed_lines = diagnostic.get('ExecutedLines', {})
        entry = None
        paths = diagnostic.get('path',{})
        files = set()
        functions = set()
        line_number = 0
        trace = []
        for point in paths:# 遍历每一个“point” 获取 location
            if isinstance(point, dict) and point['kind'] == 'event':
                location = point['location'] # 位置
                file_id = location.get('file', 'unknown') # 文件
                line = location.get('line', 'unknown') # 行号
                col = location.get('col', 'unknown') # 列号
                extended_message = point['extended_message']
                start_line = line 
                start_col = col
                end_line = line  
                end_col = col
                if 'ranges' in point:
                    ranges = point['ranges'] # range
                    if ranges and len(ranges) > 0:
                        # 获取起始和结束位置的dict
                        range_item = ranges[0]  # 取第一个子数组（核心范围）
                        start_pos = range_item[0]  # 起始位置dict
                        end_pos = range_item[1]    # 结束位置dict
                        
                        # 2. 提取具体的行、列、文件索引
                        # 起始位置
                        start_line = start_pos['line']    
                        start_col = start_pos['col']
                        # 结束位置
                        end_line = end_pos['line']        
                        end_col = end_pos['col'] 
                file_name = file_list[file_id]
                files.add(file_name)
                file_path = os.path.join(output_dir, file_name)
                func_name, var_names = parse_ast.parse(file_path, line)
                functions.add(func_name)
                if entry == None:# change entry
                    entry = func_name
                line_number = line # 取最后一个点的行号
                trace_item = {
                    "file_path": file_path,
                    "line": start_line,
                    "col": start_col,
                    "line2": end_line,
                    "col2": end_col,
                    "function": func_name,
                    "var": var_names
                }
                trace.append(trace_item)
        # step2.  构造cc.json
        cc_dir = self.create_cc_json(case_dir, files, output_dir)
        # step3.  
        output_file = os.path.join(output_dir, info_name)

        executed_lines_map = {}
        for file_id, lines in executed_lines.items():
            file_name = file_list[int(file_id)]
            file_path = os.path.join(output_dir, file_name)
            executed_lines_map[file_path] = lines

        defect_case = DefectCase(
            defect_type = bug_type,
            description = description,
            line_number = line_number,
            entry = entry,
            warning_func=warning_function,
            directory = self.directory,
            functions = functions,
            is_cross_file = len(files) > 1,
            is_cross_function= len(functions) > 1,
            trace = trace,
            compile_commands=[],
            cc_dir = cc_dir,
            files = files,
            id = case_id, # 用例的标识
            info_path = output_file, # 存储结果的路径
            labels = [],
            executed_lines = executed_lines_map
        )
        
        defect_case.out_to_json(output_file)
        logger.info(f'Defect info created at {output_file}')
