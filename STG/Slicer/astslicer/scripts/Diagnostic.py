import os
import logging
import subprocess
import handle_plist as hp
# from typing import Dict, List, Tuple
logger = logging.getLogger(__name__)
class Diagnostic:
    def __init__(self, diagnostic, d_name):
        self.description = diagnostic.get('description', 'No description')# 获取警报的描述
        self.checker = diagnostic.get('check_name', 'Unknown')
        self.bug_type = diagnostic.get('type', 'Unknown')
        self.warning_function = diagnostic.get('issue_context', 'Unknown')
        self.paths = diagnostic.get('path',{})
        self.slicing_criterion = None
        self.slicing_criterion_str=''
        self.to_slice_bc=''
        self.file_paths = set()
        self.line_count = 0 # 相关文件原代码行数量
        self.name = d_name
        self.entry = None

    def set_entry(self,entry):
        self.entry = entry

    def export_slicing_criterion(self, output_dir):
        if not self.slicing_criterion:
            return
        os.makedirs(output_dir, exist_ok=True)
        criterion_path = os.path.join(output_dir, f'{self.name}_slicing_criterion.txt')

        def dedup_slicing_criteria(criteria):
            """
            对SlicingCriterion集合按name去重，保留最后出现的对象（依赖集合的迭代顺序）
            
            Args:
                criteria: 包含SlicingCriterion对象的集合
            
            Returns:
                去重后的SlicingCriterion集合
            """
            # 以name为键构建字典，后续元素覆盖之前的，保留最后一个
            name_to_criterion = {}
            for criterion in criteria:
                name_to_criterion[criterion.name] = criterion
            
            # 转换为集合返回
            return set(name_to_criterion.values())
        temp_slicing_criterion = dedup_slicing_criteria(self.slicing_criterion)
        with open(criterion_path, 'w', encoding='utf-8') as f:
            for item in temp_slicing_criterion:
                if str(item).endswith('()'):
                    continue
                f.write(str(item) + '\n')
        logger.info(f'Exported slicing criterion to {criterion_path}')
        
    def run_IR_slice(self, tool, forward):
        logger.info(f'Slicing entry is {self.entry}')
        argument = '-forward' if forward else ''
        dg_command = f'{tool.dg_dir} -sc {self.slicing_criterion_str} -entry {self.entry} {argument} {self.to_slice_bc}'
        logger.info(dg_command)
        ret = subprocess.run(dg_command, shell=True)
        return ret
    
    def run_AST_slice(self,cc,info,tool):
        os.makedirs(tool.output_dir, exist_ok=True)
        header_func_path=f'{tool.output_dir}/header_func.h'
        with open(header_func_path, 'w') as f:
            f.write('#ifndef __HEADER_FUNC_H__ \n#define __HEADER_FUNC_H__\n')
            f.write('#include\"header_type.h\" \n')
            
        header_type_path=f'{tool.output_dir}/header_type.h'
        with open(header_type_path, 'w') as f:
            f.write('#ifndef __HEADER_TYPE_H__ \n#define __HEADER_TYPE_H__\n')
            f.write('#include\"../common.h\" \n')
            
        ast_command = tool.ast_dir + f' {cc} inst {info} {tool.output_dir}/header_func.h  {tool.output_dir}/header_type.h >{tool.output_dir}/log.txt'
        logger.info(ast_command)
        ret = subprocess.run(ast_command, shell=True)
        with open(header_func_path, 'a') as f:
            f.write('#endif')
        with open(header_type_path, 'a') as f:
            f.write('#endif')
        return ret

    def copy_header_files(self):
        logger.info(f'file_paths:{self.file_paths}')
        for header in self.file_paths:
            if header.endswith('.h'):
                # 1.把 xxx.h 复制到同目录下xxx_copy.h，作为备份
                # 2.把 xxx_InstSlice.h 更名为xxx.h
                base, ext = os.path.splitext(header)
                copy_header = f"{base}_copy{ext}"
                inst_slice_header = f"{base}_InstSlice{ext}"
                if os.path.exists(inst_slice_header):
                    # 复制备份
                    copy_command = f'cp {header} {copy_header}'
                    os.system(copy_command)
                    logger.info(copy_command)
                    # 更名
                    move_command = f'cp {inst_slice_header} {header}'
                    os.system(move_command)
                    logger.info(move_command)

    def run_Del(self,cc,info,tool):
        del_command = tool.del_dir+f' {cc} {info} >{tool.output_dir}/log2.txt'
        logger.info(del_command)
        ret = subprocess.run(del_command, shell=True)
        return ret

    def rollback_header_files(self):
        for header in self.file_paths:
            if header.endswith('.h'):
                # 1.把 xxx_copy.h 更名为xxx.h
                # 2.把 xxx_output.h 更名为xxx_InstSlice_output.h
                base, ext = os.path.splitext(header)
                copy_header = f"{base}_copy{ext}"
                output_header = f"{base}_output{ext}"
                if os.path.exists(copy_header):# 复原
                    move_command = f'mv {copy_header} {header}'
                    os.system(move_command)
                    logger.info(move_command)
                if os.path.exists(output_header):
                    inst_slice_output_header = f"{base}_InstSlice_output{ext}"
                    move_command = f'mv {output_header} {inst_slice_output_header}'
                    os.system(move_command)
                    logger.info(move_command)


    def reset_files(self, base_dir, input_file, file_list) -> bool:
        """
        解析输入文件，提取所有文件路径并转换为绝对路径
        如果没有.c文件，返回False，否则返回True
        """
        self.file_paths = set()
        has_c = False
        with open(input_file, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()  # 去除首尾空白（换行符、空格、制表符）
                # 只处理以 "Define " 开头的行（文件路径仅在这类行中）
                if line.startswith("Define "):
                    # 分割行：按空格分割为列表（格式：["Define", "函数名", "文件路径", "行数"]）
                    parts = line.split()
                    if len(parts) >= 3:
                        relative_path = parts[2]
                        absolute_path = os.path.abspath(os.path.join(base_dir, relative_path))
                        self.file_paths.add(absolute_path)
                        has_c = True if absolute_path.endswith('.c') else has_c
        # 重置：plist所有文件涉及的
        for filepath in file_list:
            slice_path = filepath[:-2] + '_InstSlice' + filepath[-2:]
            slice_output_path = filepath[:-2] + '_InstSlice_output' + filepath[-2:]
            logger.info('reset:'+filepath +' ' + slice_path + ' ' + slice_output_path)
            if os.path.exists(slice_path):
                reset_command = f'rm {slice_path}'
                os.system(reset_command)
                logger.info(reset_command)
            if os.path.exists(slice_output_path):
                reset_command = f'rm {slice_output_path}'
                os.system(reset_command)
                logger.info(reset_command)
        return has_c

    def count_single_file(self, file_path: str) -> int:
        """
        统计单个文件的行数（适配 C 语言注释规则，当前脚本注释统计已注释掉）
        返回：(总行数, 代码行, 注释行, 空行)
        """
        total = 0
        code = 0
        comment = 0
        blank = 0
        in_multi_comment = False  # 标记是否在多行注释 /* ... */ 中

        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line_stripped = line.strip()
                total += 1
                # 处理空行
                if not line_stripped:
                    blank += 1
                    continue

                # 注释统计逻辑
                if in_multi_comment:
                    comment += 1
                    if '*/' in line_stripped:
                        in_multi_comment = False
                    continue

                if '/*' in line_stripped:
                    comment += 1
                    if '*/' not in line_stripped:
                        in_multi_comment = True
                    continue

                if line_stripped.startswith('//'):
                    comment += 1
                    continue
                code += 1

        return code #(total, code, comment, blank)
    
    def move_and_clean_files(self, output_dir, file_list): 
        self.line_count = 0
        for fp in file_list:
            if hp.is_system_header_file(fp):
                continue
            fp_s = fp[:-2]+'_InstSlice_output'+fp[-2:]
            fp_r = fp[:-2]+'_InstSlice'+fp[-2:]
            fn = os.path.basename(fp)
            if os.path.exists(fp_s):
                logger.info(f'clean: {fp}')
                if fp in self.file_paths:
                    self.line_count += self.count_single_file(fp)
                    move_command = f'mv {fp_s} {output_dir}/{fn}'
                    os.system(move_command)
                    logger.info(move_command)
                    rm_command = f'rm {fp_r}'
                    logger.info(rm_command)
                    os.system(rm_command)
                else:
                    move_command = f'rm {fp_s}'
                    os.system(move_command)
                    logger.info(move_command)
                    rm_command = f'rm {fp_r}'
                    logger.info(rm_command)
                    os.system(rm_command)