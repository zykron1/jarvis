#include "HTTPTransport.h"
#include <iostream>
#include <sstream>
#include <algorithm>

struct CURLWriteBuffer {
	std::string data;
};

struct CURLHeaderState {
	std::string sessionId;
	long httpCode = 0;
	std::string contentType;
};

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	auto* buf = static_cast<CURLWriteBuffer*>(userp);
	size_t total = size * nmemb;
	buf->data.append(static_cast<char*>(contents), total);
	return total;
}

static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userp) {
	auto* state = static_cast<CURLHeaderState*>(userp);
	size_t total = size * nitems;
	std::string header(buffer, total);

	if (header.find("Mcp-Session-Id:") == 0 || header.find("mcp-session-id:") == 0) {
		auto colon = header.find(':');
		auto start = header.find_first_not_of(" \t", colon + 1);
		auto end = header.find_first_of("\r\n", start);
		if (start != std::string::npos) {
			state->sessionId = header.substr(start, end - start);
		}
	}

	if (header.find("Content-Type:") == 0 || header.find("content-type:") == 0) {
		auto colon = header.find(':');
		auto start = header.find_first_not_of(" \t", colon + 1);
		auto end = header.find_first_of(";\r\n", start);
		if (start != std::string::npos) {
			state->contentType = header.substr(start, end - start);
		}
	}

	return total;
}

HTTPTransport::HTTPTransport(const std::string& url,
							 const std::map<std::string, std::string>& headers)
	: url(url), headers(headers) {}

HTTPTransport::~HTTPTransport() {
	close();
}

HTTPTransport::HTTPTransport(HTTPTransport&& other) noexcept
	: url(std::move(other.url)),
	  headers(std::move(other.headers)),
	  sessionId(std::move(other.sessionId)),
	  connected(other.connected),
	  pendingMessages(std::move(other.pendingMessages)),
	  pendingIndex(other.pendingIndex),
	  useLegacySSE(other.useLegacySSE),
	  legacyEndpoint(std::move(other.legacyEndpoint))
{
	other.connected = false;
	other.pendingIndex = 0;
}

HTTPTransport& HTTPTransport::operator=(HTTPTransport&& other) noexcept {
	if (this != &other) {
		close();
		url = std::move(other.url);
		headers = std::move(other.headers);
		sessionId = std::move(other.sessionId);
		connected = other.connected;
		pendingMessages = std::move(other.pendingMessages);
		pendingIndex = other.pendingIndex;
		useLegacySSE = other.useLegacySSE;
		legacyEndpoint = std::move(other.legacyEndpoint);
		other.connected = false;
		other.pendingIndex = 0;
	}
	return *this;
}

bool HTTPTransport::open() {
	return connectInitialize();
}

void HTTPTransport::close() {
	if (connected && !sessionId.empty()) {
		doDELETE();
	}
	connected = false;
	pendingMessages.clear();
	pendingIndex = 0;
}

bool HTTPTransport::is_open() const {
	return connected;
}

bool HTTPTransport::doPOST(const std::string& body, std::string& responseBody,
						   std::string& contentType, long& httpCode) {
	CURL* curl = curl_easy_init();
	if (!curl) return false;

	CURLWriteBuffer writeBuf;
	CURLHeaderState headerState;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());

	curl_slist* hdrs = nullptr;
	std::string accept = "Accept: application/json, text/event-stream";
	hdrs = curl_slist_append(hdrs, accept.c_str());
	hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

	if (!sessionId.empty()) {
		std::string sessionHeader = "Mcp-Session-Id: " + sessionId;
		hdrs = curl_slist_append(hdrs, sessionHeader.c_str());
	}

	for (auto& [key, val] : headers) {
		std::string h = key + ": " + val;
		hdrs = curl_slist_append(hdrs, h.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeBuf);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerState);

	CURLcode res = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	if (res != CURLE_OK) {
		curl_slist_free_all(hdrs);
		curl_easy_cleanup(curl);
		return false;
	}

	if (!headerState.sessionId.empty()) {
		sessionId = headerState.sessionId;
	}

	responseBody = writeBuf.data;
	contentType = headerState.contentType;

	curl_slist_free_all(hdrs);
	curl_easy_cleanup(curl);
	return true;
}

