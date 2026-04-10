import json
import os
import subprocess
import logging
from handle_plist import is_system_header_file
logger = logging.getLogger(__name__)

root_path = os.path.dirname(os.path.dirname((os.path.dirname(__file__)))) #benchmark
INPUT_DIR = os.path.join(root_path,'inputs')
COMPILE_INFO_DIR = os.path.join(root_path,'compile_info')
CC_dir= os.path.join(INPUT_DIR,'compile_commands.json')  # 编译原始文件用的
CC_dir_2= os.path.join(COMPILE_INFO_DIR,'compile_commands.json') #编译切片后
def compile_to_bc(cc_list, compile_command, link_command, dir_name, bc_name='temp'):
    logger.info(f'###compile files to bc, base compile {compile_command}')
    os.makedirs(dir_name, exist_ok=True)
    i = 1
    cp_command=''
    for c in cc_list:
        fn = convert_path(os.path.join(c['directory'], c['file']))
        args = c['arguments']
        cmd_args = [os.path.join(compile_command, args[0])]
        for j in range(1, len(args)):
            arg = args[j]
            if j > 1 and args[j - 1] == '-o':
                # 处理输出文件
                output_file = f'{dir_name}{os.sep}{bc_name}{i}.bc'
                cmd_args.append(output_file)
            elif args[j] == c['file']:
                continue
            else:
                # 宏参数转义
                if arg.startswith('-D'):
                    cmd_args.append(arg.replace('"', '\"'))
                else:
                    cmd_args.append(arg)
        cmd_args.append(fn)

        logger.info(' '.join(cmd_args))  # 打印命令
        subprocess.run(cmd_args, check=True)  # 执行命令，check=True 会在出错时抛出异常

        cp_command = 'cp {1}{2}{3}{4}.bc {1}{2}input.bc'.format(fn, dir_name, os.sep, bc_name, i)
        link_command += '{1}{2}{3}{4}.bc '.format(fn, dir_name, os.sep, bc_name, i)
        i += 1
    link_command += ' -o {0}{1}{2}.bc'.format(dir_name, os.sep, 'input')
    if i>2:
        os.system(link_command)
        logger.info(link_command)
    else:
        os.system(cp_command)
        logger.info(cp_command)


def convert_path(path: str) -> str:
    return path.replace(r'\/'.replace(os.sep, ''), os.sep)


def find_cc(cp_units, file_list, file_id):
    logger.info(f'find cc>> {file_list}')
    res_list = []
    for fl in file_list:
        if fl[-2:]!='.c' or is_system_header_file(fl):
            continue
        isFind=False
        for unit in cp_units: #每一条对应一个文件
            if not unit['file'].endswith('.c'):
                continue
            '''
            "directory": "/home/nishikino/cpy-test",
            "file": "Modules/unicodedata.c"
            '''
            filename = convert_path(unit['directory']+ os.sep + unit['file'])
            if filename.strip() == convert_path(fl).strip():
                temp = {'arguments': unit['arguments'], 'directory': convert_path(unit['directory']),
                        'file': unit['file']}
                res_list.append(temp)
                isFind=True
                break
        if not isFind:
            print(f'can not find cc for {fl}, continue defect.')
            return None
        # #找同目录下其他c文件的
        #     dir_name = os.path.dirname(fl)
        #     for unit in cp_units: #每一条对应一个文件
        #         filename = os.path.dirname(convert_path(unit['directory']+ os.sep + unit['file']))
        #         if filename.strip() == convert_path(dir_name).strip():
        #             temp = {'arguments': unit['arguments'], 'directory': convert_path(unit['directory']),
        #                     'file': fl[len(convert_path(unit['directory']))+1:]}
        #             res_list.append(temp)
        #             break

    # cc_temp_name =os.path.join(COMPILE_INFO_DIR, 'compile_commands_ex-{}.json'.format(file_id))

    # os.makedirs(os.path.dirname(cc_temp_name), exist_ok=True)
    # with open(cc_temp_name, 'w') as file:
    #     json.dump(res_list, file, indent=4)
    return res_list


