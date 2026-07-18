#include "MCP.h"
#include "Microphone.h"
#include "Whisper.h"
#include "Ollama.h"
#include "nlohmann/json.hpp"

#include <iostream>
#include <cstdlib>

namespace color {
	const char* reset   = "\033[0m";
	const char* bold    = "\033[1m";
	const char* dim     = "\033[2m";
	const char* cyan    = "\033[36m";
	const char* green   = "\033[32m";
	const char* yellow  = "\033[33m";
	const char* magenta = "\033[35m";
	const char* red     = "\033[31m";
	const char* blue    = "\033[34m";
}

int main()
{
	Microphone mic;
	Whisper whisper("models/ggml-base.en.bin");
	Ollama lama("https://api.groq.com/openai/v1/chat/completions", "llama-3.1-8b-instant");

	MCP mcp;
	mcp.connect("python3 mcp/server.py");

	for (auto& mcp_func : mcp.listTools()) {
		lama.addTool(mcp_func);
	}

	std::cout << color::cyan << color::bold << "[JARVIS] " << color::reset
			  << color::dim << "Successfully loaded voice-to-text model." << color::reset << "\n";

	while (1) {
		mic.start();
		std::cout << color::cyan << color::bold << "[JARVIS] " << color::reset
				  << "Speak now sentient entity... Press any key to stop";
		std::cin.get();
		mic.stop();

		auto audio = mic.getAudio();
		std::cout << color::cyan << color::bold << "[JARVIS] " << color::reset
				  << color::dim << "Processing prompt..." << color::reset << std::endl;
		std::string text = whisper.transcribe(audio);

		std::cout << color::green << color::bold << "[USER] " << color::reset
				  << text << "\n" << std::endl;

		std::cout << color::cyan << color::bold << "[JARVIS] " << color::reset
				  << color::dim << "Thinking..." << color::reset << std::endl;
		std::cout << color::cyan << color::bold << "[JARVIS] " << color::reset << std::flush;
		json output = lama.chat(text, [](const std::string& token) {
			std::cout << token << std::flush;
		});
		std::cout << std::endl;

		while (output["message"].contains("tool_calls")) {
			json tool_calls = output["message"]["tool_calls"];
		
			for (auto& call : tool_calls) {
				std::string name = call["function"]["name"];
				json arguments = call["function"].value("arguments", json::object());
		
				std::cout << color::yellow << color::bold << "  [TOOL] " << color::reset
						  << color::dim << name << color::reset << std::endl;
		
				json result = mcp.callTool(name, arguments);
		
				json tool_response = {
					{"role", "tool"},
					{"content", result.dump()}
				};
		
				std::cout << color::cyan << color::bold << "[JARVIS] " << color::reset << std::flush;
				output = lama.chat(tool_response, [](const std::string& token) {
					std::cout << token << std::flush;
				});
				std::cout << std::endl;
			}
		}

		std::cout << std::endl;
	}
}
