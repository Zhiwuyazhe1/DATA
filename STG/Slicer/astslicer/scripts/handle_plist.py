import os
import plistlib
from collections import defaultdict
from collections import Counter

import handle_plist as hp

root_path = os.path.dirname(os.path.dirname((os.path.dirname(__file__)))) #benchmark
def count_warning_types_in_directory(directory_path):
    # 初始化一个计数器来累积所有文件的警告类型
    warning_counts = Counter()

    # 遍历目录中的所有文件
    for filename in os.listdir(directory_path):
        # 检查文件是否为 .plist 文件
        if filename.endswith('.plist'):
            filepath = os.path.join(directory_path, filename)
            try:
                # 打开并读取 .plist 文件
                with open(filepath, 'rb') as file:
                    plist_data = plistlib.load(file)
                
                # 检查 diagnostics 字段
                if 'diagnostics' in plist_data:
                    # 遍历 diagnostics，统计每种警告类型的 check_name
                    for diagnostic in plist_data['diagnostics']:
                        warning_type = diagnostic.get('check_name', 'Unknown')
                        warning_counts[warning_type] += 1
            except Exception as e:
                logger.info(f"Error processing {filepath}: {e}")
    
    return dict(warning_counts)

def count_files_in_plist(directory):
    file_counter = Counter()
    
    # 遍历目录中的所有文件
    for filename in os.listdir(directory):
        if filename.endswith('.plist'):
            plist_path = os.path.join(directory, filename)
            
            # 读取 plist 文件
            with open(plist_path, 'rb') as file:
                try:
                    plist_data = plistlib.load(file)
                    # 获取 files 字段的值
                    files = plist_data.get('files', [])
                    # 更新计数器
                    file_counter[tuple(files)]+=1
                    # for file_name in files:
                    #     file_counter[file_name] += 1
                except Exception as e:
                    logger.info(f"Error reading {plist_path}: {e}")
    
    return file_counter

def process_plist_files(directory):
    # 存储每个 plist 文件的相关信息
    plist_info = {}
    
    # 遍历目录中的所有文件
    for filename in os.listdir(directory):
        if filename.endswith('.plist'):
            plist_path = os.path.join(directory, filename)
            
            try:
                # 打开并读取 plist 文件
                with open(plist_path, 'rb') as file:
                    plist_data = plistlib.load(file)
                
                # 获取 files 字段的值
                files = plist_data.get('files', [])
                
                # 初始化警报计数器
                warning_counts = defaultdict(int)
                
                # 检查 diagnostics 字段
                if 'diagnostics' in plist_data:
                    for diagnostic in plist_data['diagnostics']:
                        warning_type = diagnostic.get('check_name', 'Unknown')
                        warning_counts[warning_type] += 1
                
                # 存储结果
                plist_info[filename] = {
                    'files': files,
                    'warning_counts': dict(warning_counts)  # 转换为普通字典
                }
                
            except Exception as e:
                logger.info(f"Error processing {plist_path}: {e}")
    
    return plist_info


import hashlib

def remove_empty_plist_files(directory):
    # 遍历指定目录
    for filename in os.listdir(directory):
        # 检查文件是否为 .plist 文件
        if filename.endswith('.plist'):
            filepath = os.path.join(directory, filename)
            try:
                # 读取 .plist 文件
                with open(filepath, 'rb') as file:
                    plist_data = plistlib.load(file)
                
                # 检查 diagnostics 是否为空
                if 'diagnostics' in plist_data and not plist_data['diagnostics']:
                    # 如果 diagnostics 为空，删除该文件
                    os.remove(filepath)
                    logger.info(f"Deleted empty plist file: {filepath}")
                else:
                    files_list = plist_data.get('files', [])
                    for i in range(0,len(files_list)):
                        # if files_list[i][-2:] ==".h" or files_list[i][-4:] ==".cpp": #排除包含头文件和cpp文件的
                        #     os.remove(filepath)
                        #     logger.info(f"Deleted .h/.cpp plist file: {filepath}")
                        #     break
                        if files_list[i][-4:] ==".cpp": #排除包含cpp文件的
                            os.remove(filepath)
                            logger.info(f"Deleted .cpp plist file: {filepath}")
                            break
                # else:
                    # print(f"Warnings found in {filepath}, file kept.")
            except Exception as e:
                logger.info(f"Error processing {filepath}: {e}")
        else:
            try:
                filepath = os.path.join(directory, filename)
                os.remove(filepath)
            except Exception as e:
                logger.info(f"Error processing {filepath}: {e}")

