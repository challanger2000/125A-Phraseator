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
    source/pattern_fragment_remap.cpp
    source/pattern_fragment_remap.h
    source/slicer.cpp
    source/slicer.h
    source/pitch_mapper.cpp
    source/pitch_mapper.h
    source/pitch_detector.cpp
    source/pitch_detector.h
    source/parameters.h
    source/project_state.h
    source/owned_audio_source.h
    source/wav_decoder.cpp
    source/wav_decoder.h
    source/sample_bank.cpp
    source/sample_bank.h
    source/sample_file_loader.cpp
    source/sample_file_loader.h
    source/sample_load_worker.cpp
    source/sample_load_worker.h
    source/phrase_fx.cpp
    source/phrase_fx.h
)

add_library(PhraseatorCore STATIC ${PHRASEATOR_CORE_SOURCES})
target_include_directories(PhraseatorCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_features(PhraseatorCore PUBLIC cxx_std_17)

add_executable(PhraseatorEngineTests tests/phrase_engine_tests.cpp)
target_link_libraries(PhraseatorEngineTests PRIVATE PhraseatorCore)

add_executable(PhraseatorSourcePoolTests tests/source_pool_tests.cpp)
target_link_libraries(PhraseatorSourcePoolTests PRIVATE PhraseatorCore)

add_executable(PhraseatorPatternRemapTests tests/pattern_fragment_remap_tests.cpp)
target_link_libraries(PhraseatorPatternRemapTests PRIVATE PhraseatorCore)

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

add_executable(PhraseatorPitchDetectorTests tests/pitch_detector_tests.cpp)
target_link_libraries(PhraseatorPitchDetectorTests PRIVATE PhraseatorCore)

add_executable(PhraseatorStateContractTests tests/state_contract_tests.cpp)
target_link_libraries(PhraseatorStateContractTests PRIVATE PhraseatorCore)

add_executable(PhraseatorWavDecoderTests tests/wav_decoder_tests.cpp)
target_link_libraries(PhraseatorWavDecoderTests PRIVATE PhraseatorCore)

add_executable(PhraseatorSampleBankTests tests/sample_bank_tests.cpp)
target_link_libraries(PhraseatorSampleBankTests PRIVATE PhraseatorCore)

add_executable(PhraseatorSampleFileLoaderTests tests/sample_file_loader_tests.cpp)
target_link_libraries(PhraseatorSampleFileLoaderTests PRIVATE PhraseatorCore)

add_executable(PhraseatorSampleLoadWorkerTests tests/sample_load_worker_tests.cpp)
target_link_libraries(PhraseatorSampleLoadWorkerTests PRIVATE PhraseatorCore)

add_executable(PhraseatorFxTests tests/phrase_fx_tests.cpp)
target_link_libraries(PhraseatorFxTests PRIVATE PhraseatorCore)

add_executable(PhraseatorMidiNoteTrackerTests tests/midi_note_tracker_tests.cpp)
target_link_libraries(PhraseatorMidiNoteTrackerTests PRIVATE PhraseatorCore)

add_executable(PhraseatorAtomicSnapshotTests tests/atomic_snapshot_tests.cpp)
target_link_libraries(PhraseatorAtomicSnapshotTests PRIVATE PhraseatorCore)

enable_testing()
add_test(NAME PhraseatorEngineTests COMMAND PhraseatorEngineTests)
add_test(NAME PhraseatorSourcePoolTests COMMAND PhraseatorSourcePoolTests)
add_test(NAME PhraseatorPatternRemapTests COMMAND PhraseatorPatternRemapTests)
add_test(NAME PhraseatorPlaybackTests COMMAND PhraseatorPlaybackTests)
add_test(NAME PhraseatorStepClockTests COMMAND PhraseatorStepClockTests)
add_test(NAME PhraseatorSchedulerTests COMMAND PhraseatorSchedulerTests)
add_test(NAME PhraseatorSlicerTests COMMAND PhraseatorSlicerTests)
add_test(NAME PhraseatorPitchTests COMMAND PhraseatorPitchTests)
add_test(NAME PhraseatorPitchDetectorTests COMMAND PhraseatorPitchDetectorTests)
add_test(NAME PhraseatorStateContractTests COMMAND PhraseatorStateContractTests)
add_test(NAME PhraseatorWavDecoderTests COMMAND PhraseatorWavDecoderTests)
add_test(NAME PhraseatorSampleBankTests COMMAND PhraseatorSampleBankTests)
add_test(NAME PhraseatorSampleFileLoaderTests COMMAND PhraseatorSampleFileLoaderTests)
add_test(NAME PhraseatorSampleLoadWorkerTests COMMAND PhraseatorSampleLoadWorkerTests)
add_test(NAME PhraseatorFxTests COMMAND PhraseatorFxTests)
add_test(NAME PhraseatorMidiNoteTrackerTests COMMAND PhraseatorMidiNoteTrackerTests)
add_test(NAME PhraseatorAtomicSnapshotTests COMMAND PhraseatorAtomicSnapshotTests)

if(PHRASEATOR_BUILD_VST3)
    include(FetchContent)

    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
    set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        vst3sdk
        GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
        GIT_TAG v3.8.1_build_84
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(vst3sdk)

    if(WIN32)
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PatchVstguiWin32Drop.cmake")
        phraseator_patch_vstgui_windows_drop(
            "${vst3sdk_SOURCE_DIR}/vstgui4/vstgui/lib/platform/win32/win32datapackage.cpp"
        )
    endif()

    set(PHRASEATOR_VST3_SOURCES
        vst3/AtomicSnapshot.h
        vst3/PhraseatorIDs.h
        vst3/PhraseatorProcessor.h
        vst3/PhraseatorProcessor.cpp
        vst3/PhraseatorController.h
        vst3/PhraseatorController.cpp
        vst3/PhraseatorViews.h
        vst3/PhraseatorViews.cpp
        vst3/branding_master.h
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
            vstgui_support
            PhraseatorCore
    )

    target_compile_features(Phraseator PRIVATE cxx_std_17)

    smtg_target_add_plugin_resources(Phraseator
        RESOURCES
            resource/Phraseator.uidesc
    )

    if(WIN32)
        target_sources(
            Phraseator
            PRIVATE
                ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp
        )
    endif()

    smtg_target_configure_version_file(Phraseator)
endif()
