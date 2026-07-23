# SPARK - Smart Platform for Automotive Rapid Kit

A Python library for AI-powered AUTOSAR development.

## Overview

SPARK is a Python library designed to assist with AUTOSAR software development. It provides:

- **AI-powered configuration assistance**: Validate, analyze, and fix JSON configuration files
- **Tool system**: Read files, list modules, validate configurations
- **Unified settings management**: Centralized configuration for LLM and environment
- **DBC file support**: Load CAN DBC files and configure COM stack automatically

## Installation

```bash
pip install openai
```

## Getting Started

### Step 1: Configure API Token

Create `.spark/settings.json` in your project root:

```json
{
  "llm": {
    "api_key": "your-modelscope-token",
    "base_url": "https://api-inference.modelscope.cn/v1/",
    "model": "Qwen/Qwen3.5-35B-A3B",
    "max_tokens": 8192,
    "max_continuations": 128,
    "timeout": 300
  }
}
```

### Step 2: Obtain ModelScope API Token

1. Sign up at [ModelScope](https://www.modelscope.cn/)
2. Go to [My Access Token](https://www.modelscope.cn/my/access/token)
3. Click "Generate Token" or use an existing token
4. Copy the token into the config as the `api_key` value

### Step 3: Choose a Model

Set the `model` parameter in your config. Popular options:

| Model | Parameters | Max Tokens | Description |
|-------|------------|------------|-------------|
| `Qwen/Qwen3.5-35B-A3B` | 35B | 8192 | Balanced performance and speed (**Recommended**) |
| `Qwen/Qwen2-72B-Instruct` | 72B | 8192 | Best for complex reasoning tasks |
| `deepseek-ai/DeepSeek-V4-Flash` | 158B | 8192 | High-performance reasoning |
| `deepseek-ai/DeepSeek-V3.2` | 685B | 8192 | Top-tier reasoning capabilities |
| `deepseek-ai/DeepSeek-R1-Distill-Qwen-1.5B` | 1.5B | 2048 | Fastest response, basic tasks only |
| `ZhipuAI/GLM-4-Plus` | 8B | 8192 | Fast Chinese understanding |

**Recommended Values:**
- Small models (1.5B-8B): 2048-4096 tokens
- Medium models (35B-72B): 4096-8192 tokens
- Large models (158B+): 8192 tokens (maximum supported)

**Performance Tips:**
- **Low Latency**: Use smaller models like `DeepSeek-R1-Distill-Qwen-1.5B` or `GLM-4-Plus`
- **Best Performance**: Use larger models like `DeepSeek-V3.2` or `Qwen2-72B-Instruct`
- **Balanced**: `Qwen3.5-35B-A3B` offers the best balance of speed and capability

## Usage

### Library Usage

```python
from spark import SparkAgent

# Create agent with schema
agent = SparkAgent(schemaFile="path/to/schema.json")

# Load configuration from directory
agent.loadConfigFromDir("app/app/config")

# Chat with AI
response = agent.chat("Validate my configuration")
print(response)
```

### With Settings

```python
from spark import SparkAgent, SettingsManager

settings = SettingsManager.getInstance()
settings.set("llm.api_key", "your-api-key")
settings.set("llm.base_url", "https://api-inference.modelscope.cn/v1/")
settings.set("llm.model", "Qwen/Qwen3.5-35B-A3B")

agent = SparkAgent()
config = [{"class": "Com", "networks": []}]
agent.loadConfig(config)
response = agent.chat("Load DBC file and configure COM stack")
```

### In JSON Editor

1. Open the JSON Editor (`tools/json.editor/main.py`)
2. Load your configuration files (CanIf, PduR, Com, Dcm, OS, EcuC, etc.)
3. Press `Ctrl+V` or select **File → AI Validate**
4. The AI Validation Assistant window will appear with:
   - **Initial Validation** on open
   - **Question input** at the bottom to ask about your config
   - **Apply Fix** buttons for suggested changes
   - **Collapsible Q&A** sections to review history

### Example Questions

- `help validate config` — Run full validation
- `check OS module` — Validate only the OS module
- `add message vehicle speed` — Add a new CAN message
- `find errors in CanIf` — Find issues in CanIf configuration
- `explain this configuration` — Get detailed explanation

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+V` | Open AI Validation Assistant |
| `Enter` | Send message (in AI window) |

## Features

### AI Agent

- **LLM Integration**: OpenAI-compatible API support
- **Tool Calling**: Read external files (DBC, XML, JSON, CSV)
- **Auto-continuation**: Handle truncated responses
- **JSON Response Format**: Structured validation results

### Configuration Management

- **Unified Settings**: `.spark/settings.json` configuration
- **Environment Variables**: Support for SPARK_LLM_API_KEY
- **Multiple Config Sources**: Project, user, and fallback locations

### Tool System

| Tool | Description |
|------|-------------|
| `read` | Read text files (DBC, XML, JSON, CSV) |

## Settings File

Full `.spark/settings.json` example:

```json
{
  "llm": {
    "api_key": "your-api-key",
    "base_url": "https://api-inference.modelscope.cn/v1/",
    "model": "Qwen/Qwen3.5-35B-A3B",
    "max_tokens": 8192,
    "max_continuations": 128,
    "timeout": 300
  },
  "environment": {
    "msys2": "C:\\msys64",
    "python": "python",
    "build_dir": "build/nt/GCC/one"
  }
}
```

## API Reference

### SparkAgent

```python
agent = SparkAgent(schemaFile=None, apiKey=None, baseUrl=None, model=None)

agent.loadConfig(config)          # Load from list
agent.loadConfigFromDir(path)     # Load from directory
response = agent.chat(message)    # Chat with AI
agent.setModel(modelName)         # Change model
agent.getAvailableModels()        # Get available models
```

### SettingsManager

```python
settings = SettingsManager.getInstance()

settings.getLLM()                 # LLM settings
settings.getEnvironment()         # Environment settings
settings.set("llm.api_key", value)
settings.getKey("llm.model")
```

## API Information

- **Daily Limit**: 2,000 API calls per day
- **Per-Model Limit**: 500 calls per model per day
- **Model ID Format**: `owner/model-name` (e.g., `Qwen/Qwen3.5-35B-A3B`)
- **Browse Models**: [ModelScope Model Hub](https://www.modelscope.cn/models)

## Troubleshooting

- **API Key Issues**: Ensure your API token is correctly set in `.spark/settings.json`
- **Network Issues**: Verify network connectivity to the API endpoint
- **Fix Button Not Showing**: The button only appears when valid fixes are available
- **Model Unavailable**: Check the [ModelScope Model Hub](https://www.modelscope.cn/models) for available models

## Requirements

- Python 3.8+
- openai Python package
- ModelScope API access (requires linked Alibaba Cloud account)

## Examples

### Validate Configuration

```python
from spark import SparkAgent

agent = SparkAgent("tools/json.editor/schema.json")
agent.loadConfigFromDir("app/app/config")
response = agent.chat("Validate the configuration and find any issues")
print(response)
```

### Load DBC and Configure COM Stack

```python
from spark import SparkAgent

agent = SparkAgent("tools/json.editor/schema.json")
response = agent.chat("Load app/app/config/Com/CAN0.dbc and configure the COM stack")
print(response)
```

## License

MIT