def hash_plist_file(filepath):
    """计算 plist 文件内容的哈希值"""
    with open(filepath, 'rb') as file:
        # 读取文件内容并生成哈希
        plist_data = plistlib.load(file)
        # 使用 hashlib 生成一个唯一的哈希
        return hashlib.md5(str(plist_data).encode('utf-8')).hexdigest()

def remove_duplicate_plists(directory):
    # 用于存储唯一文件的哈希值和文件路径
    unique_hashes = {}
    
    # 遍历目录中的所有文件
    for filename in os.listdir(directory):
        if filename.endswith('.plist'):
            filepath = os.path.join(directory, filename)
            try:
                # 计算文件的哈希值
                file_hash = hash_plist_file(filepath)
                
                # 如果哈希值已经存在，则删除该文件
                if file_hash in unique_hashes:
                    os.remove(filepath)
                    logger.info(f"Deleted duplicate file: {filepath}")
                else:
                    # 存储哈希值和文件路径
                    unique_hashes[file_hash] = filepath
                    
            except Exception as e:
                logger.info(f"Error processing {filepath}: {e}")

def remove_unused_plists(directory):
    remove_empty_plist_files(directory)
    remove_duplicate_plists(directory)

directory_path = '/home/nishikino/plist_only_cpy'
# directory_path = '/home/nishikino/benchmark/plist_with_html' 
# plist_results = process_plist_files(directory_path)
# # # 打印结果
# for plist_file, info in plist_results.items():
#     print(f">>>>File: {plist_file}",end="\t")
#     print(f"Files: {info['files']}")
#     print("Warning Counts:",end="")
#     for warning_type, count in info['warning_counts'].items():
#         print(f"{warning_type}: {count}",end="")
#     print('')
# remove_empty_plist_files(directory_path)
# remove_duplicate_plists(directory_path)


# 使用示例
# directory_path = '/home/nishikino/benchmark/plist-cpython' 
# remove_duplicate_plists(directory_path)
# plist_results = process_plist_files(directory_path)

# # # 打印结果
# for plist_file, info in plist_results.items():
#     print(f">>>>File: {plist_file}",end="\t")
#     print(f"Files: {info['files']}")
#     print("Warning Counts:[",end="")
#     for warning_type, count in info['warning_counts'].items():
#         print(f"{warning_type}: {count}",end="")
#     print(']')


# directory_path = '/home/nishikino/plist-cpython'
# remove_empty_plist_files(directory_path)

# warning_type_counts = count_warning_types_in_directory('/home/nishikino/benchmark/cpy-plist-html')
# print("Warning types and their counts:")
# count=0
# for k,v in warning_type_counts.items():
#     print(k,v)
#     count+=v
# print(count)

# file_counts = count_files_in_plist(directory_path)

# # 打印统计结果
# for file_name, count in file_counts.items():
#     print(f"{file_name}: {count}")


