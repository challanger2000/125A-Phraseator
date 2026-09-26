cmake_minimum_required(VERSION 3.25)

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
    source/audio_buffer.h
    source/fragment_player.cpp
    source/fragment_player.h
    source/step_clock.cpp
    source/step_clock.h
    source/phrase_scheduler.cpp
    source/phrase_scheduler.h
    source/slicer.cpp
    source/slicer.h
    source/pitch_mapper.cpp
    source/pitch_mapper.h
    source/parameters.h
    source/project_state.h
)

add_library(PhraseatorCore STATIC ${PHRASEATOR_CORE_SOURCES})
target_include_directories(PhraseatorCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_features(PhraseatorCore PUBLIC cxx_std_17)

add_executable(PhraseatorEngineTests tests/phrase_engine_tests.cpp)
target_link_libraries(PhraseatorEngineTests PRIVATE PhraseatorCore)

add_executable(PhraseatorSourcePoolTests tests/source_pool_tests.cpp)
target_link_libraries(PhraseatorSourcePoolTests PRIVATE PhraseatorCore)

add_executable(PhraseatorPlaybackTests tests/fragment_player_tests.cpp)
target_link_libraries(PhraseatorPlaybackTests PRIVATE PhraseatorCore)

add_executable(PhraseatorStepClockTests tests/step_clock_tests.cpp)
target_link_libraries(PhraseatorStepClockTests PRIVATE PhraseatorCore)

add_executable(PhraseatorSchedulerTests tests/phrase_scheduler_tests.cpp)
target_link_libraries(PhraseatorSchedulerTests PRIVATE PhraseatorCore)

add_executable(PhraseatorSlicerTests tests/slicer_tests.cpp)
target_link_libraries(PhraseatorSlicerTests PRIVATE PhraseatorCore)

add_executable(PhraseatorPitchTests tests/pitch_mapper_tests.cpp)
target_link_libraries(PhraseatorPitchTests PRIVATE PhraseatorCore)

add_executable(PhraseatorStateContractTests tests/state_contract_tests.cpp)
target_link_libraries(PhraseatorStateContractTests PRIVATE PhraseatorCore)

enable_testing()
add_test(NAME PhraseatorEngineTests COMMAND PhraseatorEngineTests)
add_test(NAME PhraseatorSourcePoolTests COMMAND PhraseatorSourcePoolTests)
add_test(NAME PhraseatorPlaybackTests COMMAND PhraseatorPlaybackTests)
add_test(NAME PhraseatorStepClockTests COMMAND PhraseatorStepClockTests)
add_test(NAME PhraseatorSchedulerTests COMMAND PhraseatorSchedulerTests)
add_test(NAME PhraseatorSlicerTests COMMAND PhraseatorSlicerTests)
add_test(NAME PhraseatorPitchTests COMMAND PhraseatorPitchTests)
add_test(NAME PhraseatorStateContractTests COMMAND PhraseatorStateContractTests)

if(PHRASEATOR_BUILD_VST3)
    include(FetchContent)

    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
    set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        vst3sdk
        GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
        GIT_TAG v3.8.1_build_84
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(vst3sdk)

    set(PHRASEATOR_VST3_SOURCES
        vst3/PhraseatorIDs.h
        vst3/PhraseatorProcessor.h
        vst3/PhraseatorProcessor.cpp
        vst3/PhraseatorController.h
        vst3/PhraseatorController.cpp
        vst3/entry.cpp
    )

    smtg_add_vst3plugin(
        Phraseator
        PACKAGE_NAME "125A-Phraseator"
        SOURCES_LIST ${PHRASEATOR_VST3_SOURCES}
    )

    target_link_libraries(
        Phraseator
        PRIVATE
            sdk
            PhraseatorCore
    )

    target_compile_features(Phraseator PRIVATE cxx_std_17)

    if(WIN32)
        target_sources(
            Phraseator
            PRIVATE
                ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp
        )
    endif()

    smtg_target_configure_version_file(Phraseator)
endif()
