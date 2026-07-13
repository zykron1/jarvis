#include "MCP.h"
#include "Microphone.h"
#include "Whisper.h"
#include "Ollama.h"
#include "nlohmann/json.hpp"

#include <iostream>

int main()
{
	Microphone mic;
	Whisper whisper("models/ggml-base.en.bin");
	Ollama lama("http://localhost:11434/api/chat", "qwen2.5:1.5b");

	MCP mcp;
	mcp.connect("python3 mcp/server.py");

	for (auto& mcp_func : mcp.listTools()) {
		lama.addTool(mcp_func);
	}

	std::cout << "[JARVIS] Successfully loaded voice-to-text model. \n";

	while (1) {
		mic.start();
		std::cout << "[JARVIS] Speak now sentient entity... Press any key to stop";
		std::cin.get();

		auto audio = mic.getAudio();
		std::cout << "[JARVIS] Processing Prompt\n";
		std::string text = whisper.transcribe(audio);

		std::cout << "[USER] " << text << "\n";

		std::cout << "[JARVIS] Contacting LLM...." << std::endl;
		json output = lama.chat(text);

		std::string message = output["message"]["content"];
		std::cout << "[JARVIS] " << message << std::endl;

		if (output["message"].contains("tool_calls")) {
			json tool_calls = output["message"]["tool_calls"];
		
			for (auto& call : tool_calls) {
				std::string name = call["function"]["name"];
				json arguments = call["function"].value("arguments", json::object());
		
				json result = mcp.callTool(name, arguments);
		
				json tool_response = {
					{"role", "tool"},
					{"content", result.dump()}
				};
		
				output = lama.chat(tool_response);
			}
		
			message = output["message"].value("content", "");
			std::cout << "[JARVIS] " << message << std::endl;
		}
	}
}
