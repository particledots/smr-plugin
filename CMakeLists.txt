cmake_minimum_required(VERSION 3.22)

# Must be set before project(): universal binary, macOS 12 minimum (Intel Mac runs 12.7.8).
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
endif()

project(UnofficialSMR VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- src/core: DSP only. Plain C++, no JUCE. ---
add_library(smr_core STATIC src/core/PassThrough.cpp)
target_include_directories(smr_core PUBLIC src/core)
set_target_properties(smr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Headless test of the core:  cmake --build build --target core_test && build/core_test ---
add_executable(core_test EXCLUDE_FROM_ALL tests/core_test.cpp)
target_link_libraries(core_test PRIVATE smr_core)

# Lets the core and its test be configured on any machine without downloading JUCE.
option(SMR_CORE_ONLY "Configure only src/core and tests (no JUCE, no plugin)" OFF)
if(SMR_CORE_ONLY)
  return()
endif()

# --- JUCE (same version as the Beads plugin) ---
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.6
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# --- src/plugin: the AU and VST3 ---
juce_add_plugin(SMR
  COMPANY_NAME "particledots"
  PLUGIN_MANUFACTURER_CODE Ptdt
  PLUGIN_CODE Smru
  FORMATS AU VST3
  PRODUCT_NAME "Unofficial SMR"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  AU_MAIN_TYPE kAudioUnitType_Effect
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SMR PRIVATE
  src/plugin/PluginProcessor.cpp
  src/plugin/PluginEditor.cpp)
target_compile_definitions(SMR PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SMR PRIVATE
  smr_core
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)
