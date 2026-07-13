#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class MCP {
public:
	MCP() {};
	~MCP();

	bool connect(const std::string& command);

	json listTools();

	json callTool(
		const std::string& name,
		const json& arguments
	);

private:
	int requestId = 0;
	int serverInput = -1; // write end of pipe -> child's stdin
	int serverOutput = -1; // read end of pipe <- child's stdout
	pid_t serverPid = -1;
	std::string readBuffer; // leftover bytes between reads (line buffering)

	json sendRequest(
		const std::string& method,
		const json& params
	);
	void sendNotification(
		const std::string& method,
		const json& params
	);

	std::string readLine();

	bool writeAll(int fd, const std::string& data);
};
