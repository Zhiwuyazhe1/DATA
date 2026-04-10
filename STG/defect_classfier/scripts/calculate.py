import numpy as np
from sklearn.preprocessing import MultiLabelBinarizer
from sklearn.metrics import precision_score, recall_score, f1_score
import pandas as pd
import os
import json
import re

'''
计算模型预测标签与人工标注标签的Jaccard相似度、宏平均F1、微平均F1指标，支持多标签分类评估。
'''
def clean_label_text(label):
    """
    清理标签文本，删除所有空格和符号
    
    Args:
        label: 原始标签字符串
        
    Returns:
        str: 清理后的标签字符串
    """
    if not isinstance(label, str):
        return str(label)
    
    # 删除所有空格
    cleaned = label.replace(' ', '').replace('\t', '').replace('\n', '').replace('\r', '')
    
    # 删除常见符号（保留中文字符、英文字符、数字）
    # 使用正则表达式删除所有非中文、非英文、非数字的字符
    cleaned = re.sub(r'[^\u4e00-\u9fffA-Za-z0-9]', '', cleaned)
    
    return cleaned

def clean_labels_dict(labels_dict):
    """
    清理标签字典中的每个标签
    
    Args:
        labels_dict: 标签字典 {用例id: [标签1, 标签2], ...}
        
    Returns:
        dict: 清理后的标签字典
    """
    cleaned_dict = {}
    for case_id, labels in labels_dict.items():
        if isinstance(labels, list):
            cleaned_labels = [clean_label_text(label) for label in labels if clean_label_text(label)]
            cleaned_dict[case_id] = cleaned_labels
        else:
            # 如果不是列表，转换为列表
            cleaned_label = clean_label_text(labels)
            cleaned_dict[case_id] = [cleaned_label] if cleaned_label else []
    return cleaned_dict

def read_labels_from_excel(excel_path, sheet_name=0):
    """
    从Excel表格读取人工标注标签
    
    Args:
        excel_path: Excel文件路径
        sheet_name: Sheet名称或索引，默认为0（第一个sheet）
        
    Returns:
        dict: {用例id: [标签1, 标签2, 标签3, ...]}的字典映射
              标签为非空值，过滤掉空值
    """
    if not os.path.exists(excel_path):
        raise FileNotFoundError(f"Excel文件不存在: {excel_path}")
    
    try:
        # 读取Excel文件
        df = pd.read_excel(excel_path, sheet_name=sheet_name)
        
        # 验证必需列
        if '用例id' not in df.columns:
            raise ValueError("Excel表格必须包含'用例id'列")
        
        # 标签列名
        label_columns = [col for col in df.columns if col.startswith('标签')]
        if not label_columns:
            raise ValueError("Excel表格必须至少包含一列标签列（如'标签1', '标签2'等）")
        
        # 构建标签字典
        labels_dict = {}
        for idx, row in df.iterrows():
            case_id = str(row['用例id']).strip()
            if pd.isna(case_id) or case_id == '':
                continue
            
            # 收集该行的所有标签（过滤掉NaN和空字符串）
            labels = []
            for label_col in label_columns:
                label_value = row[label_col]
                if pd.notna(label_value):
                    label_str = str(label_value).strip()
                    if label_str:  # 确保标签不为空
                        # 清理标签文本
                        cleaned_label = clean_label_text(label_str)
                        if cleaned_label:  # 确保清理后不为空
                            labels.append(cleaned_label)
            
            labels_dict[case_id] = labels
        
        return labels_dict
    
    except Exception as e:
        raise Exception(f"读取Excel文件时出错: {e}")


