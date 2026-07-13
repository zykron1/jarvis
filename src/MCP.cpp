#include "MCP.h"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <iostream>

bool MCP::connect(const std::string& command) {
	int inPipe[2];  // parent writes -> child stdin
	int outPipe[2]; // child stdout -> parent reads

	if (pipe(inPipe) != 0 || pipe(outPipe) != 0) {
		std::cerr << "MCP::connect: pipe() failed: " << strerror(errno) << "\n";
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		std::cerr << "MCP::connect: fork() failed: " << strerror(errno) << "\n";
		return false;
	}

	if (pid == 0) {
		dup2(inPipe[0], STDIN_FILENO);
		dup2(outPipe[1], STDOUT_FILENO);

		close(inPipe[0]);
		close(inPipe[1]);
		close(outPipe[0]);
		close(outPipe[1]);

		execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);

		// execl only returns on failure
		std::cerr << "MCP::connect: execl failed: " << strerror(errno) << "\n";
		_exit(127);
	}

	close(inPipe[0]);
	close(outPipe[1]);

	serverInput = inPipe[1];
	serverOutput = outPipe[0];
	serverPid = pid;

	json initParams = {
		{"protocolVersion", "2024-11-05"},
		{"capabilities", json::object()},
		{"clientInfo", {
			{"name", "cpp-mcp-client"},
			{"version", "1.0.0"}
		}}
	};

	json resp;
	try {
		resp = sendRequest("initialize", initParams);
	} catch (const std::exception& e) {
		std::cerr << "MCP::connect: initialize failed: " << e.what() << "\n";
		return false;
	}

	if (resp.contains("error")) {
		std::cerr << "MCP::connect: server returned error during initialize: "
		          << resp["error"].dump() << "\n";
		return false;
	}

	sendNotification("notifications/initialized", json::object());

	return true;
}

MCP::~MCP() {
	if (serverInput != -1) {
		close(serverInput);
		serverInput = -1;
	}
	if (serverOutput != -1) {
		close(serverOutput);
		serverOutput = -1;
	}
	if (serverPid > 0) {
		int status;
		pid_t r = waitpid(serverPid, &status, WNOHANG);
		if (r == 0) {
			kill(serverPid, SIGTERM);
			waitpid(serverPid, &status, 0);
		}
	}
}

bool MCP::writeAll(int fd, const std::string& data) {
	size_t written = 0;
	while (written < data.size()) {
		ssize_t n = write(fd, data.data() + written, data.size() - written);
		if (n < 0) {
			if (errno == EINTR) continue;
			return false;
		}
		written += static_cast<size_t>(n);
	}
	return true;
}

std::string MCP::readLine() {
	size_t pos = readBuffer.find('\n');
	if (pos == std::string::npos) {
		char chunk[4096];
		while (true) {
			ssize_t n = read(serverOutput, chunk, sizeof(chunk));
			if (n < 0) {
				if (errno == EINTR) continue;
				return ""; // read error
			}
			if (n == 0) {
				std::string rest = readBuffer;
				readBuffer.clear();
				return rest;
			}
			readBuffer.append(chunk, static_cast<size_t>(n));
			pos = readBuffer.find('\n');
			if (pos != std::string::npos) break;
		}
	}

	std::string line = readBuffer.substr(0, pos);
	readBuffer.erase(0, pos + 1);
	if (!line.empty() && line.back() == '\r') line.pop_back();
	return line;
}

json MCP::sendRequest(const std::string& method, const json& params) {
	int id = requestId++;

	json req = {
		{"jsonrpc", "2.0"},
		{"id", id},
		{"method", method},
		{"params", params}
	};

	if (!writeAll(serverInput, req.dump() + "\n")) {
		throw std::runtime_error("MCP::sendRequest: failed to write to server stdin");
	}

	while (true) {
		std::string line = readLine();
		if (line.empty()) {
			throw std::runtime_error("MCP::sendRequest: server closed connection (EOF) waiting for response to '" + method + "'");
		}

		json resp = json::parse(line, nullptr, false);
		if (resp.is_discarded()) {
			continue;
		}

		if (resp.contains("id") && !resp["id"].is_null() && resp["id"] == id) {
			return resp;
		}
	}
}

void MCP::sendNotification(const std::string& method, const json& params) {
	json note = {
		{"jsonrpc", "2.0"},
		{"method", method},
		{"params", params}
	};

	if (!writeAll(serverInput, note.dump() + "\n")) {
		throw std::runtime_error("MCP::sendNotification: failed to write to server stdin");
	}
}

 
json MCP::listTools() {
	json tools = json::array();
	std::string cursor; // pagination cursor, empty until the server hands us one
	bool haveCursor = false;
 
	while (true) {
		json params = json::object();
		if (haveCursor) params["cursor"] = cursor;
 
		json resp = sendRequest("tools/list", params);
 
		if (resp.contains("error")) {
			throw std::runtime_error("MCP::listTools: " + resp["error"].dump());
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
 
json MCP::callTool(const std::string& name, const json& arguments) {
	json params = {
		{"name", name},
		{"arguments", arguments}
	};

	json resp = sendRequest("tools/call", params);

	if (resp.contains("error")) {
		throw std::runtime_error("MCP::callTool: " + resp["error"].dump());
	}

	return resp.value("result", json::object());
}
