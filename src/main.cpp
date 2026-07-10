#include "Microphone.h"
#include "Whisper.h"

#include <iostream>
#include <thread>


int main()
{
	Microphone mic;

	Whisper whisper("models/ggml-base.en.bin");
	std::cout << "[JARVIS] Successfully loaded voice-to-text model. \n";

	mic.start();

	std::cout << "[JARVIS] Speak now sentient entity... Press any key to stop";
	std::cin.get();

	auto audio = mic.getAudio();
	std::cout << "[JARVIS] Processing Prompt\n";
	std::string text = whisper.transcribe(audio);

	std::cout << "[JARVIS] Perceived Text: " << text << "\n";

	std::cout << "[JARVIS] Contacting LLM...." << text;
}
