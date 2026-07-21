#pragma once

#include "Transport.h"
#include <string>
#include <map>
#include <sys/types.h>

class StdioTransport : public Transport {
public:
	explicit StdioTransport(const std::string& command,
							const std::map<std::string, std::string>& env = {});
	~StdioTransport() override;

	StdioTransport(const StdioTransport&) = delete;
	StdioTransport& operator=(const StdioTransport&) = delete;
	StdioTransport(StdioTransport&& other) noexcept;
	StdioTransport& operator=(StdioTransport&& other) noexcept;

	bool open() override;
	void close() override;

	bool send(const std::string& message) override;
	std::string recv() override;

	bool is_open() const override;

private:
	std::string command;
	std::map<std::string, std::string> env;

	int serverInput = -1;
	int serverOutput = -1;
	pid_t serverPid = -1;
	std::string readBuffer;

	bool writeAll(int fd, const std::string& data);
	std::string readLine();
};
