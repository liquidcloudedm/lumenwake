cmake_minimum_required(VERSION 3.22)

# Mac: one binary for Apple Silicon and Intel, macOS 11 or later.
# These have to be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac processor types to build for")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest macOS the plugin runs on")

project(Lumenwake VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(libs/JUCE EXCLUDE_FROM_ALL)

# Formats: VST3 and a standalone app everywhere, plus an Audio Unit on a Mac.
set(LUMENWAKE_FORMATS VST3 Standalone)

if(APPLE)
    list(APPEND LUMENWAKE_FORMATS AU)
endif()

# VST2 (the old ".dll" / ".vst" format) can only be built with Steinberg's
# VST 2.4 SDK. JUCE does not include it and Steinberg stopped licensing it in
# 2018, so this stays off unless you already hold the SDK and an agreement:
#     cmake ... -DLUMENWAKE_VST2_SDK=C:/path/to/vstsdk2.4
set(LUMENWAKE_VST2_SDK "" CACHE PATH "Folder of the VST 2.4 SDK, if you are licensed to use it")

if(LUMENWAKE_VST2_SDK)
    juce_set_vst2_sdk_path("${LUMENWAKE_VST2_SDK}")
    list(APPEND LUMENWAKE_FORMATS VST)
endif()

juce_add_plugin(Lumenwake
    COMPANY_NAME "Lumenwake"
    BUNDLE_ID com.lumenwake.Lumenwake
    PLUGIN_MANUFACTURER_CODE Lmwk
    PLUGIN_CODE Lmw1
    FORMATS ${LUMENWAKE_FORMATS}
    PRODUCT_NAME "Lumenwake"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Analyzer
    VST2_CATEGORY kPlugCategEffect
    AU_MAIN_TYPE kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "The standalone app listens to an audio input to drive the visuals."
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Lumenwake PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Renderer.cpp
    src/ModelLoader.cpp
    src/Recorder.cpp
    src/ThirdParty.c)

target_include_directories(Lumenwake PRIVATE src libs/ufbx libs/cgltf)

# Hold Visual Studio to the C++ standard, so code that builds on Windows is
# less likely to be refused by the Mac compiler.
if(MSVC)
    target_compile_options(Lumenwake PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/permissive->)
endif()

target_compile_definitions(Lumenwake PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Lumenwake
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Small console program that runs each model loader and prints what it found.
option(LUMENWAKE_BUILD_TESTS "Build the model loader test" OFF)

if(LUMENWAKE_BUILD_TESTS)
    juce_add_console_app(lw_modeltest PRODUCT_NAME "lw_modeltest")
    target_sources(lw_modeltest PRIVATE tests/ModelTest.cpp src/ModelLoader.cpp src/ThirdParty.c)
    target_include_directories(lw_modeltest PRIVATE src libs/ufbx libs/cgltf)
    target_compile_definitions(lw_modeltest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(lw_modeltest PRIVATE juce::juce_core juce::juce_recommended_config_flags)

    # Loads the built .vst3 the way a DAW would.
    juce_add_console_app(lw_hosttest PRODUCT_NAME "lw_hosttest")
    target_sources(lw_hosttest PRIVATE tests/HostTest.cpp)
    target_compile_definitions(lw_hosttest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(lw_hosttest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)

    # Writes a test clip with the video writer, plus the raw frames and sound that went in.
    juce_add_console_app(lw_rectest PRODUCT_NAME "lw_rectest")
    target_sources(lw_rectest PRIVATE tests/RecTest.cpp src/Recorder.cpp)
    target_compile_definitions(lw_rectest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(lw_rectest PRIVATE juce::juce_core juce::juce_recommended_config_flags)
endif()
