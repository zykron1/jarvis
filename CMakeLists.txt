cmake_minimum_required(VERSION 3.20)

project(jarvis)

set(CMAKE_CXX_STANDARD 20)

set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/whisper.cpp)

add_executable(jarvis
    src/main.cpp
)

target_link_libraries(jarvis PRIVATE whisper)
