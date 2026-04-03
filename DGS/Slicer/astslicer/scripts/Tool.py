# coding =utf-8
# pip install chardet
import re, chardet
import os
import logging
logger = logging.getLogger(__name__)
import parse_ast

def get_encode(path):
    with open(path,'rb') as f:
        encode = (chardet.detect(f.read()))['encoding']
        return encode
def isSpace(line:str):
    if line in['\n', '\r\n'] or line.strip()=="":
        return True
    return False

def isdef(line:str):
    for i in range(0,10):
        space = ' '*i
        if f'#{space}ifdef ' in line or f'#{space}ifndef ' in line or f'#{space}define 'in line or f"#{space}if "in line or\
        f'#{space}endif' in line or f'if{space}defined' in line or f"#{space}include" in line or f"#{space}else" in line:
            return True

    return False

def isifdef(line:str):
    if '#ifdef ' in line or'if defined' in line or ('#if ' in line and 'T_DESC' not in line):
        return True
    return False

def isEndif(line:str):
    if '#endif' in line:
        return True
    return False

def isExternC(line:str):
    if '#ifdef __cplusplus' in line or '#if __cplusplus' in line:
        return True
    return False

def hasChar(line:str):
    for i in line:
        if i!=' ' and i!='/':
            return True
    return False

# 删除注释和空行
# 添加头文件
def code_trans(in_path, out_path, header_files):
    logger.info(f'code_trans in_path:{in_path} out_path:{out_path}')
    if not os.path.exists(in_path):
        return
    if in_path.endswith('.c') or in_path.endswith('.h'):
        bds0='//.*' #匹配单行注释
        targeto=re.compile(bds0)
        encode=get_encode(in_path)
        if encode=='utf-8':
            f=open(in_path, encoding='utf-8')
        elif encode=='GB2312':
            f=open(in_path, encoding='gbk')
        else:
            f=open(in_path, encoding=encode)

        data=f.read()
        result = targeto.findall(data)
        for i in result:
            if not hasChar(i):
                continue
            if "<description>" in i:
                continue
            if i.count('\"') %2==1:
                continue
            data=data.replace(i,'')

        result=re.sub('//.*?|/\\*.*?\\*/', '',data,flags=re.S)
        result = re.sub(r'#define(.*\\\n)+.*', '',result)
        ret='#include "header_func.h"\n'
        if in_path.endswith('.c'):
            logger.info(f'headers:{header_files}')
            for header in header_files:
                ret+=f'#include "{header}"\n'

        pre=';'
        externc=0
        ifdef=0

        for line in result.splitlines():
            
            line = line.replace('UV_ENOBUFS', 'ENOBUFS')
            if isExternC(line):
                externc+=1
            elif externc==0 and isifdef(line):
                ifdef+=1
            elif externc>0 and isEndif(line):
                externc-=1
                continue

            if ifdef > 0 and isEndif(line):
                ifdef-=1
                continue
            elif ifdef>0:
                continue

            if line.strip()==';' and pre==';':
                continue
            if externc>0 or ifdef >0:
                continue
            if(not isSpace(line)) and (not isdef(line)):
                ret=ret+line+'\n'
                pre=line.strip()[-1]
        with open(out_path,'w',encoding='utf-8') as file:
            file.write(ret)

    else:
        logger.info('invalid file format')

import json
from typing import Dict, Optional
def load_str_num_mapping(file_path: str) -> Optional[Dict[str, int]]:
    """从JSON文件读取子文件夹->切片前相关文件代码行映射（子文件夹级）"""
    if not os.path.exists(file_path):
        return {}
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        # 过滤非法数据，确保键为字符串、值为非负整数
        return {
            str(k): int(v) for k, v in data.items() 
            if isinstance(k, str) and isinstance(v, (int, float)) and v >= 0
        }
    except Exception as e:
        return {}

