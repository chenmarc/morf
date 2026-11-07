# morf plugin: Standalone app, VST3 and AU (AU on macOS only).
include(FetchContent)

if(MORF_JUCE_DIR)
  add_subdirectory(${MORF_JUCE_DIR} ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(juce)
endif()

set(MORF_FORMATS Standalone VST3)
if(APPLE)
  list(APPEND MORF_FORMATS AU)
endif()

juce_add_plugin(morf_plugin
  PRODUCT_NAME "morf"
  COMPANY_NAME "morf"
  BUNDLE_ID "com.morf.morf"
  PLUGIN_MANUFACTURER_CODE Mrfm
  PLUGIN_CODE Mrf1
  FORMATS ${MORF_FORMATS}
  # An instrument with an audio input: Logic exposes the input as a side-chain,
  # Ableton lets a MIDI track take "MIDI From" this plugin.
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  VST3_CATEGORIES Instrument Synth
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "morf listens to your guitar input to convert it to MIDI."
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(morf_plugin PRIVATE PluginProcessor.cpp PluginEditor.cpp)

target_compile_definitions(morf_plugin PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(morf_plugin
  PRIVATE
    morf_core
    juce::juce_audio_utils
    juce::juce_audio_devices
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Host-style integration test: the real processor, odd block sizes, stereo input.
juce_add_console_app(morf_plugin_test PRODUCT_NAME "morf_plugin_test")
# Compiles the processor sources directly (a plugin target's code can't be linked into an app).
target_sources(morf_plugin_test PRIVATE ../tests/plugin_test.cpp PluginProcessor.cpp PluginEditor.cpp)
target_compile_definitions(morf_plugin_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="morf")
target_link_libraries(morf_plugin_test PRIVATE morf_bench juce::juce_audio_utils juce::juce_audio_devices
  juce::juce_recommended_config_flags)
add_test(NAME morf_plugin_test COMMAND morf_plugin_test)
