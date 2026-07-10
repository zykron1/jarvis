#include "Whisper.h"

#include <stdexcept>
#include <utility>

#include <whisper.h>

Whisper::Whisper(const std::string& modelPath) {
	m_ctx = whisper_init_from_file(modelPath.c_str());

	if (!m_ctx)
		throw std::runtime_error("Failed to load Whisper model: " + modelPath);
}

Whisper::~Whisper() {
	if (m_ctx)
		whisper_free(m_ctx);
}

Whisper::Whisper(Whisper&& other) noexcept {
	m_ctx = other.m_ctx;
	other.m_ctx = nullptr;
}

Whisper& Whisper::operator=(Whisper&& other) noexcept {
	if (this != &other) {
		if (m_ctx)
			whisper_free(m_ctx);

		m_ctx = other.m_ctx;
		other.m_ctx = nullptr;
	}

	return *this;
}

std::string Whisper::transcribe(const std::vector<float>& audio) {
	if (audio.empty())
		return "";

	whisper_full_params params =
		whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

	params.print_progress = false;
	params.print_realtime = false;
	params.print_timestamps = false;
	params.print_special = false;

	params.translate = false;
	params.language = "en";

	if (whisper_full(
			m_ctx,
			params,
			audio.data(),
			static_cast<int>(audio.size())) != 0)
	{
		throw std::runtime_error("Whisper transcription failed.");
	}

	std::string result;

	const int segments = whisper_full_n_segments(m_ctx);

	for (int i = 0; i < segments; i++)
		result += whisper_full_get_segment_text(m_ctx, i);

	return result;
}
