cmake_minimum_required(VERSION 3.22)

project(Cutline VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Cutline targets Apple Silicon")

if(DEFINED JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 2cdfca8feb300fb424002ba2c2751569e5bacb64
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

add_library(CutlineState STATIC
    Source/State/StatePersistence.cpp
    Source/State/StatePersistence.h
    Source/Parameters.h
)
target_include_directories(CutlineState PUBLIC Source)
target_link_libraries(CutlineState PUBLIC juce::juce_audio_processors)
target_link_libraries(CutlineState PRIVATE juce::juce_recommended_warning_flags)
target_compile_definitions(CutlineState PUBLIC
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

juce_add_plugin(Cutline
    COMPANY_NAME "m43d4k"
    PLUGIN_MANUFACTURER_CODE M43d
    PLUGIN_CODE Ctln
    FORMATS VST3
    PRODUCT_NAME "Cutline"
    DESCRIPTION "Minimum-phase Butterworth high-pass and low-pass filter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Filter Stereo
)

target_sources(Cutline PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/DSP/Butterworth.h
    Source/DSP/CutlineEngine.h
)
target_include_directories(Cutline PRIVATE Source)
target_link_libraries(Cutline PRIVATE
    CutlineState
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
target_compile_definitions(Cutline PUBLIC
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

if(BUILD_TESTING)
    add_executable(CutlineDspTests Tests/DspTests.cpp)
    target_include_directories(CutlineDspTests PRIVATE Source)
    target_link_libraries(CutlineDspTests PRIVATE juce::juce_recommended_warning_flags)
    add_test(NAME CutlineDspTests COMMAND CutlineDspTests)

    add_executable(CutlineDspBenchmark Tests/DspBenchmark.cpp)
    target_include_directories(CutlineDspBenchmark PRIVATE Source)
    target_link_libraries(CutlineDspBenchmark PRIVATE juce::juce_recommended_warning_flags)

    add_executable(CutlineStateTests Tests/StateTests.cpp)
    target_link_libraries(CutlineStateTests PRIVATE
        CutlineState
        juce::juce_recommended_warning_flags
    )
    add_test(NAME CutlineStateTests COMMAND CutlineStateTests)

    add_executable(CutlineProcessorTests Tests/ProcessorTests.cpp)
    target_include_directories(CutlineProcessorTests PRIVATE Source)
    target_link_libraries(CutlineProcessorTests PRIVATE
        Cutline
        juce::juce_audio_utils
        juce::juce_recommended_warning_flags
    )
    add_test(NAME CutlineProcessorTests COMMAND CutlineProcessorTests)
endif()
