'''
    读入特征说明

    json文件
'''
# pip3 install pandas
# pip3 install openpyxl
import pandas as pd
import os
import json
import argparse
import logging

logger = logging.getLogger(__name__)

PATH = os.path.join(os.path.dirname(__file__), '特征说明_local.xlsx')
FILTER = os.path.join(os.path.dirname(__file__), 'filter.json')
feature_mapping = {}
feature_name_mapping = {}
feature_id_mapping = {}
feature_expl_mapping = {}
count_feature = []


def dict_to_newline_string(input_dict):
    result = ""
    for key, value in input_dict.items():
        result += f"\t{key}: {value}\n"
    return result


# 将特征值和特征值解释存储到字典中
def read_xlsx(file_path):
    try:
        df = pd.read_excel(file_path)
        # df.info()
        for index, row in df.iterrows():
            feature_name = row['feature_name']
            feature_means = row['特征说明']
            feature_value = row['特征值']
            feature_id = row['feature_id']
            feature_explanation = row['特征值解释']
            if feature_value == 'X':  # 计数特征值设置为0
                count_feature.append(feature_name)
                feature_value = 0
            if feature_name not in feature_mapping:
                feature_mapping[feature_name] = {}
                feature_name_mapping[feature_name] = {}
                feature_id_mapping[feature_name] = {}
            feature_mapping[feature_name][feature_value] = feature_explanation
            feature_name_mapping[feature_name] = feature_means
            feature_id_mapping[feature_name][feature_value] = feature_id
            feature_expl_mapping[feature_id] = feature_explanation
    except FileNotFoundError:
        print(f"错误：未找到文件 {file_path} ，请检查文件路径是否正确。")
    except Exception as e:
        print(f"错误：读取文件时出现问题，错误信息：{e}。")


def decompose_to_powers_of_two(num):
    powers = []
    bit_position = 0
    while num:
        if num & 1:
            powers.append(2 ** bit_position)
        num >>= 1
        bit_position += 1
    return powers


# json特征转换为dict 下划线拼接特征名
# todo 單文件、單函數加入特徵説明
def trans_feature(origin_feature):
    keys = generate_ids_from_json(origin_feature)
    if keys is None:
        return origin_feature
    delete_key_list = []

    features = {}
    feature_ids = {}  # 记录id

    # 使用的宏
    if 'UsedMacro' not in keys:
        keys['UsedMacro'] = []
        features['使用的宏'] = []
    # 运算的宏
    if 'OpMacro' not in keys:
        keys['OpMacro'] = []
        features['用于运算的宏'] = []

    feature_ids['feature'] = []
    feature_ids['Use Macro'] = []
    feature_ids['Operational Macros'] = []
    for key, value in keys.items():
        key_cn = feature_name_mapping.get(key, key)
        if key in feature_mapping and value > 0:
            id_list = []
            explanation_list = []
            if key in count_feature:
                fake_value = 0
                means = feature_mapping[key][fake_value].replace('X', str(value))
                id_withX = feature_id_mapping[key][fake_value]
                id_list.append(id_withX + str(value) + id_withX[-1])  # e.g. Semantic_ControlFlow_If_X2X
            else:
                for v in decompose_to_powers_of_two(value):
                    if v in feature_mapping[key] and 'unused' not in feature_mapping[key][v]:
                        explanation_list.append(feature_mapping[key][v])
                        id_list.append(feature_id_mapping[key][v])
                means = ','.join(explanation_list)
                # print(f"{key} 特征值为 {value} 的解释是: {means}")
            features.setdefault(key_cn, means)
            feature_ids['feature'].extend(id_list)
        elif value == 0:
            delete_key_list.append(key)
        elif 'Semantic_Macro_' in key:  # 构造 使用过的宏 和 作为运算的宏
            keys['UsedMacro'].append(key[15:])
            features['使用的宏'].append(key[15:])
            feature_ids['Use Macro'].append(key[15:])
            if value > 1:
                keys['OpMacro'].append(key[15:])
                features['用于运算的宏'].append(key[15:])
                feature_ids['Operational Macros'].append(key[15:])
            delete_key_list.append(key)
        elif 'Macro' not in key:
            features.setdefault(key, value)

    # return dict_to_newline_string(features)
    return feature_ids  # dict_to_newline_string(feature_ids)


