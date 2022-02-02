cmake_minimum_required(VERSION 3.15)

project(BACF_EVAL VERSION 0.0.1)

set(Q_DIR lib/q)
option(Q_BUILD_EXAMPLES "" OFF)
option(Q_BUILD_TEST "" OFF)
option(Q_BUILD_IO "" OFF)

add_subdirectory(JUCE)
add_subdirectory(${Q_DIR})

juce_add_console_app(BacfEval
    PRODUCT_NAME "BACF evaluation")

target_sources(BacfEval
    PRIVATE
        Main.cpp)

target_compile_definitions(BacfEval
    PRIVATE
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_console_app` call
        JUCE_USE_CURL=0)    # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_console_app` call

target_link_libraries(BacfEval
    PRIVATE
        # ConsoleAppData            # If you'd created a binary data target, you'd link to it here
        juce::juce_core
    PUBLIC
        libq
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
