import pandas as pd
import numpy as np

df = pd.read_csv('./output/2025_10_21_1/compare.csv')

taint_data = {
  'same' : df['taint_same'].values,
  'score' : df['taint_score'].values,
  'tp' : df['taint_tp'].values,
  'fp' : df['taint_fp'].values,
  'fn' : df['taint_fn'].values
}
memory_data = {
  'same' : df['memory_same'].values,
  'score' : df['memory_score'].values,
  'tp' : df['memory_tp'].values,
  'fp' : df['memory_fp'].values,
  'fn' : df['memory_fn'].values
}

def compute_result(threshold, data, name):
  # 只统计置信度得分 大于等于 阈值 摘要的 precision, recall, f1 和 accuracy
  total_tp = 0
  total_fp = 0
  total_fn = 0
  total_same = 0
  total_count = 0
  for same, score, tp, fp, fn in zip(data['same'], data['score'], data['tp'], data['fp'], data['fn']):
    if score >= threshold:
      total_tp += tp
      total_fp += fp
      total_fn += fn
      total_same += same
      total_count += 1
  
  precision = total_tp / (total_tp + total_fp) if (total_tp + total_fp) > 0 else 0
  recall = total_tp / (total_tp + total_fn) if (total_tp + total_fn) > 0 else 0
  f1 = 2 * (precision * recall) / (precision + recall) if (precision + recall) > 0 else 0
  accuracy = total_same / total_count if total_count > 0 else 0
  print(f"{name} - Threshold: {threshold:.2f}, Precision: {precision:.4f}, Recall: {recall:.4f}, F1: {f1:.4f}, Accuracy: {accuracy:.4f}, Same: {total_same}, Count: {total_count}")


threshold = 0.75
compute_result(threshold, taint_data, 'taint')
compute_result(threshold, memory_data, 'memory')

# 筛选出 taint_score >= threshold 的行
taint_filtered_df = df[df['taint_score'] >= threshold]

# 筛选出 memory_score >= threshold 的行
memory_filtered_df = df[df['memory_score'] >= threshold]

# 导出筛选后的数据到新的 CSV 文件
taint_filtered_df.to_csv('./output/2025_10_21_1/taint_filtered.csv', index=False)
memory_filtered_df.to_csv('./output/2025_10_21_1/memory_filtered.csv', index=False)

print("\n数据已成功保存：")
print(f"  - 筛选后的 taint 数据已保存到：./output/2025_09_29_0/taint_filtered.csv")
print(f"  - 筛选后的 memory 数据已保存到：./output/2025_09_29_0/memory_filtered.csv")