def trans_feature_to_ids(keys):
    delete_key_list = []

    features = {}
    feature_ids = {}  # 记录id

    # 使用的宏
    if 'UsedMacro' not in keys:
        keys['UsedMacro'] = []
        features['使用的宏'] = []
    # 运算的宏
    if 'OpMacro' not in keys:
        keys['OpMacro'] = []
        features['用于运算的宏'] = []

    feature_ids['feature'] = []
    feature_ids['Use Macro'] = []
    feature_ids['Operational Macros'] = []
    for key, value in keys.items():
        key_cn = feature_name_mapping.get(key, key)
        if key in feature_mapping and value > 0:
            id_list = []
            explanation_list = []
            if key in count_feature:
                fake_value = 0
                means = feature_mapping[key][fake_value].replace('X', str(value))
                id_withX = feature_id_mapping[key][fake_value]
                id_list.append(id_withX + str(value) + id_withX[-1])  # e.g. Semantic_ControlFlow_If_X2X
            else:
                for v in decompose_to_powers_of_two(value):
                    if v in feature_mapping[key] and 'unused' not in feature_mapping[key][v]:
                        explanation_list.append(feature_mapping[key][v])
                        id_list.append(feature_id_mapping[key][v])
                means = ','.join(explanation_list)
                # print(f"{key} 特征值为 {value} 的解释是: {means}")
            features.setdefault(key_cn, means)
            feature_ids['feature'].extend(id_list)
        elif value == 0:
            delete_key_list.append(key)
        elif 'Semantic_Macro_' in key:  # 构造 使用过的宏 和 作为运算的宏
            keys['UsedMacro'].append(key[15:])
            features['使用的宏'].append(key[15:])
            feature_ids['Use Macro'].append(key[15:])
            if value > 1:
                keys['OpMacro'].append(key[15:])
                features['用于运算的宏'].append(key[15:])
                feature_ids['Operational Macros'].append(key[15:])
            delete_key_list.append(key)
        elif 'Macro' not in key:
            features.setdefault(key, value)

    # return dict_to_newline_string(features)
    return feature_ids  # dict_to_newline_string(feature_ids)


# 输入list，找到对应的含义
def get_features(id_list):
    feature_expl = []
    for i in range(len(id_list)):
        id = id_list[i]
        if id.endswith('X'):
            last_underscore_index = id.rfind('_')  # _X2X 查找最后一个下划线
            id_origin = id[:last_underscore_index + 2]  # _X
            real_value = id[last_underscore_index + 2:-1]
            id_list[i] = feature_expl_mapping.get(id_origin, id).replace('X', real_value)
            feature_expl.append(feature_expl_mapping.get(id_origin, id).replace('X', real_value))
        else:
            id_list[i] = feature_expl_mapping.get(id, id)
            feature_expl.append(feature_expl_mapping.get(id, id))
    return feature_expl


def generate_ids_from_json(data, parent_key=""):
    """
    递归遍历 JSON 数据，逐层拼接 key，生成 id。
    """
    if not isinstance(data, dict):
        return None
    ids = {}

    try:
        with open(FILTER, 'r', encoding='utf-8') as f:
            filter_data = json.load(f)
            features_to_LLM = filter_data.get('features_to_LLM', [])  # 读取需要保留的特征列表
            for key, value in data.items():
                # 拼接当前层级的 key
                new_key = f"{parent_key}_{key}" if parent_key else key
                if isinstance(value, dict):
                    # 如果 value 是字典，递归调用
                    ids.update(generate_ids_from_json(value, new_key))
                else:
                    # 如果 value 不是字典, 且需要保留, 则生成 id
                    if any(new_key.startswith(prefix) for prefix in features_to_LLM):
                        ids[new_key] = value
                    else:
                        # print(f"过滤特征: {new_key}")
                        logger.log(logging.DEBUG, f"过滤特征: {new_key}")
    except Exception as e:
        logger.error(f"Error reading filter file {FILTER}: {e}")
    return ids


def feature_to_expl_list(feature_path):
    try:
        # 打开并读取 JSON 文件内容
        with open(feature_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            keys = generate_ids_from_json(data)
            features = trans_feature_to_ids(keys)
            feature_expl = get_features(features.get('feature', []))
            defect_type = keys.get('bug_type', '未知类型')
            trace = keys.get('trace', '无trace')
            codes = keys.get('codes', '无代码')
            description = keys.get('description', '无描述')
            return defect_type, feature_expl, trace, codes, description
    except Exception as e:
        # 处理文件读取或 JSON 解析过程中可能出现的异常
        logger.error(f"Error processing {feature_path}: {e}")
    return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', '-d',
                        required=True,
                        help='特征信息路径')
    read_xlsx(PATH)
    args = parser.parse_args()

    print(feature_to_expl_list(args.directory))
