cmake_minimum_required (VERSION 3.22)

include (cmake/Version.cmake)

project (GRAPHITE VERSION ${GRAPHITE_VERSION} LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

option (GRAPHITE_BUILD_TESTS "Build the Catch2 unit tests" ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set (CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif ()

include (cmake/Dependencies.cmake)

# ---------------------------------------------------------------------------
# Source lists
#
# Core + DSP are deliberately free of any GUI dependency so the test binary can
# link them against juce_dsp alone and run headless in CI.
# ---------------------------------------------------------------------------
set (GRAPHITE_DSP_SOURCES
    Source/Core/CurveModel.cpp
    Source/Core/PresetBank.cpp
    Source/DSP/Cepstrum.cpp
    Source/DSP/IRBuilder.cpp
    Source/DSP/ConvolutionEngine.cpp
    Source/DSP/BiquadCascade.cpp
    Source/DSP/CurveFitter.cpp
    Source/DSP/BandMatcher.cpp
    Source/DSP/Analyzer.cpp
    Source/DSP/CurveWorker.cpp)

# Anything that needs juce_audio_processors or a GUI module lives here, so the
# headless test binary never has to link them.
set (GRAPHITE_PLUGIN_SOURCES
    Source/Core/Params.cpp
    Source/UI/Theme.cpp
    Source/UI/CurveCanvas.cpp
    Source/UI/CanvasLayers/GraticuleLayer.cpp
    Source/UI/CanvasLayers/AnalyzerLayer.cpp
    Source/UI/CanvasLayers/CurveLayer.cpp
    Source/UI/CanvasLayers/BandTokenLayer.cpp
    Source/UI/CanvasLayers/CursorLayer.cpp
    Source/UI/Tools/DrawTools.cpp
    Source/UI/Controls/Knob.cpp
    Source/UI/Controls/Toggle.cpp
    Source/UI/Controls/SlotStrip.cpp
    Source/UI/Editor.cpp
    Source/Processor.cpp)

# ---------------------------------------------------------------------------
# Embedded resources (fonts are SIL OFL; see Resources/Fonts/LICENSE)
# ---------------------------------------------------------------------------
set (GRAPHITE_FONT_FILES
    Resources/Fonts/SpaceGrotesk-Medium.ttf
    Resources/Fonts/JetBrainsMono-Regular.ttf)

set (GRAPHITE_FONTS_PRESENT TRUE)
foreach (f ${GRAPHITE_FONT_FILES})
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        set (GRAPHITE_FONTS_PRESENT FALSE)
    endif ()
endforeach ()

if (GRAPHITE_FONTS_PRESENT)
    juce_add_binary_data (GraphiteResources SOURCES ${GRAPHITE_FONT_FILES})
else ()
    message (WARNING "Bundled fonts missing - falling back to system faces. "
                     "Run Tools/fetch_fonts.sh to restore them.")
    add_library (GraphiteResources INTERFACE)
endif ()

# ---------------------------------------------------------------------------
# The plugin
# ---------------------------------------------------------------------------
juce_add_plugin (Graphite
    PRODUCT_NAME                GRAPHITE
    COMPANY_NAME                "${GRAPHITE_COMPANY}"
    BUNDLE_ID                   ${GRAPHITE_BUNDLE_ID}
    PLUGIN_MANUFACTURER_CODE    ${GRAPHITE_COMPANY_CODE}
    PLUGIN_CODE                 ${GRAPHITE_PLUGIN_CODE}
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Fx EQ
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources (Graphite PRIVATE ${GRAPHITE_DSP_SOURCES} ${GRAPHITE_PLUGIN_SOURCES})

target_compile_definitions (Graphite PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_REPORT_APP_USAGE=0)

target_include_directories (Graphite PRIVATE Source)

target_link_libraries (Graphite
    PRIVATE
        GraphiteResources
        graphite_eigen
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (GRAPHITE_FONTS_PRESENT)
    target_compile_definitions (Graphite PRIVATE GRAPHITE_HAS_EMBEDDED_FONTS=1)
else ()
    target_compile_definitions (Graphite PRIVATE GRAPHITE_HAS_EMBEDDED_FONTS=0)
endif ()

# ---------------------------------------------------------------------------
# Tools - offline utilities. Not shipped, not linked into the plugin.
# ---------------------------------------------------------------------------
juce_add_console_app (GraphiteUISnapshot)

target_sources (GraphiteUISnapshot PRIVATE
    Tools/RenderUI.cpp
    ${GRAPHITE_DSP_SOURCES}
    ${GRAPHITE_PLUGIN_SOURCES})

target_compile_definitions (GraphiteUISnapshot PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_REPORT_APP_USAGE=0
    GRAPHITE_HAS_EMBEDDED_FONTS=$<IF:$<BOOL:${GRAPHITE_FONTS_PRESENT}>,1,0>)

target_include_directories (GraphiteUISnapshot PRIVATE Source)

target_link_libraries (GraphiteUISnapshot PRIVATE
    GraphiteResources
    graphite_eigen
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_console_app (GraphiteFitBench)

target_sources (GraphiteFitBench PRIVATE Tools/FitBench.cpp ${GRAPHITE_DSP_SOURCES})

target_compile_definitions (GraphiteFitBench PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_REPORT_APP_USAGE=0)

target_include_directories (GraphiteFitBench PRIVATE Source)

target_link_libraries (GraphiteFitBench PRIVATE
    graphite_eigen
    juce::juce_dsp
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests - headless, no GUI modules, so they run in CI without an X server.
# ---------------------------------------------------------------------------
if (GRAPHITE_BUILD_TESTS)
    enable_testing ()

    juce_add_console_app (GraphiteTests)

    target_sources (GraphiteTests PRIVATE
        ${GRAPHITE_DSP_SOURCES}
        Tests/TestLogGrid.cpp
        Tests/TestCurveModel.cpp
        Tests/TestIRBuilder.cpp
        Tests/TestConvolution.cpp
        Tests/TestBiquadCascade.cpp
        Tests/TestCurveFitter.cpp
        Tests/TestBandMatcher.cpp
        Tests/TestStateRing.cpp
        Tests/TestNull.cpp
        Tests/TestSweeps.cpp
        Tests/TestRealtimeSafety.cpp)

    target_compile_definitions (GraphiteTests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_REPORT_APP_USAGE=0
        GRAPHITE_UNIT_TEST=1)

    target_include_directories (GraphiteTests PRIVATE Source)

    target_link_libraries (GraphiteTests PRIVATE
        graphite_eigen
        Catch2::Catch2WithMain
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    include (CTest)
    add_test (NAME graphite_unit COMMAND GraphiteTests)
endif ()
