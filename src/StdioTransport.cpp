#include "StdioTransport.h"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <iostream>

StdioTransport::StdioTransport(const std::string& command,
							   const std::map<std::string, std::string>& env)
	: command(command), env(env) {}

StdioTransport::~StdioTransport() {
	close();
}

StdioTransport::StdioTransport(StdioTransport&& other) noexcept
	: command(std::move(other.command)),
	  env(std::move(other.env)),
	  serverInput(other.serverInput),
	  serverOutput(other.serverOutput),
	  serverPid(other.serverPid),
	  readBuffer(std::move(other.readBuffer))
{
	other.serverInput = -1;
	other.serverOutput = -1;
	other.serverPid = -1;
}

StdioTransport& StdioTransport::operator=(StdioTransport&& other) noexcept {
	if (this != &other) {
		close();
		command = std::move(other.command);
		env = std::move(other.env);
		serverInput = other.serverInput;
		serverOutput = other.serverOutput;
		serverPid = other.serverPid;
		readBuffer = std::move(other.readBuffer);
		other.serverInput = -1;
		other.serverOutput = -1;
		other.serverPid = -1;
	}
	return *this;
}

bool StdioTransport::open() {
	int inPipe[2];
	int outPipe[2];

	if (pipe(inPipe) != 0 || pipe(outPipe) != 0) {
		std::cerr << "StdioTransport::open: pipe() failed: " << strerror(errno) << "\n";
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		std::cerr << "StdioTransport::open: fork() failed: " << strerror(errno) << "\n";
		::close(inPipe[0]); ::close(inPipe[1]);
		::close(outPipe[0]); ::close(outPipe[1]);
		return false;
	}

	if (pid == 0) {
		::close(inPipe[1]);
		::close(outPipe[0]);

		dup2(inPipe[0], STDIN_FILENO);
		dup2(outPipe[1], STDOUT_FILENO);

		::close(inPipe[0]);
		::close(outPipe[1]);

		for (auto& [key, val] : env) {
			setenv(key.c_str(), val.c_str(), 1);
		}

		execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
		std::cerr << "StdioTransport::open: execl failed: " << strerror(errno) << "\n";
		_exit(127);
	}

	::close(inPipe[0]);
	::close(outPipe[1]);

	serverInput = inPipe[1];
	serverOutput = outPipe[0];
	serverPid = pid;

	return true;
}

void StdioTransport::close() {
	if (serverInput != -1) {
		::close(serverInput);
		serverInput = -1;
	}
	if (serverOutput != -1) {
		::close(serverOutput);
		serverOutput = -1;
	}
	if (serverPid > 0) {
		int status;
		pid_t r = waitpid(serverPid, &status, WNOHANG);
		if (r == 0) {
			kill(serverPid, SIGTERM);
			waitpid(serverPid, &status, 0);
		}
		serverPid = -1;
	}
}

bool StdioTransport::is_open() const {
	return serverPid > 0 && serverInput != -1 && serverOutput != -1;
}

bool StdioTransport::send(const std::string& message) {
	return writeAll(serverInput, message + "\n");
}

std::string StdioTransport::recv() {
	return readLine();
}

bool StdioTransport::writeAll(int fd, const std::string& data) {
	size_t written = 0;
	while (written < data.size()) {
		ssize_t n = ::write(fd, data.data() + written, data.size() - written);
		if (n < 0) {
			if (errno == EINTR) continue;
			return false;
		}
		written += static_cast<size_t>(n);
	}
	return true;
}

std::string StdioTransport::readLine() {
	size_t pos = readBuffer.find('\n');
	if (pos == std::string::npos) {
		char chunk[4096];
		while (true) {
			ssize_t n = ::read(serverOutput, chunk, sizeof(chunk));
			if (n < 0) {
				if (errno == EINTR) continue;
				return "";
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
