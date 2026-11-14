cmake_minimum_required(VERSION 3.22)

# macOS 11 (Big Sur) on Intel is the minimum; build universal (x86_64 + arm64) on CI.
# No -march flags anywhere: the code is scalar C++ and runs on any x86-64 (no AVX2 needed).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # static CRT: no VC++ redist needed
endif()
project(Platterbox VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core engine (no JUCE dependency, unit-testable) ----
add_library(platterbox_engine STATIC Source/engine/Engine.cpp)
target_include_directories(platterbox_engine PUBLIC Source/engine)
set_target_properties(platterbox_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(PLATTERBOX_BUILD_TESTS "Build engine tests" ON)
if(PLATTERBOX_BUILD_TESTS)
    add_executable(platterbox_tests tests/EngineTests.cpp)
    target_link_libraries(platterbox_tests PRIVATE platterbox_engine)
    enable_testing()
    add_test(NAME engine COMMAND platterbox_tests)
endif()

option(PLATTERBOX_BUILD_PLUGIN "Build the JUCE plugin" ON)
if(NOT PLATTERBOX_BUILD_PLUGIN)
    return()
endif()

# ---- JUCE ----
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Platterbox
    COMPANY_NAME "PlatterboxAudio"
    PLUGIN_MANUFACTURER_CODE Ptbx
    PLUGIN_CODE Plb1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Platterbox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Sampler)

target_sources(Platterbox PRIVATE
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp)

target_compile_definitions(Platterbox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Platterbox
    PRIVATE
        platterbox_engine
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Smoke test / screenshot tool (not shipped) ----
option(PLATTERBOX_BUILD_TOOLS "Build the smoke-test tool" OFF)
if(PLATTERBOX_BUILD_TOOLS)
    juce_add_console_app(PlatterboxSmoke PRODUCT_NAME "PlatterboxSmoke")
    target_sources(PlatterboxSmoke PRIVATE tools/Smoke.cpp Source/plugin/PluginProcessor.cpp Source/plugin/PluginEditor.cpp)
    target_compile_definitions(PlatterboxSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Platterbox")
    target_link_libraries(PlatterboxSmoke PRIVATE platterbox_engine juce::juce_audio_utils juce::juce_audio_formats
        PUBLIC juce::juce_recommended_config_flags)
endif()
