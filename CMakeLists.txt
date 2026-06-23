cmake_minimum_required(VERSION 3.22)

project(LM_One VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build a universal binary (Apple Silicon + Intel) so the plugin runs on any Mac.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Pull JUCE automatically. Check https://github.com/juce-framework/JUCE/releases
# and bump GIT_TAG to the latest 8.x tag if a newer one exists.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# The plugin target. IS_SYNTH + NEEDS_MIDI_INPUT make this an instrument (AUi).
# COPY_PLUGIN_AFTER_BUILD installs it into your user plugin folders so LUNA
# (and other hosts) can find it immediately after a build.
# ---------------------------------------------------------------------------
juce_add_plugin(LM_One
    COMPANY_NAME                "Anthony Coffey"
    BUNDLE_ID                   "com.anthonycoffey.lmone"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    PLUGIN_MANUFACTURER_CODE    Ynme               # 4 chars, must contain 1 uppercase
    PLUGIN_CODE                 Lm01               # 4 chars, must contain 1 uppercase
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "LM-1")

juce_generate_juce_header(LM_One)

target_sources(LM_One PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/DrumKit.cpp
    src/VoiceStripComponent.cpp
    src/StepGridComponent.cpp)

target_compile_definitions(LM_One PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LM_One
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Embedded assets: WAV/AIFF kit samples (assets/factory_kit/) + UI images
# (assets/*.jpg|jpeg|png, e.g. wood.jpeg). When no kit audio is present the
# plugin uses procedural fallback sounds. Re-run CMake configure after changes.
# ---------------------------------------------------------------------------
file(GLOB LMONE_KIT_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/factory_kit/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/factory_kit/*.aif"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/factory_kit/*.aiff")
file(GLOB LMONE_IMG_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.jpg"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.jpeg"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png")

set(LMONE_BINARY_ASSETS ${LMONE_KIT_ASSETS} ${LMONE_IMG_ASSETS})

if(LMONE_BINARY_ASSETS)
    juce_add_binary_data(LMOneData SOURCES ${LMONE_BINARY_ASSETS})
    target_link_libraries(LM_One PRIVATE LMOneData)
    target_compile_definitions(LM_One PRIVATE LMONE_HAS_BINARY_DATA=1)
    message(STATUS "LM-1: embedding assets -> ${LMONE_BINARY_ASSETS}")
else()
    target_compile_definitions(LM_One PRIVATE LMONE_HAS_BINARY_DATA=0)
endif()

if(LMONE_KIT_ASSETS)
    target_compile_definitions(LM_One PRIVATE LMONE_HAS_BINARY_KIT=1)
else()
    target_compile_definitions(LM_One PRIVATE LMONE_HAS_BINARY_KIT=0)
    message(STATUS "LM-1: no factory kit audio; using procedural fallback")
endif()
