# jarvis

A terminal-based AI assistant with MCP (Model Context Protocol) support, voice input via Whisper, and agentic tool-use workflows. Written in C++.

## Features

- **MCP client** - Connects to external tool servers (filesystem, search, etc.) via stdio or HTTP
- **Voice input** - Real-time speech-to-text using Whisper.cpp (local, no cloud)
- **Multiple modes** - Coder, Pentester, Planner personas with different system prompts
- **Tool use** - LLM can invoke MCP tools, chain calls, and iterate
- **Context management** - Sliding window with optional summarization to preserve long conversations
- **Model-agnostic** - Works with any OpenAI-compatible API (OpenRouter, OpenCode Zen, local Ollama, etc.)

## Build

```bash
git submodule update --init --recursive
cmake -B build
cmake --build build
```

## Models

Download a Whisper model for voice input:

```bash
sh ./external/whisper.cpp/models/download-ggml-model.sh base.en
cp external/whisper.cpp/models/ggml-base.en.bin models/
```

## Configuration

Create `.env` in the project root:

```ini
api_key=your-api-key-here
EXA_KEY=your-exa-key-here
```

- `api_key` - API key for the LLM endpoint (OpenRouter, OpenCode Zen, etc.)
- `EXA_KEY` - API key for Exa search (used by the Exa MCP server)

Configure the LLM endpoint in `src/main.cpp` (line ~86). Default is OpenCode Zen:
```cpp
Ollama lama("https://opencode.ai/zen/v1/chat/completions", model, "", promptPath);
```

Alternative endpoints (uncomment to use):
```cpp
// Ollama lama("https://openrouter.ai/api/v1/chat/completions", model, "", promptPath);
// Ollama lama("https://ai.hackclub.com/proxy/v1/chat/completions", model, "", promptPath);
```

## Running

```bash
./build/jarvis [folder] [model] [options]
```

Arguments:
- `folder` — Project directory to work in (default: current directory)
- `model` — Model ID (default: `openai/gpt-oss-20b`)

Options:
- `-m, --mode <mode>` — Agent mode: `code`, `hack`, `plan` (default: `code`)
- `-h, --help` — Show help

Examples:
```bash
./build/jarvis
./build/jarvis ~/my-project
./build/jarvis ~/my-project --mode plan
./build/jarvis ~/my-project nvidia/nemotron-3-super:free
```

## Usage

Once running, type naturally. Commands:
- `/voice` — Start voice input (hold to talk, release to send)
- `/model` — Show or switch model (e.g., `/model openai/gpt-4o`)
- `/servers` — List connected MCP servers and available tools
- `/mode <code|hack|plan>` — Switch agent persona

The assistant will use MCP tools automatically when needed. Tool calls and results are shown inline.

## Project Structure

```
src/
  main.cpp          # Entry point, CLI parsing, main loop
  Ollama.cpp        # LLM client, context management, streaming
  MCPManager.cpp    # MCP client, server lifecycle, tool calls
  HTTPTransport.cpp # HTTP transport for MCP servers
  Microphone.cpp    # Audio capture (PortAudio)
  Whisper.cpp       # Whisper.cpp integration
mcp/
  server.py         # Filesystem MCP server (stdio)
  exa.py            # Exa search MCP server (stdio)
  code.md           # Coder system prompt
  hack.md           # Pentester system prompt
  plan.md           # Planner system prompt
  tools.md          # Tool descriptions injected into prompts
```

## Dependencies

- C++20 compiler
- CMake 3.20+
- PortAudio (for microphone input)
- curl (HTTP)
- nlohmann/json (included as submodule)
- Whisper.cpp (included as submodule)
- termmark (included as submodule)