bool HTTPTransport::doGET(std::string& responseBody, long& httpCode) {
	CURL* curl = curl_easy_init();
	if (!curl) return false;

	CURLWriteBuffer writeBuf;
	CURLHeaderState headerState;

	const char* targetUrl = useLegacySSE ? legacyEndpoint.c_str() : url.c_str();
	curl_easy_setopt(curl, CURLOPT_URL, targetUrl);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

	curl_slist* hdrs = nullptr;
	std::string accept = "Accept: text/event-stream";
	hdrs = curl_slist_append(hdrs, accept.c_str());

	if (!sessionId.empty()) {
		std::string sessionHeader = "Mcp-Session-Id: " + sessionId;
		hdrs = curl_slist_append(hdrs, sessionHeader.c_str());
	}

	for (auto& [key, val] : headers) {
		std::string h = key + ": " + val;
		hdrs = curl_slist_append(hdrs, h.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeBuf);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerState);

	CURLcode res = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	if (res != CURLE_OK) {
		curl_slist_free_all(hdrs);
		curl_easy_cleanup(curl);
		return false;
	}

	responseBody = writeBuf.data;

	curl_slist_free_all(hdrs);
	curl_easy_cleanup(curl);
	return true;
}

bool HTTPTransport::doDELETE() {
	CURL* curl = curl_easy_init();
	if (!curl) return false;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

	curl_slist* hdrs = nullptr;
	if (!sessionId.empty()) {
		std::string sessionHeader = "Mcp-Session-Id: " + sessionId;
		hdrs = curl_slist_append(hdrs, sessionHeader.c_str());
	}

	for (auto& [key, val] : headers) {
		std::string h = key + ": " + val;
		hdrs = curl_slist_append(hdrs, h.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

	curl_easy_perform(curl);

	curl_slist_free_all(hdrs);
	curl_easy_cleanup(curl);
	return true;
}

void HTTPTransport::parseSSEStream(const std::string& stream) {
	std::istringstream iss(stream);
	std::string line;

	while (std::getline(iss, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty() || line[0] == ':')
			continue;

		if (line.find("data: ") == 0) {
			std::string data = line.substr(6);
			if (data.empty() || data == "[DONE]")
				continue;

			try {
				auto msg = json::parse(data);
				pendingMessages.push_back(std::move(msg));
			} catch (...) {}
		}
	}
}

void HTTPTransport::parseLegacySSE(const std::string& stream) {
	std::istringstream iss(stream);
	std::string line;
	std::string currentEvent;

	while (std::getline(iss, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.empty()) {
			currentEvent.clear();
			continue;
		}

		if (line.find("event: ") == 0) {
			currentEvent = line.substr(7);
		} else if (line.find("data: ") == 0) {
			std::string data = line.substr(6);

			if (currentEvent == "endpoint") {
				legacyEndpoint = data;
				return;
			}

			try {
				auto msg = json::parse(data);
				pendingMessages.push_back(std::move(msg));
			} catch (...) {}
		}
	}
}

bool HTTPTransport::connectInitialize() {
	json initReq = {
		{"jsonrpc", "2.0"},
		{"id", 0},
		{"method", "initialize"},
		{"params", {
			{"protocolVersion", "2025-03-26"},
			{"capabilities", json::object()},
			{"clientInfo", {
				{"name", "cpp-mcp-client"},
				{"version", "2.0.0"}
			}}
		}}
	};

	std::string body = initReq.dump();
	std::string responseBody;
	std::string contentType;
	long httpCode = 0;

	if (!doPOST(body, responseBody, contentType, httpCode)) {
		if (httpCode == 405 || httpCode == 404) {
			std::cerr << "[HTTPTransport] POST failed with " << httpCode
					  << ", trying legacy SSE transport...\n";

			long getCode = 0;
			std::string getBody;
			if (doGET(getBody, getCode) && getCode == 200) {
				parseLegacySSE(getBody);
				if (!legacyEndpoint.empty()) {
					useLegacySSE = true;
					connected = true;
					std::cerr << "[HTTPTransport] Legacy SSE endpoint: " << legacyEndpoint << "\n";

					json notif = {
						{"jsonrpc", "2.0"},
						{"method", "notifications/initialized"},
						{"params", json::object()}
					};
					std::string notifBody = notif.dump();
					std::string notifResp;
					std::string notifType;
					long notifCode = 0;
					doPOST(notifBody, notifResp, notifType, notifCode);

					return true;
				}
			}
			std::cerr << "[HTTPTransport] Failed to connect via legacy SSE\n";
			return false;
		}
		std::cerr << "[HTTPTransport] Failed to connect: HTTP " << httpCode << "\n";
		return false;
	}

	if (httpCode >= 400) {
		std::cerr << "[HTTPTransport] Initialize failed: HTTP " << httpCode << "\n";
		if (!responseBody.empty())
			std::cerr << responseBody << "\n";
		return false;
	}

	pendingMessages.clear();
	if (contentType.find("text/event-stream") != std::string::npos) {
		parseSSEStream(responseBody);
	} else if (!responseBody.empty()) {
		try {
			auto msg = json::parse(responseBody);
			pendingMessages.push_back(std::move(msg));
		} catch (...) {}
	}

	json initResp;
	if (!pendingMessages.empty()) {
		initResp = pendingMessages[0];
		pendingMessages.erase(pendingMessages.begin());
	} else {
		std::cerr << "[HTTPTransport] No response to initialize\n";
		return false;
	}

	if (initResp.contains("error")) {
		std::cerr << "[HTTPTransport] Initialize error: " << initResp["error"].dump() << "\n";
		return false;
	}

	connected = true;

	json notif = {
		{"jsonrpc", "2.0"},
		{"method", "notifications/initialized"},
		{"params", json::object()}
	};
	std::string notifBody = notif.dump();
	std::string notifResp;
	std::string notifType;
	long notifCode = 0;
	doPOST(notifBody, notifResp, notifType, notifCode);

	return true;
}

bool HTTPTransport::send(const std::string& message) {
	std::string responseBody;
	std::string contentType;
	long httpCode = 0;

	if (!doPOST(message, responseBody, contentType, httpCode)) {
		std::cerr << "[HTTPTransport] POST failed\n";
		return false;
	}

	if (httpCode == 404) {
		std::cerr << "[HTTPTransport] Session expired (404), reconnecting...\n";
		connected = false;
		pendingMessages.clear();
		pendingIndex = 0;
		if (!connectInitialize()) {
			std::cerr << "[HTTPTransport] Reconnection failed\n";
			return false;
		}
		return doPOST(message, responseBody, contentType, httpCode);
	}

	if (httpCode == 202) {
		return true;
	}

	if (httpCode >= 400) {
		std::cerr << "[HTTPTransport] POST error: HTTP " << httpCode << "\n";
		return false;
	}

	pendingMessages.clear();
	pendingIndex = 0;

	if (contentType.find("text/event-stream") != std::string::npos) {
		parseSSEStream(responseBody);
	} else if (!responseBody.empty()) {
		try {
			auto msg = json::parse(responseBody);
			pendingMessages.push_back(std::move(msg));
		} catch (...) {}
	}

	return true;
}

std::string HTTPTransport::recv() {
	if (pendingIndex < pendingMessages.size()) {
		std::string result = pendingMessages[pendingIndex].dump();
		pendingIndex++;
		if (pendingIndex >= pendingMessages.size()) {
			pendingMessages.clear();
			pendingIndex = 0;
		}
		return result;
	}

	pendingMessages.clear();
	pendingIndex = 0;

	long httpCode = 0;
	std::string responseBody;
	if (doGET(responseBody, httpCode) && httpCode == 200) {
		if (responseBody.find("text/event-stream") != std::string::npos ||
			responseBody.find("data: ") != std::string::npos) {
			parseSSEStream(responseBody);
			if (!pendingMessages.empty()) {
				std::string result = pendingMessages[0].dump();
				pendingMessages.clear();
				return result;
			}
		}
	}

	return "";
}
