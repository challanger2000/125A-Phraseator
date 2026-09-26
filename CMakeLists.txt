cmake_minimum_required(VERSION 3.20)

project(125A_Phraseator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PHRASEATOR_BUILD_VST3 "Build the VST3 plugin target" OFF)

set(PHRASEATOR_CORE_SOURCES
    source/phrase_engine.cpp
    source/phrase_engine.h
    source/source_pool.cpp
    source/source_pool.h
)

add_library(PhraseatorCore STATIC ${PHRASEATOR_CORE_SOURCES})
target_include_directories(PhraseatorCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")

add_executable(PhraseatorEngineTests tests/phrase_engine_tests.cpp)
target_link_libraries(PhraseatorEngineTests PRIVATE PhraseatorCore)

add_executable(PhraseatorSourcePoolTests tests/source_pool_tests.cpp)
target_link_libraries(PhraseatorSourcePoolTests PRIVATE PhraseatorCore)

enable_testing()
add_test(NAME PhraseatorEngineTests COMMAND PhraseatorEngineTests)
add_test(NAME PhraseatorSourcePoolTests COMMAND PhraseatorSourcePoolTests)

if(PHRASEATOR_BUILD_VST3)
    if(NOT DEFINED VST3_SDK_ROOT)
        message(FATAL_ERROR "Set VST3_SDK_ROOT to the Steinberg VST3 SDK 3.7.12 root.")
    endif()

    add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
    smtg_enable_vst3_sdk()

    message(STATUS "VST3 host/plugin layer will be added after the PhraseatorCore engine baseline is validated.")
endif()
