cmake_minimum_required(VERSION 3.20)

project(125A_Phraseator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED VST3_SDK_ROOT)
    message(FATAL_ERROR "Set VST3_SDK_ROOT to the Steinberg VST3 SDK 3.7.12 root.")
endif()

add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")

smtg_enable_vst3_sdk()

set(PHRASEATOR_SOURCES
    source/phrase_engine.cpp
    source/phrase_engine.h
)

add_library(PhraseatorCore STATIC ${PHRASEATOR_SOURCES})
target_include_directories(PhraseatorCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(PhraseatorEngineTests tests/phrase_engine_tests.cpp)
target_link_libraries(PhraseatorEngineTests PRIVATE PhraseatorCore)

enable_testing()
add_test(NAME PhraseatorEngineTests COMMAND PhraseatorEngineTests)
