cmake_minimum_required(VERSION 3.22)
project(VoxPill VERSION 1.0.0)
# static MSVC runtime so users do not need the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

include(FetchContent)
set(VOXPILL_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")
if(VOXPILL_JUCE_PATH)
  add_subdirectory(${VOXPILL_JUCE_PATH} juce)
else()
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                            GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(VoxPill
  COMPANY_NAME "VoxPill"
  PLUGIN_MANUFACTURER_CODE Vpil
  PLUGIN_CODE Vpl1
  FORMATS VST3 Standalone
  PRODUCT_NAME "VoxPill"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx)

target_sources(VoxPill PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_features(VoxPill PRIVATE cxx_std_17)
target_compile_definitions(VoxPill PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(VoxPill PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