def match_labels_by_case_id(labels_dict, case_ids):
    """
    根据用例id列表，从标签字典中提取对应的标签字典
    
    Args:
        labels_dict: {用例id: [标签1, 标签2, ...]}的字典
        case_ids: 用例id列表，用于筛选匹配的用例
        
    Returns:
        dict: {用例id: [标签1, 标签2, ...]}格式的标签字典，只包含在case_ids中的用例
    """
    matched_labels = {}
    missing_cases = []
    
    for case_id in case_ids:
        case_id_str = str(case_id).strip()
        if case_id_str in labels_dict:
            matched_labels[case_id_str] = labels_dict[case_id_str]
        else:
            matched_labels[case_id_str] = []  # 如果找不到，使用空列表
            missing_cases.append(case_id_str)
    
    # if missing_cases:
    #     print(f"警告: 以下用例id在Excel中未找到: {missing_cases}")
    
    return matched_labels


def read_predicted_labels_from_folder(folder_path, label_field_name):
    """
    从指定文件夹中提取模型预测标签
    
    Args:
        folder_path: 文件夹A的路径（包含多个用例子文件夹）
        label_field_name: 标签字段名，如"qwen3.5-35b-a3b_labels"或"deepseek-r1_labels"
        
    Returns:
        dict: {用例id: [标签1, 标签2, ...]}的字典映射
        list: 按遍历顺序得到的用例id列表
    """
    if not os.path.isdir(folder_path):
        raise NotADirectoryError(f"文件夹不存在: {folder_path}")
    
    labels_dict = {}
    case_ids = []
    missing_field_cases = []
    empty_labels_cases = []
    
    try:
        # 遍历文件夹A下的所有子文件夹
        for subfolder_name in sorted(os.listdir(folder_path)):
            subfolder_path = os.path.join(folder_path, subfolder_name)
            
            # 跳过非文件夹
            if not os.path.isdir(subfolder_path):
                continue
            
            # 子文件夹名B作为用例id
            case_id = subfolder_name
            case_ids.append(case_id)
            
            # 构建defect_info.json的路径
            defect_info_path = os.path.join(subfolder_path, 'defect_info.json')
            
            # 如果文件不存在，记录并使用空列表
            if not os.path.exists(defect_info_path):
                labels_dict[case_id] = []
                continue
            
            try:
                # 读取JSON文件
                with open(defect_info_path, 'r', encoding='utf-8') as f:
                    defect_data = json.load(f)
                
                # 提取指定的标签字段
                if label_field_name in defect_data:
                    labels = defect_data[label_field_name]
                    # 确保标签是列表格式
                    if isinstance(labels, list):
                        # 清理每个标签
                        cleaned_labels = [clean_label_text(label) for label in labels if clean_label_text(label)]
                        labels_dict[case_id] = cleaned_labels
                    else:
                        cleaned_label = clean_label_text(labels)
                        labels_dict[case_id] = [cleaned_label] if cleaned_label else []
                else:
                    labels_dict[case_id] = []
                    missing_field_cases.append(case_id)
                
                # 记录空标签的用例
                if not labels_dict[case_id]:
                    empty_labels_cases.append(case_id)
            
            except json.JSONDecodeError as e:
                print(f"警告: {defect_info_path} JSON解析失败: {e}")
                labels_dict[case_id] = []
            except Exception as e:
                print(f"警告: 读取 {defect_info_path} 时出错: {e}")
                labels_dict[case_id] = []
        
        # # 输出统计信息
        # if missing_field_cases:
        #     print(f"警告: 以下用例的defect_info.json中不存在'{label_field_name}'字段: {missing_field_cases}")
        # if empty_labels_cases:
            # print(f"信息: 以下用例的标签列表为空: {empty_labels_cases}")
        
        return labels_dict, case_ids
    
    except Exception as e:
        raise Exception(f"读取预测标签时出错: {e}")


def get_predicted_labels_by_case_id(labels_dict, case_ids):
    """
    根据用例id列表，从标签字典中提取对应的预测标签列表
    
    Args:
        labels_dict: {用例id: [标签1, 标签2, ...]}的字典
        case_ids: 用例id列表，按顺序对应标签列表的顺序
        
    Returns:
        list: [[标签1, 标签2, ...], [...], ...]格式的标签列表
    """
    labels_list = []
    missing_cases = []
    
    for case_id in case_ids:
        case_id_str = str(case_id).strip()
        if case_id_str in labels_dict:
            labels_list.append(labels_dict[case_id_str])
        else:
            labels_list.append([])
            missing_cases.append(case_id_str)
    
    if missing_cases:
        print(f"警告: 以下用例id在预测标签中未找到: {missing_cases}")
    
    return labels_list

