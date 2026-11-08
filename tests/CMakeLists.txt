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
endif()
