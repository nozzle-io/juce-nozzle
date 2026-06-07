cmake_minimum_required(VERSION 3.22)
project(juce_nozzle VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

option(JUCE_NOZZLE_BUILD_EXAMPLES "Build juce-nozzle examples" ON)
set(JUCE_NOZZLE_JUCE_TAG "8.0.13" CACHE STRING "JUCE tag to fetch")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(NOZZLE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NOZZLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(nozzle)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${JUCE_NOZZLE_JUCE_TAG}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

add_library(juce_nozzle STATIC
    src/juce_nozzle_receiver.cpp
)
target_include_directories(juce_nozzle PUBLIC include)
target_link_libraries(juce_nozzle PUBLIC nozzle)
target_compile_features(juce_nozzle PUBLIC cxx_std_17)
target_compile_options(juce_nozzle PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

if(JUCE_NOZZLE_BUILD_EXAMPLES)
    if(APPLE)
        set(JUCE_NOZZLE_PLUGIN_FORMATS Standalone VST3 AU)
    else()
        set(JUCE_NOZZLE_PLUGIN_FORMATS Standalone VST3)
    endif()

    juce_add_plugin(NozzleReceiverPlugin
        COMPANY_NAME "nozzle contributors"
        COMPANY_WEBSITE "https://github.com/nozzle-io/juce-nozzle"
        BUNDLE_ID "org.nozzle-io.juce.receiver"
        PLUGIN_MANUFACTURER_CODE Nzle
        PLUGIN_CODE Nrcv
        FORMATS ${JUCE_NOZZLE_PLUGIN_FORMATS}
        PRODUCT_NAME "Nozzle Receiver"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )

    juce_generate_juce_header(NozzleReceiverPlugin)

    target_sources(NozzleReceiverPlugin PRIVATE
        examples/NozzleReceiverPlugin/PluginEditor.cpp
        examples/NozzleReceiverPlugin/PluginProcessor.cpp
    )
    target_compile_definitions(NozzleReceiverPlugin PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
    target_link_libraries(NozzleReceiverPlugin PRIVATE
        juce_nozzle
        juce::juce_audio_utils
        juce::juce_gui_extra
    )
    target_compile_options(NozzleReceiverPlugin PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
endif()
