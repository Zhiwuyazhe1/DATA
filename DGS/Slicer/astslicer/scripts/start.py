# -*- coding:utf-8 -*-
import json
import os
import argparse
import time
import logging
from datetime import datetime
import re

import process_compile_commands as pcc
import handle_plist as hp
from Plist import Plist
from Tool import Tool, save_str_num_mapping,save_mapping
from constants import OUTPUT_DIR, COMMON_FILE_NAME, COMMON_FILE_PATH, TOOL_DIR, COMPILE_INFO_DIR, LINE_COUNT_FILE, SUCCESS_TYPE_FILE

logger = logging.getLogger(__name__)
def convert_path(path: str) -> str:
    return path.replace(r'\/'.replace(os.sep, ''), os.sep)

def print_bugs(directory_path):
    warning_type_counts = hp.count_warning_types_in_directory(directory_path)
    print("Warning types and their counts:")
    count=0
    for k,v in warning_type_counts.items():
        print(k,v)
        count+=v
    print(count)


def main(directory_path, base_dir, cc_dir, proj_name):
    PROJ_OUTPUT = os.path.join(OUTPUT_DIR, proj_name)
    COMMON_PATH = os.path.join(PROJ_OUTPUT, COMMON_FILE_NAME)
    if not os.path.exists(COMMON_PATH):
        os.makedirs(PROJ_OUTPUT, exist_ok=True)
        copy_command = f'cp {COMMON_FILE_PATH} {COMMON_PATH}'
        os.system(copy_command)
        logger.info(copy_command)
    hp.remove_unused_plists(directory_path) # 删除 空警报、重复警报、路径包含头文件的警报 （头文件待定，若采取复制到某文件的手段或许可以）
    logging.warning(f'start process {proj_name}')
    logging.info(f"slicer run, tools in {TOOL_DIR}")
    tool = Tool(TOOL_DIR, OUTPUT_DIR, proj_name, base_dir)
    success_num=0
    tot_num = 0
    # 逐个处理文件
    i=0
    for filename in os.listdir(directory_path):
        if filename.endswith('.plist'):
            # if 'report-CfuRPd' not in filename:
            # # #     # and 'report-9zird3' not in filename and 'report-8KO1JL' not in filename:
                # continue
            i+=1
            plist_path = os.path.join(directory_path, filename)
            logging.warning(f'Processing {plist_path}')
            logging.info(f'Processing {plist_path}')
            p = Plist(plist_path, base_dir, cc_dir)
            tot_num += p.diagnostic_num 
            p.process_single_plist(tool)
            if p.success_num == -1:
                logging.info(f'Skipping plist {plist_path} due to missing compile commands or files.')
                tot_num -= p.diagnostic_num
                continue
            success_num += p.success_num
            logging.warning(f'Process {tot_num} diagnostics, {success_num} success.\n')
            save_str_num_mapping(p.line_count, LINE_COUNT_FILE)
            save_mapping(p.success_type, SUCCESS_TYPE_FILE)
            time.sleep(5)
    
    logging.warning(f'Totally process {tot_num} diagnostics, {success_num} success. \n')
    

    # 删除compile_info文件夹
    rm_command = f'rm -rf {COMPILE_INFO_DIR}'
    os.system(rm_command)
    logger.info(f'Removed compile info directory: {COMPILE_INFO_DIR}')
    return

