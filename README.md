# jarvis
Custom AI assistant that supports local voice to text and local AI models acting as an MCP client to support agentic workflows.

# Build Me
```
git submodule update --init --recursive
cmake -B build
cmake --build build
```

Download a Whisper model in GGML format.

```
sh ./external/whisper.cpp/models/download-ggml-model.sh base.en
cp external/whisper.cpp/models/ggml-base.en.bin models/
```

Setup Ollama

```
ollama pull qwen2.5:1.5b
ollama serve
```
