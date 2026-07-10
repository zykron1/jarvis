#include "Microphone.h"
#include "Whisper.h"
#include "Ollama.h"

#include <iostream>

int main()
{
	Microphone mic;
	Whisper whisper("models/ggml-tiny.en.bin");
	Ollama lama("http://localhost:11434/api/chat", "qwen2.5:1.5b");

	std::cout << "[JARVIS] Successfully loaded voice-to-text model. \n";

	while (1) {
		mic.start();
		std::cout << "[JARVIS] Speak now sentient entity... Press any key to stop";
		std::cin.get();

		auto audio = mic.getAudio();
		std::cout << "[JARVIS] Processing Prompt\n";
		std::string text = whisper.transcribe(audio);

		std::cout << "[USER] " << text << "\n";

		std::cout << "[JARVIS] Contacting LLM...." << std::endl;
		json output = lama.chat(text);
		std::string message = output["message"]["content"];
		std::cout << "[JARVIS] " << message << std::endl;
	}
}
