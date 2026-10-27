cmake_minimum_required(VERSION 3.22)

# Must be set before project(): universal binary, macOS 12 minimum (Intel Mac runs 12.7.8).
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")
endif()

project(UnofficialSMR VERSION 0.0.4 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SMR_FP_CONTRACT: "off" for every real build. "fast" is only for the M3-k rounding test (smr_render built with fused multiply-add allowed,
# tools/analysis/port_noise.py): the V3 test fails in such a build, so never use it for the tests or the plugin.
set(SMR_FP_CONTRACT "off" CACHE STRING "-ffp-contract mode for the core: off (default) or fast (M3-k experiment only)")
set(SMR_FPFLAGS -ffp-contract=${SMR_FP_CONTRACT})
if(SMR_FP_CONTRACT STREQUAL "fast" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  list(APPEND SMR_FPFLAGS -mfma)
endif()

# --- src/core: DSP only. Plain C++, no JUCE, no hardware code. ---
# -ffp-contract=off: no fused multiply-add (Clang on arm64 and GCC would otherwise fuse). -O2: no -Ofast, no fast-math.
# (docs/architecture.md section 13.3). PassThrough is the Milestone 0 stand-in that the plugin still uses.
add_library(smr_core STATIC
  src/core/PassThrough.cpp
  src/core/Scheduler.cpp
  src/core/Lpf.cpp
  src/core/Tables.cpp
  src/core/Engine.cpp
  src/core/Snapshot.cpp
  src/core/Filter.cpp
  src/core/Params.cpp
  src/core/Rotation.cpp
  src/core/EnvOut.cpp)
target_include_directories(smr_core PUBLIC src/core)
target_compile_features(smr_core PUBLIC cxx_std_17)
if(NOT MSVC)
  target_compile_options(smr_core PRIVATE ${SMR_FPFLAGS} -O2)
  # The upstream table headers (included verbatim) fill nested arrays with flat brace lists.
  set_source_files_properties(src/core/Tables.cpp PROPERTIES COMPILE_OPTIONS "-Wno-missing-braces")
endif()
set_target_properties(smr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- src/host: resampler and block adapter. Plain C++, no JUCE. ---
add_library(smr_host STATIC src/host/Resampler.cpp src/host/BlockAdapter.cpp)
target_include_directories(smr_host PUBLIC src/host)
target_link_libraries(smr_host PUBLIC smr_core)
if(NOT MSVC)
  target_compile_options(smr_host PRIVATE ${SMR_FPFLAGS} -O2)
endif()
set_target_properties(smr_host PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- src/plugin, JUCE-free part: the parameter table, its mapping to core inputs, and the saved-state encoding ---
add_library(smr_params STATIC src/plugin/ParamMap.cpp src/plugin/StateCodec.cpp)
target_include_directories(smr_params PUBLIC src/plugin)
target_link_libraries(smr_params PUBLIC smr_core)
if(NOT MSVC)
  target_compile_options(smr_params PRIVATE ${SMR_FPFLAGS} -O2)
endif()
set_target_properties(smr_params PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Tests and tools (built on request, e.g. cmake --build build --target smr_render) ---
# Same floating-point flags as the core, so the tests exercise the code as it is built.
function(smr_test_target name)
  add_executable(${name} EXCLUDE_FROM_ALL ${ARGN})
  if(NOT MSVC)
    target_compile_options(${name} PRIVATE ${SMR_FPFLAGS} -O2)
  endif()
endfunction()

smr_test_target(core_test tests/core_test.cpp)                       # Milestone 0 pass-through test
target_link_libraries(core_test PRIVATE smr_core)

smr_test_target(smr_core_tests tests/core_tests.cpp)                 # V2, V3, V5, scheduler, adapter, resampler, V4 hash
target_link_libraries(smr_core_tests PRIVATE smr_host smr_core)

smr_test_target(smr_param_tests tests/param_tests.cpp)               # Milestone 4: parameters, mapping, snapshot
target_link_libraries(smr_param_tests PRIVATE smr_params smr_host smr_core)

smr_test_target(port_driver tests/oracle/port_driver.cpp)            # V1: the port side of the differential test
target_link_libraries(port_driver PRIVATE smr_core)
target_include_directories(port_driver PRIVATE tests/oracle)

smr_test_target(gen_stimulus tests/oracle/gen_stimulus.cpp)
target_include_directories(gen_stimulus PRIVATE tests/oracle)

smr_test_target(trace_diff tests/oracle/trace_diff.cpp)

smr_test_target(smr_render tools/smr_render/main.cpp)                # offline renderer
target_link_libraries(smr_render PRIVATE smr_host smr_core)

smr_test_target(wav_tool tests/wav_tool.cpp)                         # V6 helper

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
  smr_params
  smr_host
  smr_core
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)

# --- Milestone 5: the real plugin code run headless (no host), for CI. Built on request only. ---
# Compiles PluginProcessor.cpp and PluginEditor.cpp into a console app. Modes: "selftest" and WAV rendering (tests/plugin_tests.cpp).
juce_add_console_app(smr_plugin_tests PRODUCT_NAME "smr_plugin_tests")
target_sources(smr_plugin_tests PRIVATE
  tests/plugin_tests.cpp
  src/plugin/PluginProcessor.cpp
  src/plugin/PluginEditor.cpp)
target_include_directories(smr_plugin_tests PRIVATE src/plugin tools/smr_render)
target_compile_definitions(smr_plugin_tests PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JucePlugin_Name="Unofficial SMR")
target_link_libraries(smr_plugin_tests PRIVATE
  smr_params
  smr_host
  smr_core
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)
set_target_properties(smr_plugin_tests PROPERTIES EXCLUDE_FROM_ALL ON)
