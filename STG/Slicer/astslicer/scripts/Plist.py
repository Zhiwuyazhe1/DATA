import os
import re

import logging

import handle_plist as hp
import parse_ast
import process_compile_commands as pcc

from Diagnostic import Diagnostic
from Tool import Tool, load_mapping

from constants import ROOT_PATH,compile_command,link_command,INPUT_DIR,CC_dir,CC_dir_2,INFO_dir, SUCCESS_TYPE_FILE
from constants import LEVEL_FAIL, LEVEL_RESULTS, SUCCESS_CODE, VALIDATE_FAIL_CODE, IR_FAIL_CODE
logger = logging.getLogger(__name__)
class Plist:
    from dataclasses import dataclass

    @dataclass(frozen=True)
    class SlicingCriterion:
        """Represents one slicing criterion element.

        Members:
            filename: relative or absolute path to the source file
            func: function name (may be empty)
            line: line number (string or number)
            name: variable name
        """
        filename: str
        func: str
        line: str
        name: str

        def __str__(self) -> str:
            # Keep the original string representation used elsewhere
            return "{}#{}#{}#{}".format(self.filename, self.func, self.line, self.name)

        def __repr__(self) -> str:
            return f"SlicingCriterion({self.filename!r}, {self.func!r}, {self.line!r}, {self.name!r})"
    
    def __init__(self, plist_path, base_dir, cc_dir):
        self.plist_path = plist_path # 文件路径
        self.base_dir = base_dir # 项目根目录
        self.cc_dir = cc_dir # 项目cc.json
        self.file_list = hp.get_files(self.plist_path, self.base_dir)
        self.diagnostic_list = hp.get_diagnostics(self.plist_path)
        self.diagnostic_num = len(self.diagnostic_list)
        self.commands_list=pcc.change_cc(self.cc_dir, self.file_list, self.plist_path, self.base_dir) # 每个plist文件涉及的文件相同
        self.checker_list = ['core.CallAndMessage', 'core.DivideZero', 'core.NullDereference']
        self.success_num = 0
        self.line_count = {}
        self.success_type = {}

    @staticmethod
    def get_slicingCriterion_ast(sc_set , variable_names, filename="", func="", line=""):
        file_name = os.path.basename(filename)
        for name in variable_names:
            sc = Plist.SlicingCriterion(filename, func, line, name)
            sc_set.add(sc)
    @staticmethod
    def create_slicingCriterion(sc_set):
        '''
        处理sc_set：同一个函数内，对同样的变量 只留最后一条记录
        '''
        # sc_set is expected to contain Plist.SlicingCriterion instances
        # Keep one entry per (filename, func, name) with the largest line number when possible
        selected = {}
        for sc in sc_set:
            if not isinstance(sc, Plist.SlicingCriterion):
                # ignore unexpected types but try to stringify
                continue
            key = (sc.filename, sc.func, sc.name)
            # try to convert line to integer for reliable comparison
            try:
                cur_line = int(sc.line)
            except Exception:
                cur_line = None

            if key not in selected:
                selected[key] = sc
                continue

            prev = selected[key]
            try:
                prev_line = int(prev.line)
            except Exception:
                prev_line = None

            # Prefer numeric greater line; if non-numeric, fall back to string compare
            replaced = False
            if cur_line is not None and prev_line is not None:
                if cur_line >= prev_line:
                    replaced = True
            elif cur_line is not None and prev_line is None:
                # prefer numeric over non-numeric
                replaced = True
            elif cur_line is None and prev_line is None:
                if str(sc.line) >= str(prev.line):
                    replaced = True

            if replaced:
                selected[key] = sc

        # produce a deterministic order: sort by filename, func, line (numeric if possible), name
        def sort_key(sc: Plist.SlicingCriterion):
            try:
                ln = int(sc.line)
            except Exception:
                ln = sc.line
            return (sc.filename, sc.func, ln, sc.name)

        items = sorted(selected.values(), key=sort_key)
        slicing_c = '\''+", ".join(str(x) for x in items)+'\''
        return slicing_c
            
    def move_dirs(self, output_dir,target_dir):
        move_command = f'mv {output_dir} {target_dir}'
        logger.info(move_command)
        os.system(move_command)

    def get_slicingCriterion(self, d: Diagnostic, all_events):
        d.slicing_criterion=set([])
        compile_args=[]
        i=1
        warning_function = d.warning_function
        for point in d.paths:# 遍历每一个“point” 获取 location
            if isinstance(point, dict) and point['kind'] == 'event':
                location = point['location'] # 位置
                file_id = location.get('file', 'unknown') # 文件
                line = location.get('line', 'unknown') # 行号
                col = location.get('col', 'unknown') # 列号
                extended_message = point['extended_message']
                abs_path = self.file_list[file_id]
                toParse = f"{abs_path} : {line}"
                if not abs_path.endswith('.h'):
                    # 头文件不更新
                    compile_args=[]
                    for commands in self.commands_list:
                        filename = os.path.join(commands['directory'], commands['file'])
                        if filename == abs_path:
                            compile_args = commands['arguments']
                            break
                    compile_args = [arg for arg in compile_args if arg.startswith('-I') or arg.startswith('-D')]
                function_name = parse_ast.get_function(abs_path, line, compile_args)
                if d.entry==None:# change entry
                    d.set_entry(function_name)
                if d.entry==None:
                    logger.info('None entry, continue')
                    continue
                if (not all_events) and not (function_name == warning_function):
                    continue
                logger.info(f'Point[{i}]:{toParse}')
                func_name, var_names = parse_ast.parse(abs_path, line, compile_args)
                Plist.get_slicingCriterion_ast(d.slicing_criterion, var_names, os.path.relpath(abs_path,ROOT_PATH), func_name, line)
                i+=1
        
        # sc may contain SlicingCriterion instances; convert to strings
        slicing_c = self.create_slicingCriterion(d.slicing_criterion)
        pcc.compile_to_bc(self.commands_list,compile_command,link_command,INPUT_DIR,'temp')
        d.slicing_criterion_str = slicing_c

        d.to_slice_bc = os.path.join(INPUT_DIR,'input.bc')

    def run_slicer(self, tool: Tool, d: Diagnostic, level = 0, must_forward=False):
        # 保证level递增, 0 和 2 需要解析报告
        # set parmas
        forward = True if (level % 2 == 1) else False # 1 3
        forward = must_forward or forward
        all_events = True if (level > 1) else False # 2 3 
        if level == 0 or level == 2:
            self.get_slicingCriterion(d, all_events)
            d.export_slicing_criterion(tool.output_dir)
        if d.slicing_criterion_str.strip() == "\'\'":
            logger.warning(f'No slicing criterion for Diagnostic:{d.name}, skip.')
            return IR_FAIL_CODE # 跳过编译测试
        logger.warning(f'Slicing Criterion for Diagnostic:{d.name}: {d.slicing_criterion_str}')

        run_code = d.run_IR_slice(tool, forward).returncode
        if SUCCESS_CODE != run_code:
            logger.warning(f'Diagnostic:{d.name} IR slicing failed with code {run_code}, skip.')
            return run_code # dg运行出错，跳过
        if not d.reset_files(ROOT_PATH, INFO_dir, self.file_list):
            return IR_FAIL_CODE  # 无.c文件，跳过编译测试

        # move info directory
        info_in_output = os.path.join(tool.output_dir,'input-instsInfo.txt')
        info_move_command = f'mv {INFO_dir} {info_in_output}'
        tool.run_system_command(info_move_command)

        d.run_AST_slice(CC_dir,info_in_output,tool)
        d.copy_header_files()
        d.run_Del(CC_dir_2,info_in_output,tool)
        d.rollback_header_files()
        d.move_and_clean_files(tool.output_dir,self.file_list)
        tool.clean_files()
        tool.set_entry(d.entry)
        return tool.compile_test()

    def process_single_plist(self, tool):
        if self.commands_list == None or self.file_list == None or self.diagnostic_list == None:
            self.success_num = -1
            return
        tool.set_plist_path(self.plist_path)
        self.success_type = load_mapping(SUCCESS_TYPE_FILE)
        file_n = os.path.basename(self.plist_path)[:-6]
        d_index = 1
        nums_ok = 0
        for diagnostic in self.diagnostic_list:
            tool.set_output(d_index, file_n+str(d_index))
            d_name = file_n+str(d_index)
            d = Diagnostic(diagnostic, d_name)
            if not d.paths :
                d_index+=1
                continue

            if d.name in self.success_type and self.success_type[d.name] in LEVEL_RESULTS: # 已经成功的跳过
                nums_ok+=1
                d_index+=1
                tool.defectInfoCreator.create_defect_info(diagnostic, self.file_list, d_index, tool.output_dir, "defect_info_original.json")

                continue
            else:
                self.success_type[d.name] = LEVEL_FAIL # 初始化
            
            tool.defectInfoCreator.create_defect_info(diagnostic, self.file_list, d_index, tool.output_dir, "defect_info_original.json")

            level = 0 
            must_forward = True if 'Memory leak' in d.bug_type else False # 内存泄漏必须正向切片
            while(level < 4 and self.success_type[d.name] == LEVEL_FAIL):
                status_code = self.run_slicer(tool, d, level,must_forward)
                if status_code == SUCCESS_CODE:# 无问题
                    nums_ok+=1
                    self.success_type[d.name] = LEVEL_RESULTS[level]
                elif status_code == IR_FAIL_CODE:
                    level += 1
                    logger.warning(f'Do not have C file, level up to {level} slicing. Diagnostic:{d.name}')
                    continue
                elif status_code == VALIDATE_FAIL_CODE:# 可编译 无缺陷
                    level += 1
                    logger.warning(f'Do not match, level up to {level} slicing. Diagnostic:{d.name}')
                else:# 编译错误
                    level = 4
                    logger.warning(f'Compile error, level up to {level} slicing. Diagnostic:{d.name}')
                    
            d_index+=1
            self.line_count[d.name] = d.line_count
        self.success_num = nums_ok
        return