def calculate_multilabel_metrics(y_true_dict, y_pred_dict):
    """
    计算多标签分类的Jaccard相似度、宏平均F1、微平均F1
    :param y_true_dict: 人工标注标签字典，格式为{用例id: [标签1, 标签2], ...}
    :param y_pred_dict: 模型预测标签字典，格式与y_true_dict一致
    :return: 包含三个指标的字典
    """
    # 确保两个字典的键集合相同
    common_case_ids = set(y_true_dict.keys()) & set(y_pred_dict.keys())
    if not common_case_ids:
        raise ValueError("真实标签和预测标签没有共同的用例ID")
    
    # 按共同用例ID排序，确保顺序一致
    sorted_case_ids = sorted(common_case_ids)

    # 提取标签列表
    y_true = [y_true_dict[case_id] for case_id in sorted_case_ids]
    y_pred = [y_pred_dict[case_id] for case_id in sorted_case_ids]

    # 过滤掉真实标签为空的样本（不参与评估）
    filtered = [(t, p) for t, p in zip(y_true, y_pred) if t and p]
    if not filtered:
        print('filtered all samples due to empty true labels, cannot calculate metrics.')
        return {"Jaccard相似度": 0.0, "宏平均F1": 0.0, "微平均F1": 0.0, "样本数量": 0, "预测标签数量": 0}

    y_true, y_pred = zip(*filtered)
    sample_cnt = len(y_true)

    # 1. 初始化多标签二值化器，统一标签编码
    mlb = MultiLabelBinarizer()
    # 合并真实标签和预测标签，确保所有标签都被编码
    all_labels = list(y_true) + list(y_pred)
    mlb.fit(all_labels)

    # 将标签列表转换为二值化矩阵（样本数 × 总标签数）
    y_true_bin = mlb.transform(y_true)
    y_pred_bin = mlb.transform(y_pred)

    # 2. 计算Jaccard相似度
    jaccard_scores = []
    for true, pred in zip(y_true_bin, y_pred_bin):
        # 计算交集和并集的大小
        intersection = np.logical_and(true, pred).sum()
        union = np.logical_or(true, pred).sum()
        if union == 0 or len(true) == 0:  # 如果并集为0
            continue
        jaccard = intersection / union
        jaccard_scores.append(jaccard)
    jaccard_mean = np.mean(jaccard_scores) if jaccard_scores else 0.0
    
    # 3. 计算宏平均F1（按公式对每个标签计算F1后取平均）
    # zero_division=0：避免某个标签无预测结果时的警告
    macro_f1 = f1_score(y_true_bin, y_pred_bin, average='macro', zero_division=0)
    
    # 4. 计算微平均F1（按公式合并所有标签计算整体F1）
    micro_f1 = f1_score(y_true_bin, y_pred_bin, average='micro', zero_division=0)

    # 预测标签总数量（在评估样本范围内）
    pred_label_count = sum(len(p) for p in y_pred)
    
    # 返回结果（保留4位小数，符合实验报告精度要求）
    return {
        "Jaccard相似度": round(jaccard_mean, 4),
        "宏平均F1": round(macro_f1, 4),
        "微平均F1": round(micro_f1, 4),
        "样本数量": sample_cnt,
        "预测标签数量": pred_label_count
    }


