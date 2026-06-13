# SSAS JSON Editor - AI Assistant

This document describes the AI-powered features of the SSAS JSON Editor for automotive ECU configuration validation and assistance.

## Features

- **AI Validation**: Automatically validate JSON configurations against SSAS schema
- **Issue Detection**: Identify errors, invalid values, missing fields, and inconsistencies
- **Smart Fixes**: Get AI-generated fixes for configuration issues
- **Interactive Chat**: Ask questions about your configuration and get detailed explanations
- **Collapsible Q&A**: Organized conversation history with expandable/collapsible sections

## Getting Started

### Step 1: Configure API Token

Create or edit the `.ai.config.json` file in the `tools/json.editor/` directory:

```json
{
  "api_key": "your-modelscope-token",
  "base_url": "https://api-inference.modelscope.cn/v1/",
  "model": "Qwen/Qwen3.5-35B-A3B"
}
```

### Step 2: Obtain ModelScope API Token

1. Sign up at [ModelScope](https://www.modelscope.cn/)
2. Go to [My Access Token](https://www.modelscope.cn/my/access/token)
3. Click "Generate Token" or use an existing token
4. Copy the token into `.ai.config.json` as the `api_key` value

### Step 3: Choose a Model

You can change the `model` parameter in `.ai.config.json` to use different models. Popular options include:

| Model | Parameters | Max Tokens | Description | Performance | Latency |
|-------|------------|------------|-------------|-------------|---------|
| `Qwen/Qwen3.5-35B-A3B` | 35B | 8192 | Balanced performance and speed (**Recommended**) | ???? | ???? |
| `Qwen/Qwen2-72B-Instruct` | 72B | 8192 | Best for complex reasoning tasks | ????? | ?? |
| `deepseek-ai/DeepSeek-V4-Flash` | 158B | 8192 | High-performance reasoning | ????? | ?? |
| `deepseek-ai/DeepSeek-V3.2` | 685B | 8192 | Top-tier reasoning capabilities | ????? | ? |
| `deepseek-ai/DeepSeek-R1-Distill-Qwen-1.5B` | 1.5B | 2048 | Fastest response, basic tasks only | ?? | ????? |
| `ZhipuAI/GLM-4-Plus` | 8B | 8192 | Fast Chinese understanding | ??? | ???? |

**Max Tokens Configuration:**

The `max_tokens` parameter controls the maximum number of tokens the AI can generate in a single response. You can configure this in `.ai.config.json`:

```json
{
  "api_key": "your-api-key",
  "base_url": "https://api-inference.modelscope.cn/v1/",
  "model": "Qwen/Qwen3.5-35B-A3B",
  "max_tokens": 8192
}
```

**Recommended Values:**
- Small models (1.5B-8B): 2048-4096 tokens
- Medium models (35B-72B): 4096-8192 tokens
- Large models (158B+): 8192 tokens (maximum supported)

**Performance Tips:**
- **Low Latency**: Use smaller models like `DeepSeek-R1-Distill-Qwen-1.5B` or `GLM-4-Plus` for fast responses
- **Best Performance**: Use larger models like `DeepSeek-V3.2` or `Qwen2-72B-Instruct` for complex tasks
- **Balanced**: `Qwen3.5-35B-A3B` offers the best balance of speed and capability

**To browse all available models:**
- Visit the [ModelScope Model Hub](https://www.modelscope.cn/models)
- Filter by task type, framework, and license
- Use the model ID format: `owner/model-name`

## Usage

### Access AI Validation

1. Open the JSON Editor
2. Load your configuration files (CanIf, PduR, Com, Dcm, OS, EcuC, etc.)
3. Press `Ctrl+V` or select "AI Validate" from the File menu
4. The AI Validation Assistant window will appear

### Use the AI Assistant

1. **Initial Validation**: When opened, the assistant automatically validates your configuration
2. **Ask Questions**: Type questions in the input field at the bottom
3. **Apply Fixes**: If fixes are available, click the "Apply Fix" button to update your configuration
4. **Expand/Collapse**: Click on Q&A headers to expand or collapse sections

### Example Questions

- `help validate config` - Run full validation
- `check OS module` - Validate only the OS module
- `add message vehicle speed` - Add a new CAN message
- `find errors in CanIf` - Find issues in CanIf configuration
- `explain this configuration` - Get detailed explanation

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+V` | Open AI Validation Assistant |
| `Enter` | Send message (in AI window) |

## API Information

### Free Quota
- **Daily Limit**: 2,000 API calls per day
- **Per-Model Limit**: 500 calls per model per day
- **More Details**: [ModelScope API Limits](https://www.modelscope.cn/docs/model-service/API-Inference/limits)

### Model ID Format
ModelScope uses namespace-prefixed format: `owner/model-name`

### OpenAPI Access
You can programmatically list available models using:
- **API Endpoint**: `GET /models`
- **Documentation**: [https://modelscope.cn/docs/openapi](https://modelscope.cn/docs/openapi)

## Troubleshooting

- **API Key Issues**: Ensure your API token is correctly set in `.ai.config.json`
- **Network Issues**: Verify network connectivity to the API endpoint
- **Fix Button Not Showing**: The button only appears when valid fixes are available
- **Model Unavailable**: Check the [ModelScope Model Hub](https://www.modelscope.cn/models) for available models

## Requirements

- Python 3.8+
- PyQt5
- openai Python package
- ModelScope API access (requires linked Alibaba Cloud account)

## License

This tool is part of the SSAS (Simple Smart Automotive Software) project.
