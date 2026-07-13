#pragma once

#include <curl/curl.h>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Ollama {
	private:
		std::string url;
		std::string model;
		json messages;
		CURL* curl;
		json tools = json::array();
	public:
		Ollama(std::string url, std::string model);
		json chat(std::string prompt);
		json chat(json message);

		void addTool(json mcp_tool);
};
