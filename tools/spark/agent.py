"""
SPARK - Smart Platform for Automotive Rapid Kit
AI-powered Agent for AUTOSAR configuration and development.
"""

import json
import os
import logging
from datetime import datetime
import openai

log_dir = os.path.join(os.path.dirname(__file__), "logs")
os.makedirs(log_dir, exist_ok=True)

formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")


class AgentLogFilter(logging.Filter):
    def filter(self, record):
        return record.name == "spark.agent"


logger = logging.getLogger("spark.agent")
logger.setLevel(logging.DEBUG)
logger.propagate = False

agent_file_handler = logging.FileHandler(os.path.join(log_dir, "spark.log"), encoding="utf-8")
agent_file_handler.setFormatter(formatter)
agent_file_handler.addFilter(AgentLogFilter())
logger.addHandler(agent_file_handler)

# stream_handler = logging.StreamHandler()
# stream_handler.setFormatter(formatter)
# logger.addHandler(stream_handler)

misc_logger = logging.getLogger()
misc_logger.setLevel(logging.DEBUG)
misc_file_handler = logging.FileHandler(os.path.join(log_dir, "spark.misc.log"), encoding="utf-8")
misc_file_handler.setFormatter(formatter)
misc_logger.addHandler(misc_file_handler)


class SparkMsgMgr:
    def __init__(self):
        self._messages = []

    def messages(self):
        return self._messages

    def append(self, msg):
        logger.debug(msg)
        self._messages.append(msg)
        return self

    def __iadd__(self, msgs):
        for msg in msgs:
            logger.debug(msg)
        self._messages.extend(msgs)
        return self

    def __repr__(self):
        return f"SparkMsgMgr({self._messages})"

    def clear(self):
        self._messages.clear()
        return self

    def extend(self, msgs):
        for msg in msgs:
            logger.debug(msg)
        self._messages.extend(msgs)
        return self