def save_str_num_mapping(
    mapping: Dict[str, int],
    file_path: str
) -> bool:
    """
    保存映射（追加模式，存在的key自动覆盖，不存在的key新增）
    :param mapping: 要保存的str->int映射字典
    :param file_path: 保存路径
    :return: 成功True/失败False
    """
    # 读取已有数据（无文件则创建空字典）
    existing = load_str_num_mapping(file_path) or {}
    # 覆盖已有key，新增新key
    existing.update(mapping)
    try:
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(existing, f, ensure_ascii=False, indent=2)
        return True
    except Exception:
        return False


def load_mapping(file_path: str) -> Optional[Dict[str, str]]:
    """从JSON文件读取子文件夹->切片前相关文件代码行映射（子文件夹级）"""
    if not os.path.exists(file_path):
        return {}
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        # 过滤非法数据，确保键为字符串、值为非负整数
        return {
            str(k): str(v) for k, v in data.items() 
        }
    except Exception as e:
        return {}

def save_mapping(
    mapping: Dict[str, str],
    file_path: str
) -> bool:
    """
    保存映射（追加模式，存在的key自动覆盖，不存在的key新增）
    :param mapping: 要保存的str->str映射字典
    :param file_path: 保存路径
    :return: 成功True/失败False
    """
    # 读取已有数据（无文件则创建空字典）
    existing = load_mapping(file_path) or {}
    # 覆盖已有key，新增新key
    existing.update(mapping)
    try:
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(existing, f, ensure_ascii=False, indent=2)
        return True
    except Exception:
        return False



