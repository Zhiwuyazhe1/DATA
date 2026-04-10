# -*- coding:utf-8 -*-
from clang.cindex import Config

Config.set_library_file("/usr/lib/llvm-18/lib/libclang.so")
import os
import re
import argparse
import sys
import csv
from datetime import datetime
from Tool import Tool
from DefectCaseInfo import DefectCase
from PromptGenerator import PromptGenerator, SYSTEM_PROMPT
from LLMClient import LLMClient
import logging
import json
logger = logging.getLogger(__name__)


def convert_path(path: str) -> str:
    return path.replace(r'\/'.replace(os.sep, ''), os.sep)


TOOL_DIR = f'{os.path.dirname(os.path.dirname(__file__))}'  # defect_classfier
CONFIG_DIR = f'{TOOL_DIR}/config.txt'
SCRIPTS_DIR = os.path.dirname(__file__) # defect_classfier/scripts

def import_defect_cases(path: str) -> list:
    """
    载入缺陷用例列表
    Args:
        path: 代码路径
    Returns:
        缺陷用例列表
    """
    defect_cases = []
    for root, _, files in os.walk(path):
        dir_name = os.path.basename(root)
        for file in files:
            if file.endswith('defect_info.json'):
                defect_case = DefectCase.from_json(os.path.join(root, file))
                defect_cases.append(defect_case)
    return defect_cases