if __name__ == "__main__":
    # MULTI = True
    MULTI = False
    # 生成带时间戳的日志文件名
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    if MULTI:
        proj_name = 'multi_test'
        log_filename = f"{OUTPUT_DIR}/logs/{proj_name}_{timestamp}.log"  # FIX ME
        os.makedirs(f'{OUTPUT_DIR}/logs', exist_ok=True)
        # 配置日志
        logging.basicConfig(
            filename=log_filename,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            level=logging.INFO,
            filemode='w'
        )
    # # redis
    # base_dir = '/home/nishikino/new_version_benchmarks/tests/redis-8.2.2/src' # 项目根目录
    # directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/redis-8.2.2_tests'
    # cc_dir = '/home/nishikino/new_version_benchmarks/tests/redis-8.2.2/compile_commands.json'
    # proj_name = 'redis-8.2.2'
    # main(directory_path, base_dir, cc_dir, proj_name)

    # # cpython-3.10.17
    # base_dir = '/home/nishikino/new_version_benchmarks/tests/cpython-3.10.17' # 项目根目录
    # directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/cpython-3.10.17_tests'
    # cc_dir = '/home/nishikino/new_version_benchmarks/tests/cpython-3.10.17/compile_commands.json'
    # proj_name = 'cpython-3.10.17'
    # main(directory_path, base_dir, cc_dir, proj_name)

    # # jq-1.8.1
    # base_dir = '/home/nishikino/new_version_benchmarks/tests/jq-1.8.1' # 项目根目录
    # directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/jq-1.8.1_tests'
    # cc_dir = '/home/nishikino/new_version_benchmarks/tests/jq-1.8.1/compile_commands.json'
    # proj_name = 'jq-1.8.1'
    # main(directory_path, base_dir, cc_dir, proj_name)

    # # libevent-2.1.12
    # base_dir = '/home/nishikino/new_version_benchmarks/tests/libevent-2.1.12-stable' # 项目根目录
    # directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/libevent-2.1.12_tests'
    # cc_dir = '/home/nishikino/new_version_benchmarks/tests/libevent-2.1.12-stable/compile_commands.json'
    # proj_name = 'libevent-2.1.12'
    # main(directory_path, base_dir, cc_dir, proj_name)

    # # libuv-1.51.0
    # base_dir = '/home/nishikino/new_version_benchmarks/tests/libuv-1.51.0' # 项目根目录
    # directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/libuv-1.51.0_tests'
    # cc_dir = '/home/nishikino/new_version_benchmarks/tests/libuv-1.51.0/compile_commands.json'
    # proj_name = 'libuv-1.51.0'
    # main(directory_path, base_dir, cc_dir, proj_name)

    # # bzip3-1.4.0
    # base_dir = '/home/nishikino/new_version_benchmarks/tests/bzip3-1.4.0' # 项目根目录
    # directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/bzip3-1.4.0_tests'
    # cc_dir = '/home/nishikino/new_version_benchmarks/tests/bzip3-1.4.0/compile_commands.json'
    # proj_name = 'bzip3-1.4.0'
    # main(directory_path, base_dir, cc_dir, proj_name)
    
    # # libpng-libpng16
    # base_dir = '/home/nishikino/new_version_benchmarks/tests/libpng-libpng16' # 项目根目录
    # directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/libpng-libpng16_tests'
    # cc_dir = '/home/nishikino/new_version_benchmarks/tests/libpng-libpng16/compile_commands.json'
    # proj_name = 'libpng-libpng16'
    # main(directory_path, base_dir, cc_dir, proj_name)
    
    # # libusb-1.0.27
    # base_dir = '/home/nishikino/new_version_benchmarks/tests/libusb-1.0.27/libusb' # 项目根目录
    # directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/libusb-1.0.27_tests'
    # cc_dir = '/home/nishikino/new_version_benchmarks/tests/libusb-1.0.27/compile_commands.json'
    # proj_name = 'libusb-1.0.27'

    # itc
    base_dir = '/home/nishikino/new_version_benchmarks/tests/itc-cases' # 项目根目录
    directory_path = '/home/nishikino/new_version_benchmarks/tests/scan-results/itc-cases_tests'
    cc_dir = '/home/nishikino/new_version_benchmarks/tests/itc-cases/compile_commands.json'
    proj_name = 'itc-cases'
    if not MULTI:
        log_filename = f"{OUTPUT_DIR}/logs/{proj_name}_{timestamp}.log"  # FIX ME
        os.makedirs(f'{OUTPUT_DIR}/logs', exist_ok=True)
        # 配置日志
        logging.basicConfig(
            filename=log_filename,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            level=logging.INFO,
            filemode='w'
        )
    main(directory_path, base_dir, cc_dir, proj_name)

# ./configure CC=clang CXX=clang++
    '''
    
    /home/nishikino/benchmark/dg-master/build/tools/llvm-slicer -sc '/home/nishikino/cpy-test/Python/pylifecycle.c##532#config,/home/nishikino/cpy-test/Python/pylifecycle.c##532#interp,' -entry pycore_create_interpreter ../temp-compile/input.ll
    
    ./astslicer /home/nishikino/benchmark/astslicer/compile_info/compile_commands.json  inst /home/nishikino/benchmark/astslicer/temp-compile/input-instsInfo.txt /home/nishikino/benchmark/test-res/header_func.h /home/nishikino/benchmark/test-res/header_type.h >log.txt
    
    '''