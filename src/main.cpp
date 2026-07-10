#include <iostream>
#include "Whisper.h"

int main () {
	std::cout << "wow build system worked" << std::endl;

	Whisper whisper("models/ggml-base.en.bin"); // only english for now

	std::vector<float> audio;
	
	std::string out = whisper.transcribe(audio);

	std::cout << "Voice to Text output: " << out << std::endl;
	return 0;
}
