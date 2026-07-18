#pragma once

#include <curl/curl.h>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using StreamCallback = std::function<void(const std::string&)>;

class Ollama {
	private:
		std::string url;
		std::string model;
		json messages;
		CURL* curl;
		json tools = json::array();
	public:
		Ollama(std::string url, std::string model);
		json chat(std::string prompt, StreamCallback on_token = nullptr);
		json chat(json message, StreamCallback on_token = nullptr);

		void addTool(json mcp_tool);
};
