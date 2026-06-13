# SSAS - Simple Smart Automotive Software
# Copyright (C) 2015 ~ 2023 Parai Wang <parai@foxmail.com>
# OpenAI-powered JSON Configuration Assistant

import json
import os
import logging
from typing import List, Dict
import openai

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    handlers=[logging.StreamHandler()],
)
logger = logging.getLogger(__name__)


class OpenAIAgent:
    DEFAULT_MODELS = [
        ("Qwen/Qwen3.5-35B-A3B", "Qwen3.5-35B (Balanced - Recommended)"),
        ("Qwen/Qwen2-72B-Instruct", "Qwen2-72B (High Performance)"),
        ("deepseek-ai/DeepSeek-V4-Flash", "DeepSeek-V4-Flash (158B)"),
        ("deepseek-ai/DeepSeek-V3.2", "DeepSeek-V3.2 (685B - Top Performance)"),
        ("deepseek-ai/DeepSeek-R1-Distill-Qwen-1.5B", "DeepSeek-R1-Distill (1.5B - Fastest)"),
        ("ZhipuAI/GLM-4-Plus", "GLM-4-Plus (8B - Fast Chinese)"),
    ]

    def __init__(self, schema_file: str, api_key: str = None, base_url: str = None, model: str = None):
        self.schema = self._load_schema(schema_file)
        self.config = None

        ai_config = self._load_ai_config()

        final_api_key = api_key or ai_config.get("api_key")
        final_base_url = base_url or ai_config.get("base_url") or "https://api-inference.modelscope.cn/v1/"
        self.model = model or ai_config.get("model") or "Qwen/Qwen3.5-35B-A3B"
        self.base_url = final_base_url
        self.api_key = final_api_key
        self.max_tokens = ai_config.get("max_tokens", 8192)

        self.client = openai.OpenAI(api_key=self.api_key, base_url=self.base_url)

        self.system_prompt = self._build_system_prompt()

    def set_model(self, model_name: str):
        """Dynamically change the model being used"""
        self.model = model_name

    def get_available_models(self):
        """Get list of available models as (model_id, display_name) tuples"""
        return self.DEFAULT_MODELS

    def _load_ai_config(self) -> dict:
        config_path = os.path.join(os.path.dirname(__file__), ".ai.config.json")
        if os.path.exists(config_path):
            try:
                with open(config_path, "r") as f:
                    return json.load(f)
            except json.JSONDecodeError as e:
                logger.warning("Corrupt .ai.config.json at %s: %s", config_path, e)
            except Exception as e:
                logger.warning("Failed to read .ai.config.json at %s: %s", config_path, e)
        return {}

    def _load_schema(self, schema_file: str) -> List[Dict]:
        with open(schema_file, "r") as f:
            schema = json.load(f)
        if isinstance(schema, dict):
            return [schema]
        return schema

    def _build_system_prompt(self) -> str:
        schema_json = json.dumps(self.schema, indent=2, ensure_ascii=False)
        return f"""
You are an expert automotive software configuration assistant for SSAS (Simple Smart Automotive Software).

Your role is to help users analyze, validate, and fix JSON configuration files for automotive ECUs.

## SCHEMA REFERENCE (for validation):
```json
{schema_json}
```

## INSTRUCTIONS:
1. Analyze JSON configurations against the provided schema
2. Identify errors, invalid values, missing fields, and inconsistencies
3. Provide detailed explanations of problems found
4. Offer specific, actionable solutions

## RESPONSE FORMAT - YOU MUST RETURN ONLY VALID JSON:
DO NOT RETURN ANY TEXT OUTSIDE OF THE JSON OBJECT.
DO NOT USE MARKDOWN.
DO NOT USE CODE BLOCKS.
RETURN ONLY A SINGLE VALID JSON OBJECT.

{{
  "success": true/false,
  "message": "Brief summary",
  "issues": [
    {{
      "id": "issue1",
      "severity": "ERROR/WARNING/INFO",
      "module": "ModuleName",
      "description": "Issue description",
      "location": "path/to/field",
      "suggestion": "Fix suggestion"
    }}
  ],
  "fixed_config": [
    {{
      "class": "ModuleName",
      ...fixed configuration...
    }}
  ],
  "explanation": "Detailed explanation in plain text with line breaks using \\\\n"
}}

## Key Validation Rules:
1. All values must match their defined type (integer, string, boolean)
2. Integer values must be within defined minimum/maximum ranges
3. Enum values must be from the allowed list
4. References between modules must exist (enumref validation)
5. CAN IDs must be unique within each network
6. Required fields must be present

**IMPORTANT**: 
- ALWAYS return a valid JSON object
- The "fixed_config" field should contain the complete fixed configuration
- The "explanation" field should contain detailed markdown-formatted explanation
- If no issues found, return success=true with empty issues array and the original config
"""

    
    def load_config(self, config: List[Dict]):
        self.config = config
        self.config_map = {cfg["class"]: cfg for cfg in config}

    def chat(self, user_message: str) -> str:
        config_context = self._format_config_context()

        message_parts = []
        
        message_parts.append(f"## CURRENT CONFIGURATION:\n{config_context}")
        message_parts.append(f"## USER QUESTION:\n{user_message}")
        message_parts.append("## TASK:\nBased on the schema reference (provided in system prompt) and current configuration, provide a comprehensive response to the user's question. Include validation checks, error identification, and specific fixes if applicable.")

        full_message = "\n\n".join(message_parts)

        messages = [{"role": "system", "content": self.system_prompt}, {"role": "user", "content": full_message}]
        logger.info(f"Request to {self.model}: {user_message[:50]}...")
        try:
            response = self.client.chat.completions.create(
                model=self.model, messages=messages, temperature=0.3, max_tokens=self.max_tokens
            )

            answer = response.choices[0].message.content
            logger.info(f"Response from {self.model}: {answer[:100]}...")

            return answer

        except Exception as e:
            error_msg = f"API Error: {str(e)}. Please check your API key and network connection."
            return error_msg

    def _format_config_context(self) -> str:
        if not self.config:
            return "No configuration loaded."

        return f"""{json.dumps(self.config, indent=2, ensure_ascii=False)}"""


if __name__ == "__main__":
    import argparse
    import getpass

    parser = argparse.ArgumentParser(description="OpenAI-powered JSON Configuration Assistant")
    parser.add_argument("-s", "--schema", type=str, default="schema.json", help="Path to schema file")
    parser.add_argument("-i", "--input", type=str, help="Optional JSON configuration file to load")
    parser.add_argument("-k", "--api-key", type=str, help="OpenAI API key")
    args = parser.parse_args()

    api_key = args.api_key or os.environ.get("OPENAI_API_KEY")
    if not api_key:
        api_key = getpass.getpass("Enter your OpenAI API key: ")

    agent = OpenAIAgent(args.schema, api_key=api_key)

    if args.input:
        with open(args.input, "r") as f:
            config = json.load(f)
        if not isinstance(config, list):
            config = [config]
        agent.load_config(config)
        print("Configuration loaded successfully!\n")

    print("OpenAI-powered SSAS Configuration Assistant")
    print("Type 'exit' to quit, 'help' for available commands.\n")

    while True:
        user_input = input("> ")
        if user_input.lower() == "exit":
            break

        response = agent.chat(user_input)
        print(response)
        print("\n" + "=" * 60 + "\n")