def calculate_comprehensive_metrics(datasets_config):
    """
    计算多个数据集的综合指标
    
    Args:
        datasets_config: 数据集配置列表，每个元素为dict包含:
            - excel_path: Excel文件路径
            - output_folder: 输出文件夹路径  
            - label_field_name: 模型标签字段名
            - sheet_name: (可选) sheet索引，默认0
            - name: (可选) 数据集名称，用于输出展示
            
    Returns:
        dict: {
            "individual": [数据集1指标, 数据集2指标, ...],
            "comprehensive": 综合指标,
            "dataset_names": [数据集1名称, 数据集2名称, ...]
        }
    """
    all_y_true_dict = {}  # 汇总所有真实标签
    all_y_pred_dict = {}  # 汇总所有预测标签
    individual_metrics = []
    dataset_names = []
    
    for dataset in datasets_config:
        excel_path = dataset["excel_path"]
        output_folder = dataset["output_folder"]
        label_field_name = dataset["label_field_name"]
        sheet_name = dataset.get("sheet_name", 0)
        dataset_name = dataset.get("name", os.path.basename(output_folder))
        
        try:
            # 读取真实标签
            labels_dict = read_labels_from_excel(excel_path, sheet_name)
            
            # 读取预测标签
            pred_labels_dict, case_ids = read_predicted_labels_from_folder(
                output_folder,
                label_field_name=label_field_name
            )
            
            # 匹配标签
            y_true_dict = match_labels_by_case_id(labels_dict, case_ids)
            y_pred_dict = pred_labels_dict
            
            # 清理标签
            y_true_dict = clean_labels_dict(y_true_dict)
            y_pred_dict = clean_labels_dict(y_pred_dict)
            
            # 计算单个数据集指标
            metrics = calculate_multilabel_metrics(y_true_dict, y_pred_dict)
            individual_metrics.append(metrics)
            dataset_names.append(dataset_name)
            
            # 汇总到全局字典
            all_y_true_dict.update(y_true_dict)
            all_y_pred_dict.update(y_pred_dict)
            
        except Exception as e:
            print(f"❌ 处理数据集 {dataset_name} 失败: {e}")
            continue
    
    # 计算综合指标
    comprehensive_metrics = calculate_multilabel_metrics(all_y_true_dict, all_y_pred_dict)
    
    return {
        "individual": individual_metrics,
        "comprehensive": comprehensive_metrics,
        "dataset_names": dataset_names
    }


def save_metrics_to_csv(result, output_path):
    """
    将计算结果保存到CSV文件
    
    Args:
        result: calculate_comprehensive_metrics的返回值
        output_path: CSV文件输出路径
    """
    data = []
    
    # 添加单个数据集指标
    for name, metrics in zip(result["dataset_names"], result["individual"]):
        data.append({
            "数据集": name,
            "Jaccard相似度": metrics["Jaccard相似度"],
            "宏平均F1": metrics["宏平均F1"],
            "微平均F1": metrics["微平均F1"],
            "样本数量": metrics["样本数量"],
            "预测标签数量": metrics.get("预测标签数量", 0)
        })
    
    # 添加综合指标
    comprehensive = result["comprehensive"]
    data.append({
        "数据集": "综合",
        "Jaccard相似度": comprehensive["Jaccard相似度"],
        "宏平均F1": comprehensive["宏平均F1"],
        "微平均F1": comprehensive["微平均F1"],
        "样本数量": comprehensive["样本数量"],
        "预测标签数量": comprehensive.get("预测标签数量", 0)
    })
    
    df = pd.DataFrame(data)
    df.to_csv(output_path, index=False, encoding='utf-8-sig')
    print(f"结果已保存到: {output_path}")


