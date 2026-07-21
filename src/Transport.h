#pragma once

#include <string>

class Transport {
public:
	virtual ~Transport() = default;

	virtual bool open() = 0;
	virtual void close() = 0;

	virtual bool send(const std::string& message) = 0;
	virtual std::string recv() = 0;

	virtual bool is_open() const = 0;
};
