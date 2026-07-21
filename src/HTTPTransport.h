#pragma once

#include "Transport.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>

using json = nlohmann::json;

class HTTPTransport : public Transport {
public:
	explicit HTTPTransport(const std::string& url,
						   const std::map<std::string, std::string>& headers = {});
	~HTTPTransport() override;

	HTTPTransport(const HTTPTransport&) = delete;
	HTTPTransport& operator=(const HTTPTransport&) = delete;
	HTTPTransport(HTTPTransport&& other) noexcept;
	HTTPTransport& operator=(HTTPTransport&& other) noexcept;

	bool open() override;
	void close() override;

	bool send(const std::string& message) override;
	std::string recv() override;

	bool is_open() const override;

private:
	std::string url;
	std::map<std::string, std::string> headers;
	std::string sessionId;
	bool connected = false;

	std::vector<json> pendingMessages;
	size_t pendingIndex = 0;

	bool useLegacySSE = false;
	std::string legacyEndpoint;

	bool doPOST(const std::string& body, std::string& responseBody,
				std::string& contentType, long& httpCode);
	bool doGET(std::string& responseBody, long& httpCode);
	bool doDELETE();

	void parseSSEStream(const std::string& stream);
	void parseLegacySSE(const std::string& stream);

	bool connectInitialize();
};
