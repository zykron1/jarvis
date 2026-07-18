#include "Ollama.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

static void loadEnvFile(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open()) return;

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#') continue;

		auto eq = line.find('=');
		if (eq == std::string::npos) continue;

		std::string key = line.substr(0, eq);
		std::string val = line.substr(eq + 1);

		while (!key.empty() && key.back() == ' ') key.pop_back();
		while (!val.empty() && val.front() == ' ') val.erase(0, 1);

		if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
			val = val.substr(1, val.size() - 2);
		else if (val.size() >= 2 && val.front() == '\'' && val.back() == '\'')
			val = val.substr(1, val.size() - 2);

		setenv(key.c_str(), val.c_str(), 1);
	}
}

Ollama::Ollama(std::string url, std::string model, std::string api_key)
	: url(url), model(model), api_key(api_key)
{
	openai_compat = (url.find("localhost") == std::string::npos &&
					 url.find("127.0.0.1") == std::string::npos);

	loadEnvFile(".env");

	if (this->api_key.empty()) {
		const char* env_key = std::getenv("groq_api_key");
		if (env_key) this->api_key = env_key;
	}

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
	json tool_calls = json::array();
	bool has_tool_calls = false;
	bool openai_compat = false;
	StreamCallback on_token;
};

static void parseOllamaLine(const std::string& line, StreamState* state)
{
	auto chunk = json::parse(line);

	if (chunk.contains("message")) {
		auto& msg = chunk["message"];

		if (msg.contains("content")) {
			std::string token = msg["content"].get<std::string>();
			if (!token.empty()) {
				state->full_content += token;
				if (state->on_token) state->on_token(token);
			}
		}

		if (msg.contains("tool_calls")) {
			state->has_tool_calls = true;
			state->tool_calls = msg["tool_calls"];
		}
	}
}

static void parseOpenAISSE(const std::string& line, StreamState* state)
{
	if (line == "[DONE]") return;

	std::string json_str = line;
	if (json_str.rfind("data: ", 0) == 0)
		json_str = json_str.substr(6);

	if (json_str.empty() || json_str == "{}") return;

	try {
		auto chunk = json::parse(json_str);

		if (!chunk.contains("choices") || chunk["choices"].empty()) return;
		auto& delta = chunk["choices"][0];

		if (delta.contains("delta")) {
			auto& d = delta["delta"];

			if (d.contains("content") && !d["content"].is_null()) {
				std::string token = d["content"].get<std::string>();
				if (!token.empty()) {
					state->full_content += token;
					if (state->on_token) state->on_token(token);
				}
			}

			if (d.contains("tool_calls") && !d["tool_calls"].is_null()) {
				state->has_tool_calls = true;

				for (auto& tc : d["tool_calls"]) {
					if (!tc.contains("index")) continue;
					int idx = tc["index"].get<int>();

					while ((int)state->tool_calls.size() <= idx)
						state->tool_calls.push_back(json::object());

					auto& entry = state->tool_calls[idx];

					if (tc.contains("id") && !tc["id"].is_null())
						entry["id"] = tc["id"].get<std::string>();

					if (tc.contains("type") && !tc["type"].is_null())
						entry["type"] = tc["type"].get<std::string>();

					if (tc.contains("function")) {
						auto& fn = tc["function"];
						if (fn.contains("name") && !fn["name"].is_null())
							entry["function"]["name"] = fn["name"].get<std::string>();

						if (fn.contains("arguments") && !fn["arguments"].is_null()) {
							std::string cur = entry["function"].value("arguments", "");
							cur += fn["arguments"].get<std::string>();
							entry["function"]["arguments"] = cur;
						}
					}
				}
			}
		}
	} catch (...) {}
}

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
			if (state->openai_compat)
				parseOpenAISSE(line, state);
			else
				parseOllamaLine(line, state);
		} catch (...) {}
	}

	return totalSize;
}

static void processRemainingBuffer(StreamState* state)
{
	if (state->buffer.empty()) return;

	try {
		if (state->openai_compat)
			parseOpenAISSE(state->buffer, state);
		else
			parseOllamaLine(state->buffer, state);
	} catch (...) {}
}

static json buildResponse(StreamState& state)
{
	json response_msg;
	response_msg["role"] = "assistant";
	response_msg["content"] = state.full_content;

	if (state.has_tool_calls) {
		json formatted_calls = json::array();

		for (auto& tc : state.tool_calls) {
			json call;
			call["type"] = "function";

			if (tc.contains("id"))
				call["id"] = tc["id"];

			if (tc.contains("function")) {
				auto& fn = tc["function"];
				call["function"]["name"] = fn["name"];

				if (fn.contains("arguments")) {
					std::string args_str = fn["arguments"].get<std::string>();
					try {
						call["function"]["arguments"] = json::parse(args_str);
					} catch (...) {
						call["function"]["arguments"] = json::object();
					}
				} else {
					call["function"]["arguments"] = json::object();
				}
			}

			formatted_calls.push_back(call);
		}

		response_msg["tool_calls"] = formatted_calls;
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
	if (!tools.empty()) request["tools"] = tools;

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

	if (!api_key.empty()) {
		std::string auth = "Authorization: Bearer " + api_key;
		headers = curl_slist_append(headers, auth.c_str());
	}

	curl_easy_setopt(
		this->curl,
		CURLOPT_HTTPHEADER,
		headers
	);

	StreamState state;
	state.on_token = on_token;
	state.openai_compat = openai_compat;

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

	if (openai_compat && message.value("role", "") == "tool") {
		json tool_call_id;

		for (int i = (int)messages.size() - 1; i >= 0; i--) {
			if (messages[i].value("role", "") == "assistant" &&
				messages[i].contains("tool_calls")) {
				for (auto& tc : messages[i]["tool_calls"]) {
					if (tc.contains("id")) {
						tool_call_id = tc["id"];
						break;
					}
				}
				break;
			}
		}

		if (!tool_call_id.is_null()) {
			message["tool_call_id"] = tool_call_id;
		}
	}

	messages.push_back(message);

	json request;

	request["model"] = model;
	request["stream"] = true;
	request["messages"] = messages;
	if (!tools.empty()) request["tools"] = tools;

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

	if (!api_key.empty()) {
		std::string auth = "Authorization: Bearer " + api_key;
		headers = curl_slist_append(headers, auth.c_str());
	}

	curl_easy_setopt(
		this->curl,
		CURLOPT_HTTPHEADER,
		headers
	);

	StreamState state;
	state.on_token = on_token;
	state.openai_compat = openai_compat;

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
