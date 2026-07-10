#pragma once

/*
Not to be confused with `whisper.h`!
Whisper.h is a wrapper of the whisper.cpp library only exposing used functionality in C++ syntax.
*/

#include <string>
#include <vector>

struct whisper_context;

class Whisper {
public:
	explicit Whisper(const std::string& modelPath);
	~Whisper();

	Whisper(const Whisper&) = delete;
	Whisper& operator=(const Whisper&) = delete;

	Whisper(Whisper&& other) noexcept;
	Whisper& operator=(Whisper&& other) noexcept;

	std::string transcribe(const std::vector<float>& audio);

private:
	whisper_context* m_ctx = nullptr;
};