import plistlib
from constants import SUCCESS_CODE, VALIDATE_FAIL_CODE
from DefectInfoCreator import DefectInfoCreator
class Tool:
    def __init__(self, tool_dir, output_dir, proj_name, base_dir = ''):
        self.dg_dir = tool_dir+'/dg-master/build/tools/llvm-slicer'# DG目录
        self.ast_dir = tool_dir+'/astslicer/build/astslicer ' # ASTSlicer目录
        self.del_dir = tool_dir+'/delSpace/build/delSpace ' # delSpace目录
        self.output_dir_origin = os.path.join(output_dir, proj_name) # output/proj目录
        self.output_dir = self.output_dir_origin # output/proj/report-xxx
        self.INPUT_DIR = os.path.join(tool_dir,'inputs')
        self.COMPILE_INFO_DIR = os.path.join(tool_dir,'compile_info')
        self.plist_path = ""
        self.index = 1
        self.entry = ""
        self.defectInfoCreator = DefectInfoCreator(proj_name, base_dir)

    def run_system_command(self, commond):
        logger.info(commond)
        os.system(commond)

    def set_plist_path(self, plist_path):
        self.plist_path = plist_path
    
    def set_entry(self, entry):
        self.entry = entry

    def set_output(self, index, fn):
        # Build a safe path and ensure the output directory exists before any moves
        self.output_dir = os.path.join(self.output_dir_origin, fn)
        self.index = index
        try:
            os.makedirs(self.output_dir, exist_ok=True)
        except Exception as e:
            logger.error(f"Failed to create output directory {self.output_dir}: {e}")

    def clean_files(self):
        # 收集头文件
        header_files = []
        for filename in os.listdir(self.output_dir):
            if filename.endswith('.h') and 'header_type.h' not in filename and 'header_func.h' not in filename:
                header_files.append(os.path.basename(filename))
                # remove_command = f'rm {self.output_dir}/{filename}'
                filename = os.path.join(self.output_dir, filename)
                code_trans(filename, filename, header_files)

        for filename in os.listdir(self.output_dir):
            if filename.endswith('.c') and '_end' not in filename:
                logger.info(f'clean:{filename}')
                basename, ext = os.path.splitext(filename)
                filename = os.path.join(self.output_dir, filename)
                filename2 =  os.path.join(self.output_dir, basename+'_end'+ext)
                # 清空filename2文件内容
                open(filename2, 'w').close()
                code_trans(filename, filename2, header_files)

        # move compile_commands.json
        CC_dir_2 = os.path.join(self.COMPILE_INFO_DIR,'compile_commands.json') # instSlice
        cc_in_output = os.path.join(self.output_dir,'compile_commands.json') 
        cc_copy_command = f'cp {CC_dir_2} {cc_in_output}'
        logger.info(cc_copy_command)
        os.system(cc_copy_command)

    def compile_test(self):
        compile_command = f'scan-build -plist-html -disable-checker deadcode.DeadStores -o {self.output_dir} clang -c -emit-llvm *_end.c > {self.output_dir}/compile_output.log 2>&1'
        logger.info(f'cd {self.output_dir} && {compile_command}')
        code = os.system(f'cd {self.output_dir} && {compile_command}')
        
        if code == SUCCESS_CODE: # 编译成功
            code = SUCCESS_CODE if self.validate_plist() else VALIDATE_FAIL_CODE
        else:
            logger.warning(f'Compile failed with code {code}, dir:{self.output_dir}')
        return code

    def validate_plist(self):
        """
        检查子文件夹中是否包含期望的缺陷，并输出判断依据的取值
        返回值：True表示存在匹配缺陷，False表示不存在
        """
        target_index = self.index - 1  # 转换为0-based索引

        # 2. 解析目标plist文件，获取指定下标的缺陷信息
        try:
            with open(self.plist_path, 'rb') as f:
                plist_data = plistlib.load(f)
        except Exception as e:
            logger.info(f"解析目标plist文件失败: {e}")
            return False

        diagnostics = plist_data.get('diagnostics', [])
        if not isinstance(diagnostics, list):
            logger.info("plist中diagnostics不是列表类型")
            return False

        # 检查下标是否有效
        if target_index < 0 or target_index >= len(diagnostics):
            logger.info(f"缺陷下标{target_index + 1}超出范围（总缺陷数：{len(diagnostics)}）")
            return False

        target_defect = diagnostics[target_index]
        # 定义需要比对的关键字
        check_keys = ['description', 'category', 'type', 'check_name', 'issue_context']

        # 提取目标缺陷的关键信息并输出
        target_info = {}
        logger.warning("判断依据（目标缺陷取值）：")
        for key in check_keys:
            value = target_defect.get(key)
            target_info[key] = value
            logger.warning(f"  {key}: {repr(value)}")  # 使用repr显示空值和特殊字符

        # 3. 遍历子文件夹中的所有plist文件进行比对
        for root, _, files in os.walk(self.output_dir):
            for file in files:
                if file.endswith('.plist'):
                    plist_file_path = os.path.join(root, file)
                    try:
                        with open(plist_file_path, 'rb') as f:
                            current_plist = plistlib.load(f)
                    except Exception as e:
                        logger.info(f"跳过无效plist文件 {plist_file_path}: {e}")
                        continue

                    current_diagnostics = current_plist.get('diagnostics', [])
                    if not isinstance(current_diagnostics, list):
                        current_diagnostics = [current_diagnostics]

                    file_list = current_plist.get('files', [])

                    # 检查是否引入新的缺陷
                    new_defects = []
                    for idx, defect in enumerate(current_diagnostics):
                        new_defect_flag = True
                        for i in range(len(diagnostics)):
                            i_defect = diagnostics[i]
                            match_flag = True
                            for key in check_keys:
                                if defect.get(key) != i_defect.get(key):
                                    match_flag = False
                                    break
                            if match_flag:
                                new_defect_flag = False
                                break
                        if new_defect_flag:
                            new_defects.append(idx)
                    if new_defects:
                        idxs = ', '.join(map(str, new_defects))
                        logger.warning(f"在文件 {plist_file_path} 中引入新的缺陷（索引：{idxs}）")


                    # 检查当前plist中的每个缺陷
                    for idx, defect in enumerate(current_diagnostics):
                        # 比对所有关键字
                        match_flag = True
                        for key in check_keys:
                            if defect.get(key) != target_info[key]:
                                match_flag = False
                                break
                        if match_flag:
                            logger.warning(f"在文件 {plist_file_path} 中找到匹配缺陷（索引：{idx}）")
                            self.defectInfoCreator.create_defect_info(defect, file_list, self.index, self.output_dir)
                            return True


        return False

# scan-build -plist-html -o /home/nishikino/new_version_benchmarks/output/report-ebqQJ41 clang -c -emit-llvm *_end.c > /home/nishikino/new_version_benchmarks/output/report-ebqQJ41/compile_output.log 2>&1
