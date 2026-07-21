#include "MCPConnection.h"
#include <stdexcept>
#include <iostream>

MCPConnection::MCPConnection(std::string name, std::unique_ptr<Transport> transport)
	: name(std::move(name)), transport(std::move(transport)) {}

bool MCPConnection::connect() {
	if (!transport->open()) {
		std::cerr << "[MCP:" << name << "] Transport failed to open\n";
		return false;
	}

	json initParams = {
		{"protocolVersion", "2025-03-26"},
		{"capabilities", json::object()},
		{"clientInfo", {
			{"name", "cpp-mcp-client"},
			{"version", "2.0.0"}
		}}
	};

	try {
		json resp = sendRequest("initialize", initParams);
		if (resp.contains("error")) {
			std::cerr << "[MCP:" << name << "] Initialize error: "
					  << resp["error"].dump() << "\n";
			return false;
		}
	} catch (const std::exception& e) {
		std::cerr << "[MCP:" << name << "] Initialize failed: " << e.what() << "\n";
		return false;
	}

	try {
		sendNotification("notifications/initialized", json::object());
	} catch (const std::exception& e) {
		std::cerr << "[MCP:" << name << "] Initialized notification failed: " << e.what() << "\n";
		return false;
	}

	connected = true;
	return true;
}

bool MCPConnection::is_connected() const {
	return connected;
}

const std::string& MCPConnection::getName() const {
	return name;
}

json MCPConnection::sendRequest(const std::string& method, const json& params) {
	int id = requestId++;

	json req = {
		{"jsonrpc", "2.0"},
		{"id", id},
		{"method", method},
		{"params", params}
	};

	std::string reqStr = req.dump();
	if (!transport->send(reqStr)) {
		throw std::runtime_error("Failed to send request: " + method);
	}

	while (true) {
		std::string line = transport->recv();
		if (line.empty()) {
			throw std::runtime_error("Server closed connection waiting for response to '" + method + "'");
		}

		json resp = json::parse(line, nullptr, false);
		if (resp.is_discarded()) {
			continue;
		}

		if (resp.contains("id") && !resp["id"].is_null() && resp["id"] == id) {
			return resp;
		}

		if (resp.contains("method") && !resp.contains("id")) {
			continue;
		}
	}
}

void MCPConnection::sendNotification(const std::string& method, const json& params) {
	json note = {
		{"jsonrpc", "2.0"},
		{"method", method},
		{"params", params}
	};

	if (!transport->send(note.dump())) {
		throw std::runtime_error("Failed to send notification: " + method);
	}
}

json MCPConnection::listTools() {
	json tools = json::array();
	std::string cursor;
	bool haveCursor = false;

	while (true) {
		json params = json::object();
		if (haveCursor) params["cursor"] = cursor;

		json resp = sendRequest("tools/list", params);

		if (resp.contains("error")) {
			throw std::runtime_error("listTools: " + resp["error"].dump());
		}

		if (!resp.contains("result") || !resp["result"].contains("tools")) break;

		for (const auto& tool : resp["result"]["tools"]) {
			json entry = {
				{"name", tool.value("name", "")},
				{"description", tool.value("description", "")},
				{"input_schema", tool.contains("inputSchema")
					? tool["inputSchema"]
					: json::object({{"type", "object"}, {"properties", json::object()}})}
			};
			tools.push_back(std::move(entry));
		}

		if (resp["result"].contains("nextCursor") && !resp["result"]["nextCursor"].is_null()) {
			cursor = resp["result"]["nextCursor"].get<std::string>();
			haveCursor = true;
		} else {
			break;
		}
	}

	return tools;
}

json MCPConnection::callTool(const std::string& name, const json& arguments) {
	json params = {
		{"name", name},
		{"arguments", arguments}
	};

	json resp = sendRequest("tools/call", params);

	if (resp.contains("error")) {
		throw std::runtime_error("callTool '" + name + "': " + resp["error"].dump());
	}

	return resp.value("result", json::object());
}
