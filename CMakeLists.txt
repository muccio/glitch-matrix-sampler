cmake_minimum_required(VERSION 3.22)
project(GlitchMatrixSampler VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# JUCE 8 FetchContent
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Optional Frontend build step
find_program(NPM_EXECUTABLE npm)
if (NPM_EXECUTABLE)
    add_custom_target(FrontendBuild
        COMMAND ${NPM_EXECUTABLE} --prefix ${CMAKE_CURRENT_SOURCE_DIR}/Frontend run build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Frontend
        COMMENT "Compiling embedded React/Vite frontend assets..."
    )
endif()

# Source files for the DSP & Audio engine
set(DSP_SOURCES
    Source/DSP/OscillatorSource.cpp
    Source/DSP/NoiseSource.cpp
    Source/DSP/SampleSource.cpp
    Source/DSP/ClickSource.cpp
    Source/DSP/VoiceManager.cpp
)

set(IPC_SOURCES
    Source/IPC/WebBridge.cpp
    Source/IPC/StateSerializer.cpp
)

set(PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${DSP_SOURCES}
    ${IPC_SOURCES}
)

# Declare JUCE plugin
juce_add_plugin(GlitchMatrixSampler
    COMPANY_NAME "AntigravityAudio"
    BUNDLE_ID "com.antigravity.glitchmatrixsampler"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Anti
    PLUGIN_CODE Glmx
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "GlitchMatrixSampler"
)

target_sources(GlitchMatrixSampler
    PRIVATE
        ${PLUGIN_SOURCES}
)

target_compile_definitions(GlitchMatrixSampler
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_include_directories(GlitchMatrixSampler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(GlitchMatrixSampler
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if (TARGET FrontendBuild)
    add_dependencies(GlitchMatrixSampler FrontendBuild)
endif()

# Standalone Headless DSP Test Target
add_executable(GlitchMatrixSampler_Tests
    Tests/DspEngineTests.cpp
    ${DSP_SOURCES}
    Source/IPC/StateSerializer.cpp
)

target_include_directories(GlitchMatrixSampler_Tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(GlitchMatrixSampler_Tests
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(GlitchMatrixSampler_Tests
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_events
        juce::juce_core
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
