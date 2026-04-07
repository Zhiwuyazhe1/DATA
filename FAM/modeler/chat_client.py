import os
import json
import time
import httpx
from openai import OpenAI
from openai import APIStatusError, APITimeoutError, APIConnectionError, APIError 

# 封装的opanai client框架，提供基础prompt
# 使用时需要填充其他prompt
class LLMClient:
  def __init__(self, config):
    self.api_key = config['api_key']
    self.base_url = config['base_url']
    self.model = config['model']
    self.base_prompt = config['base_prompt']
    self.max_retries = 2
    self.client = OpenAI(
      base_url = self.base_url,
      api_key = self.api_key,
      http_client = httpx.Client(
        base_url = self.base_url,
        follow_redirects=True,
      ),
    )
    self.messages = []

  def add_system_message(self, content):
    self.messages.append({'role' : 'system', 'content' : content})
  def add_user_message(self, content):
    self.messages.append({'role' : 'user', 'content' : content})
  def remove_all_messages(self):
    self.messages = []

  # 输出模型的回复，只是从json格式中提取出文本内容，并不进行额外处理
  def get_reply(self):
    retry_count = 1
    while retry_count <= self.max_retries:
      try:
        completion = self.client.chat.completions.create(
          model = self.model,
          messages = self.messages,
          stream = True,
          timeout= 60.0 * retry_count
        )
        full_response = []
        for chunk in completion:
          if chunk.choices:
            choice = chunk.choices[0]
            if choice.delta and choice.delta.content is not None:
              chunk_content = choice.delta.content
              full_response.append(chunk_content)
            # if chunk.choices[0].delta.content is not None:
            #   chunk_content = chunk.choices[0].delta.content
            #   full_response.append(chunk_content)
        return "".join(full_response) 
      except Exception as e:
        print("请求失败,将于10s后重试")
        print(e)
        time.sleep(10)
        retry_count += 1
        continue
    return 'request error!'
  
  # import time

  # def get_reply(self):
  #   retry_count = 1
  #   while retry_count <= self.max_retries:
  #       try:
  #           completion = self.client.chat.completions.create(
  #               model=self.model,
  #               messages=self.messages,
  #               stream=True,
  #               timeout=60.0 * retry_count 
  #           )
  #           full_response = []
  #           for chunk in completion:
  #             content = chunk.choices[0].delta.content
  #             if content:
  #               full_response.append(content)
          
  #           return "".join(full_response)
        
  #       except (APITimeoutError, APIConnectionError) as e:
  #           print(f"请求超时或连接错误 ({e.__class__.__name__})，将在 10s 后进行第 {retry_count + 1} 次重试...")
  #           print(e)
  #           time.sleep(10)
  #           retry_count += 1
  #           continue

  #       # 5. 捕获非重试的异常（模型/参数错误、速率限制等）
  #       except (APIError, APIStatusError) as e:
  #           # 这些通常是代码或配置错误，重试无用
  #           print(f"致命 API 错误 ({e.__class__.__name__})，无法重试，错误详情：")
  #           print(e)
  #           return 'request error: fatal API error!'
            
  #       # 6. 捕获其他所有未预期的异常
  #       except Exception as e:
  #           print(f"发生未知错误 ({e.__class__.__name__})，将在 10s 后进行第 {retry_count + 1} 次重试...")
  #           print(e)
  #           time.sleep(10)
  #           retry_count += 1
  #           continue

  #   # 7. 达到最大重试次数
  #   return 'request error: max retries reached!'
      
    #   response_json = completion.model_dump()
    #   if 'choices' in response_json and len(response_json['choices']) > 0:
    #     return response_json['choices'][0]['message']['content']
    #   else:
    #     print("模型请求失败")
    #     return 'request error!'