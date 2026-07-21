#pragma once

#include "Transport.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

using json = nlohmann::json;

class MCPConnection {
public:
	MCPConnection(std::string name, std::unique_ptr<Transport> transport);
	~MCPConnection() = default;

	MCPConnection(const MCPConnection&) = delete;
	MCPConnection& operator=(const MCPConnection&) = delete;
	MCPConnection(MCPConnection&&) = default;
	MCPConnection& operator=(MCPConnection&&) = default;

	bool connect();
	bool is_connected() const;
	const std::string& getName() const;

	json listTools();
	json callTool(const std::string& name, const json& arguments);

private:
	std::string name;
	std::unique_ptr<Transport> transport;
	int requestId = 0;
	bool connected = false;

	json sendRequest(const std::string& method, const json& params);
	void sendNotification(const std::string& method, const json& params);
};