def train_set():
        # ==================== 训练集综合指标统计 ====================
    print("\n" + "=" * 80)
    print("训练集指标计算结果")
    print("=" * 80)
    
    datasets_config = [
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练数据集_72.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/itc-cases",
            "label_field_name": "qwen3.5-122b-a10b_labels",
            "name": "训练集 (itc-cases)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练数据集_72.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/redis-8.2.2",
            "label_field_name": "qwen3.5-122b-a10b_labels",
            "name": "训练集 (redis-8.2.2)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练数据集_72.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/bzip3-1.4.0",
            "label_field_name": "qwen3.5-122b-a10b_labels",
            "name": "训练集 (bzip3-1.4.0)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练数据集_72.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/jq-1.8.1",
            "label_field_name": "qwen3.5-122b-a10b_labels",
            "name": "训练集 (jq-1.8.1)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练数据集_72.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/cpython-3.10.17",
            "label_field_name": "qwen3.5-122b-a10b_labels",
            "name": "训练集 (cpython-3.10.17)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练数据集_72.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/libusb-1.0.27",
            "label_field_name": "qwen3.5-122b-a10b_labels",
            "name": "训练集 (libusb-1.0.27)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练数据集_72.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/libpng-libpng16",
            "label_field_name": "qwen3.5-122b-a10b_labels",
            "name": "训练集 (libpng-libpng16)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练数据集_72.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/libevent-2.1.12",
            "label_field_name": "qwen3.5-122b-a10b_labels",
            "name": "训练集 (libevent-2.1.12)"
        }
    ]
    
    result = calculate_comprehensive_metrics(datasets_config)
    
    print("\n【单个数据集指标】")
    for i, (name, metrics) in enumerate(zip(result["dataset_names"], result["individual"])):
        print(f"\n  {i+1}. {name}:")
        for metric, value in metrics.items():
            print(f"     {metric}: {value}")
    
    print("\n【训练集综合指标】")
    comprehensive = result["comprehensive"]
    for metric, value in comprehensive.items():
        print(f"  {metric}: {value}")
    
    # 保存训练集结果到CSV
    save_metrics_to_csv(result, "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/训练集指标.csv")

def test_set():
    

    # ==================== 测试集综合指标统计 ====================
    print("\n" + "=" * 80)
    print("测试集指标计算结果")
    print("=" * 80)
    # label_version ='qwen3-max-2026-01-23_labels'
    # label_version ='deepseek-r1_labels'
    # label_version ='qwen3.5-122b-a10b_labels'
    # label_version ='qwen3-max-2026-01-23_thinking_labels'
    label_version ='qwen3.5-122b-a10b_NOTASK_labels'
    datasets_config = [
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试数据集_48.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/itc-cases",
            "label_field_name": label_version,
            "name": "测试集（itc-cases)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试数据集_48.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/redis-8.2.2",
            "label_field_name": label_version,
            "name": "测试集（redis-8.2.2)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试数据集_48.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/bzip3-1.4.0",
            "label_field_name": label_version,
            "name": "测试集（bzip3-1.4.0)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试数据集_48.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/jq-1.8.1",
            "label_field_name": label_version,
            "name": "测试集（jq-1.8.1)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试数据集_48.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/cpython-3.10.17",
            "label_field_name": label_version,
            "name": "测试集（cpython-3.10.17)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试数据集_48.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/libusb-1.0.27",
            "label_field_name": label_version,
            "name": "测试集（libusb-1.0.27)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试数据集_48.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/libpng-libpng16",
            "label_field_name": label_version,
            "name": "测试集（libpng-libpng16)"
        },
        {
            "excel_path": "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试数据集_48.xlsx",
            "output_folder": "/home/nishikino/new_version_benchmarks/output_update/libevent-2.1.12",
            "label_field_name": label_version,
            "name": "测试集（libevent-2.1.12)"
        }
    ]
    
    result = calculate_comprehensive_metrics(datasets_config)
    
    print("\n【单个数据集指标】")
    for i, (name, metrics) in enumerate(zip(result["dataset_names"], result["individual"])):
        print(f"\n  {i+1}. {name}:")
        for metric, value in metrics.items():
            print(f"     {metric}: {value}")
    
    print("\n【测试集综合指标】")
    comprehensive = result["comprehensive"]
    for metric, value in comprehensive.items():
        print(f"  {metric}: {value}")
    
    # 保存测试集结果到CSV
    save_metrics_to_csv(result, "/home/nishikino/new_version_benchmarks/defect_classfier/scripts/测试集指标.csv")
if __name__ == "__main__":
    # train_set()
    test_set()