''' plist only
{'deadcode.DeadStores': 39,
 'core.uninitialized.Assign': 2, 
 'core.NullDereference': 22, 
 'core.UndefinedBinaryOperatorResult': 10, 
 'core.NonNullParamChecker': 12, 
 'core.CallAndMessage': 5, 
 'core.DivideZero': 2, 
 'core.StackAddressEscape': 1
 }
 93

'''
''' with_html
Warning types and their counts:
deadcode.DeadStores 42  +3
core.NullDereference 30 +8
core.CallAndMessage 6 +1
core.uninitialized.Assign 10 +8
core.UndefinedBinaryOperatorResult 13 -3
core.DivideZero 2 *
core.NonNullParamChecker 13 +1
core.StackAddressEscape 1 *
117
'''
import os
import plistlib

def is_system_header_file(file_path):
    system_paths = ['/usr/include/', '/usr/local/include/', '/sys/']
    return any(file_path.startswith(system_path) for system_path in system_paths)
    
def get_files(plist_path, base_dir):
    with open(plist_path, 'rb') as file:
        plist_data = plistlib.load(file)
    files_list = plist_data.get('files', [])
    
    # 可能需要调整
    for i in range(0,len(files_list)):
        if is_system_header_file(files_list[i]):
            continue
        if base_dir not in files_list[i]:
            files_list[i] = os.path.join(base_dir, files_list[i])
        files_list[i] = os.path.abspath(files_list[i])
    return files_list

def get_diagnostics(plist_path):
    with open(plist_path, 'rb') as file:
        plist_data = plistlib.load(file)
    if 'diagnostics' in plist_data:
        return plist_data['diagnostics']
    return None
import logging
logger = logging.getLogger(__name__)
def extract_warnings_from_plist(plist_path,base_dir, txt_path):
    with open(plist_path, 'rb') as file:
        plist_data = plistlib.load(file)
    files_list = plist_data.get('files', [])
    for i in range(0,len(files_list)):
        files_list[i] = base_dir+files_list[i][1:]
    
    logger.info(f'now process{plist_path}')
    str=""
    with open(txt_path, 'w') as txt_file:
        if 'diagnostics' in plist_data:
            for diagnostic in plist_data['diagnostics']:
                # print(diagnostic)
                if 'path' in diagnostic and 'ExecutedLines' in diagnostic:
                    # 获取警报的描述
                    description = diagnostic.get('description', 'No description')
                    checker = diagnostic.get('check_name', 'Unknown')
                    bug_type = diagnostic.get('type', 'Unknown')
                    paths = diagnostic.get('path',{})

                    txt_file.write(f"[Warning]:{checker} : {bug_type}\n")
                    str+=f"[Warning]:{checker} : {bug_type}\n"
                    # 遍历 events 获取 location
                    i=0
                    toParse=""
                    for path in paths:# 遍历每一个“event”
                        if isinstance(path, dict) and path['kind'] == 'event':
                            i+=1
                            location = path['location']
                            file_id = location.get('file', 'unknown')
                            line = location.get('line', 'unknown')
                            col = location.get('col', 'unknown')
                            extended_message = path['extended_message']
                            # 写入 txt 文件
                            txt_file.write(f"step{i}:{extended_message} in File: {files_list[file_id]}, Line: {line}, Column: {col}\n")
                            toParse+=f"{files_list[file_id]} : {line} \n"
                    txt_file.write(toParse+"\n\n")
    file_name = os.path.join(root_path,'total.txt')
    os.makedirs(file_name, exist_ok=True)
    with open(file_name, 'a') as txt_file:
        txt_file.write(str+"\n\n")
        

def process_directory(directory_path,base_dir):
    for filename in os.listdir(directory_path):
        if filename.endswith('.plist'):
            plist_path = os.path.join(directory_path, filename)
            txt_path = os.path.splitext(plist_path)[0] + '.txt'
            extract_warnings_from_plist(plist_path, base_dir ,txt_path)

# if __name__ == '__main__':
#     base_dir = '/home/nishikino/cpython-3.8' # 项目根目录
#     directory_path = '/home/nishikino/benchmark/plist-cpython'  # 该项目对应的警报
#     process_directory(directory_path,base_dir)
