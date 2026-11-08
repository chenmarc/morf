# Engine and test-bench unit tests.
add_executable(morf_engine_tests engine_tests.cpp)
target_link_libraries(morf_engine_tests PRIVATE morf_bench morf_warnings)
add_test(NAME engine COMMAND morf_engine_tests)

if(MORF_BUILD_PLUGIN)
  # Drives the real plugin processor the way a host does: odd block sizes, stereo input.
  # It compiles the processor sources directly, because a plugin target's code can't be linked into an app.
  juce_add_console_app(morf_plugin_tests PRODUCT_NAME "morf_plugin_tests")
  target_sources(morf_plugin_tests PRIVATE
    plugin_tests.cpp
    ../plugin/source/PluginProcessor.cpp
    ../plugin/source/PluginEditor.cpp)
  target_include_directories(morf_plugin_tests PRIVATE ../plugin/source)
  target_compile_definitions(morf_plugin_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="morf")
  target_link_libraries(morf_plugin_tests PRIVATE
    morf_bench juce::juce_audio_utils juce::juce_audio_devices juce::juce_recommended_config_flags)
  set_target_properties(morf_plugin_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  add_test(NAME plugin COMMAND morf_plugin_tests)

  # Loads the built morf.vst3 through the VST3 API, like a DAW, and plays guitar into it.
  juce_add_console_app(morf_vst3_host_tests PRODUCT_NAME "morf_vst3_host_tests")
  target_sources(morf_vst3_host_tests PRIVATE vst3_host_tests.cpp)
  target_compile_definitions(morf_vst3_host_tests PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(morf_vst3_host_tests PRIVATE
    morf_bench juce::juce_audio_processors juce::juce_recommended_config_flags)
  set_target_properties(morf_vst3_host_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  add_dependencies(morf_vst3_host_tests morf_plugin_VST3)
  get_target_property(morf_vst3_file morf_plugin_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
  add_test(NAME vst3_host COMMAND morf_vst3_host_tests "${morf_vst3_file}")
endif()