class SparkAgent:
    DEFAULT_MODELS = [
        ("Qwen/Qwen3.5-35B-A3B", "Qwen3.5-35B (Balanced - Recommended)"),
        ("Qwen2-72B-Instruct", "Qwen2-72B (High Performance)"),
        ("deepseek-ai/DeepSeek-V4-Flash", "DeepSeek-V4-Flash (158B)"),
        ("deepseek-ai/DeepSeek-V3.2", "DeepSeek-V3.2 (685B - Top Performance)"),
        ("deepseek-ai/DeepSeek-R1-Distill-Qwen-1.5B", "DeepSeek-R1-Distill (1.5B - Fastest)"),
        ("ZhipuAI/GLM-4-Plus", "GLM-4-Plus (8B - Fast Chinese)"),
    ]

    def __init__(self, apiKey: str = None, baseUrl: str = None, model: str = None):
        from .config import SettingsManager
        from .prompt_manager import PromptManager

        self.promptManager = PromptManager()

        settings = SettingsManager.getInstance()
        llmSettings = settings.getLLM()

        finalApiKey = apiKey or llmSettings.get("api_key")
        finalBaseUrl = baseUrl or llmSettings.get("base_url") or "https://api-inference.modelscope.cn/v1/"
        self.model = model or llmSettings.get("model") or "Qwen/Qwen3.5-35B-A3B"
        self.baseUrl = finalBaseUrl
        self.apiKey = finalApiKey
        self.maxTokens = llmSettings.get("max_tokens", 8192)
        self.maxContinuations = llmSettings.get("max_continuations", 128)
        self.timeout = llmSettings.get("timeout", 300)

        if self.apiKey:
            self.client = openai.OpenAI(api_key=self.apiKey, base_url=self.baseUrl, timeout=self.timeout)
        else:
            self.client = None
            logger.warning("No API key configured. LLM features will be disabled.")

        self.systemPrompt = self.promptManager.getSystemPrompt()
        self.msgMgr = SparkMsgMgr()
        self.msgMgr.append({"role": "system", "content": self.systemPrompt})

    def setModel(self, modelName: str):
        self.model = modelName

    def getAvailableModels(self):
        return self.DEFAULT_MODELS

    def _executeTool(self, toolCall) -> dict:
        from .tools import SparkTools

        toolName = toolCall.get("name")
        parameters = toolCall.get("parameters", {})
        logger.info(f"Executing tool: {toolName} with parameters: {parameters}")
        result = SparkTools.executeTool(toolName, parameters)
        reason = toolCall.get("reason", "")

        role = "user"
        # Skill prompt: inject directly as system message for higher authority
        if toolName == "read" and result.get("success"):
            file_path = parameters.get("file_path", "")
            if self.promptManager.isSkillPrompt(file_path):
                role = "system"

        toolResult = {"tool_name": toolName, "parameters": parameters, "result": result, "reason": reason}

        toolCallRes = {
            "role": role,
            "content": f"## TOOL CALL:\n\n### Name:\n{toolName}\n### Parameters:\n{json.dumps(parameters, indent=2, ensure_ascii=False)}\n"
            + f"## TOOL RESULTS:\n\n### Purpose:\n{reason}\n"
            + "\n### Results:\n"
            + f"```json\n{json.dumps(toolResult, indent=2, ensure_ascii=False)}\n```\n",
        }
        self.msgMgr.append(toolCallRes)

    def _callLLM(self, messages: list) -> openai.types.chat.ChatCompletion:
        response = self.client.chat.completions.create(
            model=self.model,
            messages=messages,
            temperature=0.3,
            max_tokens=self.maxTokens,
            response_format={"type": "text"},
            extra_body={"thinking": {"type": "disabled"}},
        )
        return response

    def _chatOnce(self) -> tuple:
        response = self._callLLM(self.msgMgr.messages())
        answer = response.choices[0].message.content or ""
        finishReason = response.choices[0].finish_reason
        usage = response.usage
        logger.debug(f"LLM response: {finishReason}, usage: {usage}")
        preview = answer[:80].replace("\n", "").replace("\r", "")
        print(f"\r{preview:80} ..., finish: {finishReason}, tokens: {usage.total_tokens}", end="")

        if finishReason == "length" and self.maxContinuations > 0:
            answer = self._continueResponse(answer)

        jsAns = SparkAgent._fixJsonControlChars(answer)
        self.msgMgr.append({"role": "assistant", "content": jsAns})

        return jsAns, finishReason, usage

    def chat(self, user_message: str) -> str:
        if not self.client:
            return "LLM service is not available. Please configure your API key."

        self.msgMgr.append({"role": "user", "content": user_message})

        answer, finishReason, usage = self._chatOnce()

        while True:
            logger.debug(f"Trying to parse tool calls from answer (first 500 chars): {answer[:500]}")
            toolCalls = self._parseToolCalls(answer)
            if toolCalls:
                logger.debug(f"Parsed {len(toolCalls)} tool calls: {[tc['name'] for tc in toolCalls]}")
            else:
                logger.debug("No tool calls parsed from response")
            if not toolCalls:
                break

            logger.debug("Found tool calls in response, executing...")
            answer = self._handleToolCalls(user_message, toolCalls)

        return answer

    @staticmethod
    def _fixJsonControlChars(text: str) -> str:
        fixed = []
        inString = False
        escape = False
        for c in text:
            if escape:
                fixed.append(c)
                escape = False
            elif c == "\\":
                fixed.append(c)
                escape = True
            elif c == '"':
                inString = not inString
                fixed.append(c)
            elif inString:
                if c == "\n":
                    fixed.append("\\n")
                elif c == "\r":
                    fixed.append("\\r")
                elif c == "\t":
                    fixed.append("\\t")
                else:
                    fixed.append(c)
            else:
                fixed.append(c)
        return "".join(fixed)

    @staticmethod
    def _extractJson(text: str) -> str:
        fixedText = SparkAgent._fixJsonControlChars(text)

        start = fixedText.find("{")
        if start == -1:
            return None

        depth = 0
        inString = False
        escape = False
        for i in range(start, len(fixedText)):
            c = fixedText[i]
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                inString = not inString
            elif not inString:
                if c == "{":
                    depth += 1
                elif c == "}":
                    depth -= 1
                    if depth == 0:
                        return fixedText[start : i + 1]
        return None

    @staticmethod
    def _parseToolCalls(answer: str) -> list:
        try:
            parsed = json.loads(answer)
            if isinstance(parsed, dict) and parsed.get("type") == "tool_call":
                calls = parsed.get("tool_calls", [])
                if isinstance(calls, list) and calls:
                    return calls
        except json.JSONDecodeError:
            pass

        jsonStr = SparkAgent._extractJson(answer)
        if jsonStr:
            try:
                parsed = json.loads(jsonStr)
                if isinstance(parsed, dict) and parsed.get("type") == "tool_call":
                    calls = parsed.get("tool_calls", [])
                    if isinstance(calls, list) and calls:
                        return calls
            except (json.JSONDecodeError, Exception):
                pass
        return []

    def _handleToolCalls(self, user_message: str, toolCalls: list) -> str:
        for tc in toolCalls:
            self._executeTool(tc)
        msg = {
            "role": "system",
            "content": "Now provide the final response based on the tool results. Follow the instructions from loaded skills carefully.",
        }

        self.msgMgr.append(msg)

        answer, finishReason, usage = self._chatOnce()

        return answer

    def _continueResponse(self, firstChunk: str) -> str:
        baseMessages = self.msgMgr.messages()
        fullResponse = firstChunk

        for i in range(self.maxContinuations):
            tail = fullResponse[-300:]
            currentMessages = baseMessages + [
                {"role": "assistant", "content": fullResponse},
                {
                    "role": "user",
                    "content": (
                        "The assistant response above is INCOMPLETE - it was "
                        "cut off mid-output (max_tokens limit).\n\n"
                        "Your task: continue the JSON from the exact point "
                        "where it was cut off. Do NOT restart from the "
                        "beginning. Do NOT repeat any text that already "
                        "appears above.\n\n"
                        f"The last part of the incomplete output was:\n"
                        f"```\n...{tail}\n```\n\n"
                        "Continue directly from that point, outputting ONLY "
                        "the missing remaining content. No preamble, no "
                        "repetition."
                    ),
                },
            ]

            response = self._callLLM(currentMessages)

            chunk = response.choices[0].message.content
            finishReason = response.choices[0].finish_reason
            usage = response.usage

            fullResponse += chunk

            if finishReason != "length":
                break

        return fullResponse
