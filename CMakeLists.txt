# ==============================================================================
# TempoGate - CMake build (JUCE 9)
#
# Configure & build (native arch):
#   cmake -S /Users/user/Desktop/tempogate/TempoGate \
#         -B /Users/user/Desktop/tempogate/TempoGate/Builds/CMake \
#         -DCMAKE_BUILD_TYPE=Release
#   cmake --build /Users/user/Desktop/tempogate/TempoGate/Builds/CMake \
#         --config Release -j
#
# JUCE location: $ENV{HOME}/JUCE by default, override with -DJUCE_DIR=...
# ==============================================================================

cmake_minimum_required (VERSION 3.22)

project (TempoGate VERSION 1.0.1 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif ()

# MSVC interprets sources in the system code page unless told otherwise;
# enforce UTF-8 so string literals and comments behave identically everywhere.
add_compile_options ($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

set (JUCE_DIR "$ENV{HOME}/JUCE" CACHE PATH "JUCE framework source root")
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message (FATAL_ERROR "JUCE not found at ${JUCE_DIR}. Pass -DJUCE_DIR=<path-to-JUCE>.")
endif ()

add_subdirectory ("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
# Plugin formats per platform. AU exists only on Apple platforms; Windows gets
# VST3 plus Standalone for DAW-less smoke testing.
if (APPLE)
    set (TEMPOGATE_PLUGIN_FORMATS AU VST3 Standalone)
else ()
    set (TEMPOGATE_PLUGIN_FORMATS VST3 Standalone)
endif ()

# Copying the VST3 into the system plug-in folder needs elevation on Windows,
# so it is opt-out-able (pass -DTEMPOGATE_COPY_PLUGIN_AFTER_BUILD=OFF).
option (TEMPOGATE_COPY_PLUGIN_AFTER_BUILD "Copy plug-ins to the system folder after build" ON)

# Diagnostic take logging: per-block clock state + per-event beats appended to
# <tempdir>/tempogate_take.log while recording. OFF by default (zero overhead
# and zero behavior change when off). Build a separate tree with
# -DTEMPOGATE_TAKE_LOG=ON for input-timing forensics; never ship it enabled
# (file I/O on the audio thread can perturb timing under load).
option (TEMPOGATE_TAKE_LOG "Compile in per-block/per-event take logging (diagnostic builds only)" OFF)
if (TEMPOGATE_TAKE_LOG)
    add_compile_definitions (TEMPOGATE_TAKE_LOG=1)
endif ()

juce_add_plugin (TempoGate
    COMPANY_NAME "kh"
    BUNDLE_ID kr.me.kh.TempoGate
    PLUGIN_MANUFACTURER_CODE Rzgl
    PLUGIN_CODE Gzlb
    VERSION ${PROJECT_VERSION}

    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    FORMATS ${TEMPOGATE_PLUGIN_FORMATS}
    PRODUCT_NAME "TempoGate"

    # NOTE (Cubase): IS_SYNTH makes JUCE emit the VST3 "Instrument"
    # sub-category (result: "Instrument|Tools"), so Cubase lists TempoGate as
    # a VSTi and routes MIDI input to it on an instrument track. AU keeps the
    # MIDI-processor type (IS_MIDI_EFFECT wins there), so the Logic MIDI FX
    # slot keeps working.
    VST3_CATEGORIES Tools
    # AU type intentionally unset: JUCE 9 defaults MIDI effects to
    # kAudioUnitType_MIDIProcessor ('aumf', Logic MIDI FX).

    COPY_PLUGIN_AFTER_BUILD ${TEMPOGATE_COPY_PLUGIN_AFTER_BUILD}
)

# Back-compat for sources including <JuceHeader.h> (Projucer-style).
juce_generate_juce_header (TempoGate)

target_sources (TempoGate PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/TempoEngine.h
    Source/BarGate.h
    Source/PerformanceModel.h
    Source/MidiExport.h
)

target_compile_definitions (TempoGate
    PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries (TempoGate
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ------------------------------------------------------------------------------
# Headless smoke tests: drive the processor's processBlock directly.
option (TEMPOGATE_BUILD_TESTS "Build TempoGate smoke tests" ON)
if (TEMPOGATE_BUILD_TESTS)
    juce_add_console_app (TempoGateTest VERSION 1.0.0)
    juce_generate_juce_header (TempoGateTest)

    target_sources (TempoGateTest PRIVATE
        tests/MetronomeTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
    )

    target_include_directories (TempoGateTest PRIVATE Source)

    target_compile_definitions (TempoGateTest
        PUBLIC
            JUCE_STRICT_REFCOUNTEDPOINTER=1
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JucePlugin_Name="TempoGate"
            JucePlugin_WantsMidiInput=1
            JucePlugin_ProducesMidiOutput=1
            JucePlugin_IsMidiEffect=1
    )

    target_link_libraries (TempoGateTest
        PRIVATE
            juce::juce_audio_basics
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_core
            juce::juce_data_structures
            juce::juce_events
            juce::juce_graphics
            juce::juce_gui_basics
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endif ()
