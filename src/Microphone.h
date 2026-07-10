#pragma once

#include <vector>
#include <mutex>

#include "miniaudio.h"

class Microphone {
public:
	Microphone();
	~Microphone();

	bool start();
	void stop();

	// Gets all audio captured since last call
	std::vector<float> getAudio();

private:
	ma_device device;

	std::vector<float> buffer;
	std::mutex mutex;

	static void callback(
		ma_device* device,
		void* output,
		const void* input,
		ma_uint32 frameCount
	);

	void processSamples(
		const float* samples,
		ma_uint32 frameCount
	);
};