def main(directory_path, output_dir):
    if not os.path.isdir(directory_path):
        raise NotADirectoryError(f"指定的目录不存在: {directory_path}")

    logging.info("defect run")
    tot_num = 0
    tool = Tool(TOOL_DIR, output_dir)

    # 载入已有缺陷用例，输入路径
    logger.info('=====start create defect cases =====')
    defectCases = import_defect_cases(directory_path)
    logger.info('=====end of create defect cases =====')

    def append_to_json_file(file_path, new_data):
        """
        将新的键值对添加到JSON文件中。如果文件不存在或无效，则创建一个新的JSON文件。
        """
        if os.path.exists(file_path):
            with open(file_path, 'r', encoding='utf-8') as file:
                try:
                    data = json.load(file)
                except json.JSONDecodeError:
                    data = {}  # Initialize as empty if the file is invalid
        else:
            data = {}  # Initialize as empty if the file doesn't exist
        data.update(new_data)
        with open(file_path, 'w', encoding='utf-8') as file:
            json.dump(data, file, ensure_ascii=False, indent=4)
        
    
    # 计算通用特征
    logger.info('=====start compute defect features=====')
    defect_feature_dir = f"{tool.output_dir}/features"
    os.makedirs(f'{defect_feature_dir}', exist_ok=True)

    # 建立特征文件路径与原始defect_info的映射
    feature_to_defect_info = {}
    
    for case in defectCases:
        defect_info = case.info_path
        case.parse_trace()
        case.out_to_json(output_path=defect_info)
        sseq_output_path = os.path.join(defect_feature_dir, f"features_{case.id}.json")
        if os.path.exists(sseq_output_path):
            feature_to_defect_info[sseq_output_path] = defect_info  # 保存映射关系
            continue
        # 保存映射关系
        feature_to_defect_info[sseq_output_path] = defect_info
        status_code = tool.run_Sseq(case.cc_dir, defect_info, CONFIG_DIR, sseq_output_path, case.id)
        if status_code == 0:
            tot_num += 1
            logger.info(f'{defect_info} works complete!')
            codes_data = {"codes": case.codes}
            description_data = {"description": case.description}
            append_to_json_file(sseq_output_path, codes_data)
            append_to_json_file(sseq_output_path, description_data)
        else:
            logger.warning(f'{defect_info} works failed! error code {status_code}, retry:')
            status_code = tool.run_Sseq(case.cc_dir, defect_info, CONFIG_DIR, sseq_output_path, case.id)
            logger.warning(f'retry result is {status_code}')

    logger.info('=====end of compute defect features=====\n')

    # 检查是否有特征文件生成
    if len(os.listdir(defect_feature_dir)) == 0:
        logger.warning('Empty defect feature dir.')
        return

    # 生成prompt并调用LLM
    promptGenerator = PromptGenerator(defect_feature_dir)
    promptGenerator.feature_convertor()  # 执行特征映射
    
    file_list = promptGenerator.file_list
    formatted_date =  datetime.now().strftime("%Y%m%d_%H%M") #datetime.now().strftime('%Y%m%d')  # 获取当前日期并格式化

    # LLM输出路径：output目录下的LLM_output子目录，使用模型版本和时间标识一次调用
    llm_output_dir = os.path.join(output_dir, "LLM_output", LLMClient.LLM_VERSION, formatted_date)  # llm输出目录 如 path/defect_classfier/bzip3-1.4.0_output/LLM_output/qwen3.5-122b-a10b/20260316_1848/results.json
    llm_output_path = os.path.join(llm_output_dir,'results.json')
    os.makedirs(f'{llm_output_dir}', exist_ok=True)

    # 初始化LLM客户端，设置系统提示词
    client = LLMClient(SYSTEM_PROMPT)
    cnt=0
    for defect_file in file_list:
        if defect_file in feature_to_defect_info:
            defect_info_path = feature_to_defect_info[defect_file]
            dir_name = os.path.basename(os.path.dirname(defect_info_path))

            if os.path.exists(defect_info_path):
                with open(defect_info_path, 'r', encoding='utf-8') as file:
                    try:
                        data = json.load(file)
                    except json.JSONDecodeError:
                        data = {}
                # 跳过已经生成该模型的标签的缺陷用例    
                if f"{LLMClient.LLM_VERSION}_bug_type" in data:
                    logger.info(f'{defect_file} already has LLM results, skip.')
                    continue
        else:
            continue
        
        prompt = promptGenerator.generate_prompt(defect_file)
        logger.info(f'======prompt========\n{prompt}')
        ret_features = client.call_feature_generator(
            prompt=prompt,
            parameters={
                "temperature": 0.5,
                "max_tokens": 2048
            }
        )
        if len(ret_features) == 0:  # 返回空
            continue

        logger.info(f'======LLM return========\n{defect_file}:{ret_features}')
        if isinstance(ret_features, dict) and isinstance(next(iter(ret_features.values())), dict):
            output_data = {defect_file: next(iter(ret_features.values()))}
        else:
            output_data = {defect_file: ret_features}
        if cnt == 0:
            with open(llm_output_path, 'w', encoding='utf-8') as file:
                json.dump(output_data, file, ensure_ascii=False, indent=4)
        else:
            append_to_json_file(llm_output_path, output_data)
        cnt+=1
        
        # 将LLM返回结果写入原始的defect_info.json
        if defect_file in feature_to_defect_info:
            defect_info_path = feature_to_defect_info[defect_file]
            if os.path.exists(defect_info_path):
                try:
                    llm_output = output_data[defect_file]
                    logger.info(f'LLM output for {defect_file}: {llm_output}')
                    label_data = {
                        f"{LLMClient.LLM_VERSION}_NOTASK_bug_type": '未知',
                        f"{LLMClient.LLM_VERSION}_NOTASK_labels": []
                    }
                    # 提取type_label和cause_labels，填入对应字段
                    if "type_label" in llm_output:
                        label_data[f"{LLMClient.LLM_VERSION}_NOTASK_bug_type"] = llm_output["type_label"]
                    if "cause_labels" in llm_output:
                        label_data[f"{LLMClient.LLM_VERSION}_NOTASK_labels"] = llm_output["cause_labels"]


                    append_to_json_file(defect_info_path, label_data)
                    logger.info(f'Updated {defect_info_path} with LLM results')
                except Exception as e:
                    logger.error(f'Failed to update {defect_info_path}: {e}')
            else:
                logger.warning(f'Defect info file not found: {defect_info_path}')
    logging.info(f'Success process {tot_num} defect cases\n')
    return

# 示例： /bin/python3 []/code-infrastructure/defect_classfier/scripts/start.py  -d '[]/output/bzip3-1.4.0/

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', '-d',
                        required=True,
                        help='存储缺陷用例的路径')

    parser.add_argument('--output', '-o',
                        help='程序特征信息和本次运行日志输出路径',
                        default=None)
    args = parser.parse_args()

    directory = os.path.normpath(args.directory)
    # 生成带时间戳的日志文件名
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    proj_name = os.path.basename(directory)

    # 输出路径和日志路径
    if args.output is None:
        output_dir = f'{TOOL_DIR}/{proj_name}_output'
    else:
        output_dir = args.output

    log_filename = f"{output_dir}/logs/{proj_name}_{timestamp}.log"  # FIX ME
    os.makedirs(f'{output_dir}/logs', exist_ok=True)
    print(log_filename)
    # 配置日志
    logging.basicConfig(
        filename=log_filename,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        level=logging.INFO,
        filemode='w'
    )

    try:
        main(directory, output_dir)
    except NotADirectoryError as e:
        logger.error(f"错误: {e}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"发生未知错误: {e}")
        sys.exit(1)
