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
	public:
		Ollama(std::string url, std::string model);
		json chat(std::string prompt);
};