# clang允许的编译参数 删去了-O，因为优化后会影响dg
clangArgs = ['-c', '-o', '-emit-llvm', '-I', '-W', '-D', '-g']


def get_sliced_cc(rel_list):
    # 处理arguments
    for j in range(len(rel_list) - 1, -1, -1):
        c=rel_list[j]
        directory = c['directory']
        args = c['arguments']
        fn = c['file']
        delList = []
        for i in range(len(args) - 1, 0, -1):
            item = args[i]
            if item[-2:] == '.c':
                args[i] = item[:-2]+'_InstSlice'+item[-2:]

        c['arguments'] = args
        c['file'] = fn[:-2]+'_InstSlice'+fn[-2:]
        rel_list[j] = c
        
    cc_final_name_2 = CC_dir_2
    logger.info(f'Writing sliced compile commands to {cc_final_name_2}')
    os.makedirs(os.path.dirname(cc_final_name_2), exist_ok=True)
    with open(cc_final_name_2, 'w') as f:
        json.dump(rel_list, f, indent=4)

import copy
def cgpath_fix_cc(rel_list, file_id='', base_dir=''):
    # 处理arguments
    for j in range(len(rel_list) - 1, -1, -1):
        # print(j)
        c=rel_list[j]
        flag = False
        g_flag =False
        directory = c['directory']
        args = c['arguments']
        file = c['file']
        delList = []
        for i in range(len(args) - 1, 0, -1):
            item = args[i]
            if item[-2:]=='.c':
                args[i] = file # 修改
            if item == '-emit-llvm':
                flag = True
            if item == '-g':
                g_flag = True
            if item[0] == '-' and item != '-emit-llvm':
                if item[:2] not in clangArgs:  # FIX ME:不严谨
                    delList.append(i)
                if item[0:2]=='-I' and item[2]!=os.sep:
                    args[i] = item[0:2]+directory+os.sep+item[2:]
                # elif ('-D' in item) and ('=' in item) :
                #     delList.append(i)
            elif item == '--compatible-with-void-pointers':
                delList.append(i)
            elif item[0] == os.sep and not os.path.exists(item):
                args[i] = convert_path(item)
                if not os.path.exists(args[i]):
                    delList.append(i)
        for i in delList:
            args.pop(i)
        if not flag:
            args.insert(2,'-emit-llvm')
        # args.append('-I/usr/include')
        # args.append('-I/usr/local/llvm/include/clang')
        # args.append('-I/usr/local/llvm/lib/clang/12.0.0/include') #不同机器需要修改
        if not g_flag:
            args.append('-g')
        c['arguments'] = args
        rel_list[j] = c
        # if not flag:
            # rel_list.pop(j) #删除不是编译成bc的编译命令
    cc_final_name = CC_dir
    os.makedirs(os.path.dirname(cc_final_name), exist_ok=True)
    with open(cc_final_name, 'w') as f:
        json.dump(rel_list, f, indent=4)

    get_sliced_cc(copy.deepcopy(rel_list))


def change_cc(cc_path, file_list,plist_path, base_dir):# 从cc_path里把file_list对应的几个文件的编译命令抽取出来
    with open(cc_path) as f:
        cc = json.load(f)
    res_list = []
    logger.info(f'>>>>{type(cc)}')
    file_id = os.path.basename(plist_path)
    if isinstance(cc, list):
        cp_units = cc
        res_list = find_cc(cp_units, file_list,file_id)
        if res_list is None:
            return None
        cgpath_fix_cc(res_list,file_id, base_dir)
        return res_list
    else:
        logger.info('--is not a vaild format!!')
        return res_list


# LLVM_dir = 'llvm-12.0.0.obj/bin'  # llvm-12.0.0.obj/bin
# link_command = LLVM_dir + os.sep+'llvm-link '
# compile_command = LLVM_dir +os.sep
# link_command = 'llvm-link '
# compile_command = ''
# commands_list = change_cc('compile_commands_full.json', ['ow_memcpy.c', 'dead_code.c'], 1)
# compile_to_bc(commands_list, compile_command, link_command, 'temp-testdemo', 'testdemo')
