# morf plugin: Standalone app, VST3, and AU (AU on macOS only).

set(MORF_FORMATS Standalone VST3)
if(APPLE)
  list(APPEND MORF_FORMATS AU)
endif()

juce_add_plugin(morf_plugin
  PRODUCT_NAME "morf"
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
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

target_sources(morf_plugin PRIVATE source/PluginProcessor.cpp source/PluginEditor.cpp)

# macOS: JUCE signs each bundle and afterwards writes moduleinfo.json into the VST3. That breaks
# the VST3's signature ("a sealed resource is missing or invalid"), and some hosts, Ableton Live 11
# among them, then skip the plugin without an error. Re-sign every bundle (ad-hoc) as the very last
# build step, and fail the build if the signature still doesn't verify.
if(APPLE)
  find_program(MORF_CODESIGN codesign REQUIRED)
  foreach(format IN LISTS MORF_FORMATS)
    set(bundle_target morf_plugin_${format})
    add_custom_command(TARGET ${bundle_target} POST_BUILD
      COMMAND "${MORF_CODESIGN}" --force --sign - "$<TARGET_BUNDLE_DIR:${bundle_target}>"
      COMMAND "${MORF_CODESIGN}" --verify --strict "$<TARGET_BUNDLE_DIR:${bundle_target}>"
      COMMENT "Re-signing ${bundle_target} now that all its files are in place"
      VERBATIM)
  endforeach()
endif()

target_compile_definitions(morf_plugin PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
  target_compile_options(morf_plugin PRIVATE /utf-8)
endif()

target_link_libraries(morf_plugin
  PRIVATE
    morf_core
    juce::juce_audio_utils
    juce::juce_audio_devices
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
