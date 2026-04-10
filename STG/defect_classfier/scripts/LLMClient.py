import os
import json
import ast
import logging
from http import HTTPStatus
from dashscope import Generation, MultiModalConversation
from typing import Optional, Dict, Tuple, Any
from tenacity import (
    retry, stop_after_attempt, wait_exponential,
    retry_if_exception_type, retry_if_result, RetryError
)
from requests.exceptions import (
    ReadTimeout, ConnectTimeout, SSLError, ConnectionError as RequestsConnectionError
)

logger = logging.getLogger(__name__)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)


class LLMClient:
    api_key = os.getenv("DASHSCOPE_API_KEY")  # API密钥(类变量)
    LLM_VERSION = "qwen3.5-122b-a10b" # 基准
    #! qwen 3.5+模型需要使用多模态调用！！

    def __init__(self, system_prompt):
        self.system_prompt = system_prompt
        # 校验API密钥
        if not self.api_key:
            logger.error("未配置DASHSCOPE_API_KEY环境变量！")
            raise ValueError("DASHSCOPE_API_KEY is required")

    # @staticmethod  # 静态方法，避免实例调用报错
    def is_retryable_result(result):
        """判断结果是否需要重试"""
        try:
            response_text, status_code = result
            # 重试条件：5xx状态码 或 超时相关错误文本
            retry_conditions = [
                status_code >= 500,
                "Read timed out" in response_text,
                "timeout" in response_text.lower(),
                "服务暂时不可用" in response_text,
                "限流" in response_text
            ]
            need_retry = any(retry_conditions)
            if need_retry:
                logger.warning(f"满足重试条件 | 状态码={status_code} | 响应文本={response_text[:100]}")
            return need_retry
        except Exception as e:
            logger.error(f"解析重试结果失败: {e}")
            return False

    @retry(
        stop=stop_after_attempt(5),  # 最多重试5次
        wait=wait_exponential(multiplier=1, min=2, max=15),  # 最长等待15秒
        retry=(
            retry_if_exception_type((ConnectTimeout,
                                   ReadTimeout,
                                   RequestsConnectionError,
                                   SSLError)) |
            retry_if_result(is_retryable_result)
        ),
        reraise=True,  # 最终重试失败时重新抛出异常
        # 重试日志：打印每次重试原因
        before_sleep=lambda rs: logger.warning(
            f"LLM调用重试 | 第{rs.attempt_number}次 | 原因: {rs.outcome.result() if rs.outcome else rs.exception}"
        )
    )
    def callLLM(self, prompt: str,
                parameters: Optional[Dict[str, Any]] = None) -> Tuple[str, int]:
        """
        调用指定的LLM应用

        参数:
            prompt: 输入提示
            parameters: 可选的额外参数

        返回:
            Tuple[LLM的响应文本, HTTP状态码]
        """
        # logger.info(f"开始调用LLM | model={LLMClient.LLM_VERSION} | prompt长度={len(prompt)} | parameters={parameters}")
        try:

            messages = [
                {
                    'role': 'system',
                    'content': [
                        {
                            "type": "text",
                            "text": self.system_prompt, 
                            "cache_control": {"type": "ephemeral"},
                        }
                    ],
                },
                {
                    'role': 'user',
                    'content': prompt
                }
            ]
            # logger.debug(f"构造的请求messages | system_prompt长度={len(self.system_prompt)} | user_prompt长度={len(prompt)}")

            if '3.5' in LLMClient.LLM_VERSION:
                response = MultiModalConversation.call(
                    api_key=LLMClient.api_key,
                    model=LLMClient.LLM_VERSION,
                    messages=messages,
                    parameters=parameters
                )
            else: 
                response = Generation.call(
                    api_key=LLMClient.api_key,
                    model=LLMClient.LLM_VERSION,
                    messages=messages,
                    result_format='message',
                    # enable_thinking=True, # 控制是否启用深度思考
                    parameters=parameters
                )
            # logger.info(f"LLM API响应状态 | status_code={response.status_code} ")

            # 检查响应状态
            if response.status_code == HTTPStatus.OK:
                result = ""
                if hasattr(response, 'output'):
                    output = response.output
                    if 'choices' in output and len(output['choices']) > 0:
                        result = output['choices'][0]['message']['content']
                        # print(output)
                        if '3.5' in LLMClient.LLM_VERSION:
                            result = output.choices[0].message.content[0]['text']
                    elif 'text' in output:
                        result = output['text']
                    else:
                        result = str(output)
                elif isinstance(getattr(response, 'result', None), str):
                    result = response.result
                else:
                    result = str(response)

                # logger.info(f"LLM调用成功 | 响应结果长度={len(result)} | 响应内容预览={result}")
                return result, response.status_code
            else:
                error_msg = f"API调用失败: {getattr(response, 'message', '未知错误')}"
                logger.error(f"LLM API返回非200状态 | status_code={response.status_code} | 错误信息={error_msg}")
                return error_msg, response.status_code

        except Exception as e:
            error_msg = f"发生错误: {str(e)}"
            error_type = type(e).__name__
            logger.error(f"LLM调用抛出异常 | 异常类型={error_type} | 异常信息={error_msg}", exc_info=True)
            # 异常时返回500状态码，触发重试判断
            return error_msg, HTTPStatus.INTERNAL_SERVER_ERROR


    def clean_json_string(self, text: str) -> dict:
        """清理JSON字符串，处理常见的非标准格式问题"""
        # 移除可能的JSON前缀/后缀文本
        # 例如："```json\n{\\"name\\":\\"张三\\"}\n```" 或 "JSON响应: {...}"
        text = text.strip()

        # 检查是否有常见的代码块标记
        json_start = None
        json_end = None

        # 检查 ```json 前缀
        if text.startswith("```json"):
            json_start = text.find("{")
            json_end = text.rfind("}") + 1
            if json_start != -1 and json_end != 0:
                text = text[json_start:json_end]

        # 检查普通的JSON对象边界
        elif text.startswith("{") and text.endswith("}"):
            pass  # 已经是有效的JSON对象
        elif text.startswith("[") and text.endswith("]"):
            pass  # 已经是有效的JSON数组
        else:
            # 尝试提取第一个完整的JSON对象
            try:
                # 查找第一个 '{' 和最后一个 '}'
                start_idx = text.find("{")
                end_idx = text.rfind("}")
                if start_idx != -1 and end_idx != -1 and end_idx > start_idx:
                    text = text[start_idx:end_idx + 1]
            except:
                pass

        # 处理转义字符问题
        try:
            # 尝试修复可能的转义问题
            text = text.replace("\\'", "'")
            text = text.replace('\\"', '"')
        except:
            pass

        # 先尝试JSON解析
        try:
            return json.loads(text)
        except (json.JSONDecodeError, ValueError):
            # 如果JSON解析失败，尝试使用 ast.literal_eval 解析 Python dict 格式
            # 这处理形如 {'key': 'value'} 的字符串（单引号）
            try:
                return ast.literal_eval(text)
            except (ValueError, SyntaxError):
                raise json.JSONDecodeError(f"无法解析为JSON或Python dict格式", text, 0)

    def call_feature_generator(self, prompt: str,
                               parameters: Optional[Dict[str, Any]] = None) -> dict:
        response_data, status_code = self.callLLM(
            prompt=prompt,
            parameters=parameters
        )
        if status_code != HTTPStatus.OK:
            return {}
        # 尝试解析JSON
        try:
            output_feature = self.clean_json_string(response_data)
            return output_feature
        except (json.JSONDecodeError, TypeError, ValueError) as e:
            logger.warning(f'WARNING: 响应内容无法解析为JSON | 错误: {e.__class__.__name__} | 响应: {response_data[:200]}')
        return {}


# 使用示例
if __name__ == "__main__":
    from PromptGenerator import SYSTEM_PROMPT
    # 初始化客户端
    try:
        client = LLMClient(SYSTEM_PROMPT)
        prompt = '''
'''
        # 调用LLM
        response_data, status_code = client.callLLM(
            prompt=prompt,
            parameters={
                "temperature": 0.5,
                "max_tokens": 1024,
                "enable_thinking": True  # 启用思考链功能（如果模型支持）
            }
        )
        print(f"\n===== 调用结果 =====")
        print(f"状态码: {status_code}")
        print(f"响应类型: {type(response_data).__name__}")
        print(f"响应内容: {response_data}")

        # 解析JSON
        print(f"\n===== JSON解析结果 =====")
        cleaned_result = client.clean_json_string(response_data)
        if cleaned_result:
            print(cleaned_result)
        else:
            print("JSON解析失败，非标准JSON格式")

    except ValueError as e:
        logger.error(f"客户端初始化失败: {e}")
    except Exception as e:
        logger.error(f"运行出错: {e}", exc_info=True)