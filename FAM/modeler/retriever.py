from rank_bm25 import BM25Okapi
from sentence_transformers import SentenceTransformer, util
import torch
import pandas as pd
import jieba

# 用于检索的类， 目前仅使用BM25算法, 后续考虑引入向量嵌入等方法
class Retriever:
  def __init__(self, file):
    self.datas = pd.read_csv(file)
    docs = [self.build_a_doc(row) for index, row in self.datas.iterrows()]
    tokenized_docs = [jieba.lcut(doc) for doc in docs]
    self.bm25 = BM25Okapi(tokenized_docs)

  def build_a_doc(self, row):
    doc = ''
    # if not pd.isna(row['namespace']):
    #   doc += row['namespace'] + ' '
    if not pd.isna(row['class']):
      doc += row['class'] + ' '
    if not pd.isna(row['func']):
      doc += row['func'] + ' '
    if not pd.isna(row['docstring']):
      doc += row['docstring']
    return doc
    
  def retrieve(self, query, k = 10):
    tokenized_query = jieba.lcut(self.build_a_doc(query))
    scores = self.bm25.get_scores(tokenized_query)
    top_indices = sorted(range(len(scores)), key=lambda i: scores[i], reverse=True)[:k]
    return [self.datas.iloc[i] for i in top_indices]
  
  def load_transformer(self, model_name='sentence-transformers/paraphrase-multilingual-MiniLM-L12-v2'):
    self.model = None
    print('Loading model:', model_name)
    try:
      self.model = SentenceTransformer(model_name)
    except Exception as e:
      print('Error loading model:', e)
      exit(1)
    if torch.cuda.is_available():
      self.model = self.model.to('cuda')
    print('Model loaded successfully.')
  
  def get_embedding(self, text):
    if self.model is None:
      print('Model not loaded. Please load the model first.')
      return None
    try:
      embedding = self.model.encode(text, convert_to_tensor=True, show_progress_bar=False)
      return embedding
    except Exception as e:
      print('Error generating embedding:', e)
      return None
  
  def get_cos_sim(self, query_embedding, doc_embedding):
    if query_embedding is None or doc_embedding is None:
      print('One of the embeddings is None. Cannot compute cosine similarity.')
      return 0.0
    try:
      return util.pytorch_cos_sim(query_embedding, doc_embedding).item()
    except Exception as e:
      print('Error computing cosine similarity:', e)
      return 0.0


# # used for testing
# if __name__ == '__main__':
#   retriever = Retriever('../examples/vector_v4.csv')
#   query = {
#     'namespace': 'std::list<T,Allocator>::list',
#     'class': 'list',
#     'func': 'list() : list(Allocator()) {}',
#     'docstring': 'C++11 起的默认构造函数。构造拥有默认构造的分配器的空 list。参数 alloc	-	用于此容器所有内存分配的分配器'
#   }
#   # print(retriever.retrieve(query))
#   retriever.load_transformer()
#   query_doc = ''
#   for key, value in query.items():
#     if not pd.isna(value):
#       query_doc += value + ' '
#   query_doc = query_doc.strip()
#   print('Query Document:', query_doc)
#   query_embedding = retriever.get_embedding(query_doc)
#   for row in retriever.retrieve(query, 50):
#     embedding = retriever.get_embedding(retriever.build_a_doc(row))
#     print(retriever.get_cos_sim(query_embedding, embedding))
  