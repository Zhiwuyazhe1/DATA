import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns

df = pd.read_csv('./output/2025_06_14_0/compare.csv')

taint_data = {
  'same' : df['taint_same'].values,
  'score' : df['taint_score'].values
}
memory_data = {
  'same' : df['memory_same'].values,
  'score' : df['memory_score'].values
}

def draw_test(data, name):
  fig, ax = plt.subplots(figsize=(12, 7))

  # 使用 seaborn 的 histplot 函数
  # x: 指定x轴数据
  # hue: 指定用于分类颜色的列
  # multiple='stack': 指定将不同分类的条形堆叠起来
  # bins: 指定分箱数量，与之前的示例保持一致
  # palette: 自定义颜色
  # hue_order: 确保'正确'在下，'错误'在上
  result_map = {1: 'correct', 0: 'error'}
  data['same'] = [result_map[item] for item in data['same']]
  sns.histplot(
      data=data,
      x='score',
      hue='same',
      multiple='stack',
      bins=20,
      palette={'correct': '#2ca02c', 'error': '#d62728'},
      ax=ax,
      edgecolor='white',
      hue_order=['error', 'correct']
  )

  # 4. 添加图表元素
  # ax.set_title('不同置信度下的结果分布', fontsize=16)
  path_svg = f'{name}_pic.svg'
  plt.savefig(path_svg)
  plt.close()
  print(f"图像已保存为: {path_svg}")

draw_test(taint_data, 'taint')
draw_test(memory_data, 'memory')