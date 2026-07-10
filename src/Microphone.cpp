#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "Microphone.h"

#include <stdexcept>


Microphone::Microphone()
{
	ma_device_config config =
		ma_device_config_init(
			ma_device_type_capture
		);

	config.capture.format = ma_format_f32;
	config.capture.channels = 1;
	config.sampleRate = 16000;

	config.dataCallback = callback;
	config.pUserData = this;


	if (ma_device_init(
		nullptr,
		&config,
		&device) != MA_SUCCESS)
	{
		throw std::runtime_error(
			"Failed to initialize microphone"
		);
	}
}


Microphone::~Microphone()
{
	stop();

	ma_device_uninit(&device);
}


bool Microphone::start()
{
	return ma_device_start(&device) == MA_SUCCESS;
}


void Microphone::stop()
{
	ma_device_stop(&device);
}


std::vector<float> Microphone::getAudio()
{
	std::lock_guard lock(mutex);

	std::vector<float> result;

	result.swap(buffer);

	return result;
}


void Microphone::callback(
	ma_device* device,
	void* output,
	const void* input,
	ma_uint32 frameCount)
{
	auto mic =
		static_cast<Microphone*>(
			device->pUserData
		);

	const float* samples =
		static_cast<const float*>(input);

	mic->processSamples(
		samples,
		frameCount
	);
}


void Microphone::processSamples(
	const float* samples,
	ma_uint32 frameCount)
{
	std::lock_guard lock(mutex);

	buffer.insert(
		buffer.end(),
		samples,
		samples + frameCount
	);
}
