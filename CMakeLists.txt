cmake_minimum_required(VERSION 3.20)

project(jarvis)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/whisper.cpp)
add_subdirectory(external/json)
find_package(CURL REQUIRED)

add_executable(jarvis
    src/main.cpp
	src/Whisper.cpp
	src/Microphone.cpp
	src/Ollama.cpp
)

target_include_directories(jarvis PRIVATE
	external/miniaudio
)

target_link_libraries(jarvis PRIVATE whisper)
target_link_libraries(jarvis PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(jarvis PRIVATE CURL::libcurl)
