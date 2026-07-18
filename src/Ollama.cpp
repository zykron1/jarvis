#include "Ollama.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

Ollama::Ollama(std::string url, std::string model)
	: url(url), model(model)
{
	std::string systemPrompt = "Keep all answers as short as possible. Cut to the chase.";

	std::ifstream file("mcp/system.md");
	if (file.is_open()) {
		std::stringstream buf;
		buf << file.rdbuf();
		systemPrompt = buf.str();
	}

	messages = json::array({
		{
			{"role", "system"},
			{"content", systemPrompt}
		}
	});
}


struct StreamState {
	std::string buffer;
	std::string full_content;
	json tool_calls;
	bool has_tool_calls = false;
	StreamCallback on_token;
};

static size_t streamWriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	StreamState* state = static_cast<StreamState*>(userp);
	size_t totalSize = size * nmemb;
	state->buffer.append(static_cast<char*>(contents), totalSize);

	size_t pos;
	while ((pos = state->buffer.find('\n')) != std::string::npos) {
		std::string line = state->buffer.substr(0, pos);
		state->buffer.erase(0, pos + 1);

		if (line.empty()) continue;

		try {
			auto chunk = json::parse(line);

			if (chunk.contains("message")) {
				auto& msg = chunk["message"];

				if (msg.contains("content")) {
					std::string token = msg["content"].get<std::string>();
					if (!token.empty()) {
						state->full_content += token;
						if (state->on_token) {
							state->on_token(token);
						}
					}
				}

				if (msg.contains("tool_calls")) {
					state->has_tool_calls = true;
					state->tool_calls = msg["tool_calls"];
				}
			}
		} catch (...) {}
	}

	return totalSize;
}

static void processRemainingBuffer(StreamState* state)
{
	if (state->buffer.empty()) return;

	try {
		auto chunk = json::parse(state->buffer);

		if (chunk.contains("message")) {
			auto& msg = chunk["message"];

			if (msg.contains("content")) {
				std::string token = msg["content"].get<std::string>();
				if (!token.empty()) {
					state->full_content += token;
					if (state->on_token) {
						state->on_token(token);
					}
				}
			}

			if (msg.contains("tool_calls")) {
				state->has_tool_calls = true;
				state->tool_calls = msg["tool_calls"];
			}
		}
	} catch (...) {}
}

static json buildResponse(StreamState& state)
{
	json response_msg;
	response_msg["role"] = "assistant";
	response_msg["content"] = state.full_content;
	if (state.has_tool_calls) {
		response_msg["tool_calls"] = state.tool_calls;
	}

	json parsed_response;
	parsed_response["message"] = response_msg;
	return parsed_response;
}

json Ollama::chat(std::string prompt, StreamCallback on_token)
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

	this->curl = curl_easy_init();

	if (!this->curl)
		return json();

	messages.push_back({
		{"role", "user"},
		{"content", prompt}
	});

	json request;

	request["model"] = model;
	request["stream"] = true;
	request["messages"] = messages;
	request["tools"] = tools;

	std::string j = request.dump();

	curl_easy_setopt(
		this->curl,
		CURLOPT_URL,
		this->url.c_str()
	);

	curl_easy_setopt(
		this->curl,
		CURLOPT_POST,
		1L
	);

	curl_easy_setopt(
		this->curl,
		CURLOPT_POSTFIELDS,
		j.c_str()
	);

	curl_slist* headers = nullptr;
	headers = curl_slist_append(
		headers,
		"Content-Type: application/json"
	);

	curl_easy_setopt(
		this->curl,
		CURLOPT_HTTPHEADER,
		headers
	);

	StreamState state;
	state.on_token = on_token;

	curl_easy_setopt(
		this->curl,
		CURLOPT_WRITEFUNCTION,
		streamWriteCallback
	);

	curl_easy_setopt(
		this->curl,
		CURLOPT_WRITEDATA,
		&state
	);

	CURLcode result = curl_easy_perform(this->curl);

	processRemainingBuffer(&state);

	curl_slist_free_all(headers);
	curl_easy_cleanup(this->curl);

	if (result != CURLE_OK)
		return json();

	json parsed_response = buildResponse(state);

	if (parsed_response.contains("message")) {
		messages.push_back(parsed_response["message"]);
	}

	return parsed_response;
}

json Ollama::chat(json message, StreamCallback on_token)
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

	this->curl = curl_easy_init();

	if (!this->curl)
		return json();

	messages.push_back(message);

	json request;

	request["model"] = model;
	request["stream"] = true;
	request["messages"] = messages;
	request["tools"] = tools;

	std::string j = request.dump();

	curl_easy_setopt(
		this->curl,
		CURLOPT_URL,
		this->url.c_str()
	);

	curl_easy_setopt(
		this->curl,
		CURLOPT_POST,
		1L
	);

	curl_easy_setopt(
		this->curl,
		CURLOPT_POSTFIELDS,
		j.c_str()
	);

	curl_slist* headers = nullptr;
	headers = curl_slist_append(
		headers,
		"Content-Type: application/json"
	);

	curl_easy_setopt(
		this->curl,
		CURLOPT_HTTPHEADER,
		headers
	);

	StreamState state;
	state.on_token = on_token;

	curl_easy_setopt(
		this->curl,
		CURLOPT_WRITEFUNCTION,
		streamWriteCallback
	);

	curl_easy_setopt(
		this->curl,
		CURLOPT_WRITEDATA,
		&state
	);

	CURLcode result = curl_easy_perform(this->curl);

	processRemainingBuffer(&state);

	curl_slist_free_all(headers);
	curl_easy_cleanup(this->curl);

	if (result != CURLE_OK)
		return json();

	json parsed_response = buildResponse(state);

	if (parsed_response.contains("message")) {
		messages.push_back(parsed_response["message"]);
	}

	return parsed_response;
}

void Ollama::addTool(json mcp_tool)
{
	json ollama_tool;

	ollama_tool["type"] = "function";

	ollama_tool["function"]["name"] = mcp_tool["name"];
	ollama_tool["function"]["description"] = mcp_tool.value("description", "");

	if (mcp_tool.contains("input_schema")) {
		ollama_tool["function"]["parameters"] = mcp_tool["input_schema"];
	} else {
		ollama_tool["function"]["parameters"] = {
			{"type", "object"},
			{"properties", json::object()}
		};
	}

	this->tools.push_back(ollama_tool);
}
