#include "MCP.h"
#include "Microphone.h"
#include "Whisper.h"
#include "Ollama.h"
#include "nlohmann/json.hpp"

#include <iostream>
#include <cstdlib>
#include <ostream>
#include <string>
#include <filesystem>

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

int main(int argc, char* argv[])
{
	std::string prog = std::filesystem::path(argv[0]).filename().string();
	std::string model = "anthropic/claude-sonnet-4";

	if (argc > 1) {
		std::string arg = argv[1];
		if (arg == "--help" || arg == "-h") {
			std::cout << "Usage: " << prog << " [MODEL]\n\n"
					  << "  MODEL  OpenRouter model ID (default: anthropic/claude-sonnet-4)\n"
					  << "\nExamples:\n"
					  << "  " << prog << "\n"
					  << "  " << prog << " openrouter/owl-alpha:free\n"
					  << "  " << prog << " nvidia/nemotron-3-super:free\n"
					  << "  " << prog << " anthropic/claude-sonnet-4\n";
			return 0;
		}
		model = arg;
	}

	Microphone mic;
	Whisper whisper("models/ggml-base.en.bin");
	Ollama lama("https://ai.hackclub.com/proxy/v1/chat/completions", model);

	std::cout << "Model: " << model << std::endl;

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
			std::string args_str = call["function"].value("arguments", "{}");
			json arguments;
			try {
				arguments = json::parse(args_str);
			} catch (...) {
				arguments = json::object();
			}

			std::cout << color::yellow << color::bold << "  [TOOL] " << color::reset
					  << color::dim << name << color::reset << std::endl;

			std::string tool_text;
			try {
				json result = mcp.callTool(name, arguments);

				if (result.contains("content") && result["content"].is_array()) {
					for (auto& item : result["content"]) {
						if (item.contains("text")) {
							tool_text += item["text"].get<std::string>();
						}
					}
				} else {
					tool_text = result.dump();
				}
			} catch (const std::exception& e) {
				tool_text = std::string("Error calling tool '") + name + "': " + e.what();
				std::cout << color::red << color::bold << "  [ERROR] " << color::reset
						  << color::dim << tool_text << color::reset << std::endl;
			}

			json tool_response = {
				{"role", "tool"},
				{"content", tool_text}
			};

			if (call.contains("id") && !call["id"].is_null()) {
				tool_response["tool_call_id"] = call["id"];
			}

			lama.addMessage(tool_response);
		}

			std::cout << color::cyan << color::bold << "[JARVIS] " << color::reset << std::flush;
			output = lama.complete([](const std::string& token) {
				std::cout << token << std::flush;
			});
			std::cout << std::endl;
		}

		std::cout << std::endl;
	}
}
