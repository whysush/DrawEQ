cmake_minimum_required (VERSION 3.22)

include (cmake/Version.cmake)

project (DrawEQ VERSION ${DRAWEQ_VERSION} LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

option (DRAWEQ_BUILD_TESTS "Build the Catch2 unit tests" ON)

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
set (DRAWEQ_DSP_SOURCES
    Source/Core/CurveModel.cpp
    Source/Core/PresetBank.cpp
    Source/Core/Shapes.cpp
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
set (DRAWEQ_PLUGIN_SOURCES
    Source/Core/Params.cpp
    Source/UI/Theme.cpp
    Source/UI/CurveCanvas.cpp
    Source/UI/CanvasLayers/GraticuleLayer.cpp
    Source/UI/CanvasLayers/AnalyzerLayer.cpp
    Source/UI/CanvasLayers/CurveLayer.cpp
    Source/UI/CanvasLayers/BandTokenLayer.cpp
    Source/UI/CanvasLayers/CursorLayer.cpp
    Source/UI/Tools/DrawTools.cpp
    Source/UI/Controls/LookAndFeel.cpp
    Source/UI/Controls/Fader.cpp
    Source/UI/Controls/Checkbox.cpp
    Source/UI/Controls/Console.cpp
    Source/UI/Controls/ToolButton.cpp
    Source/UI/Controls/Toggle.cpp
    Source/UI/Controls/SlotStrip.cpp
    Source/UI/Editor.cpp
    Source/Processor.cpp)

# ---------------------------------------------------------------------------
# Embedded resources (fonts are SIL OFL; see Resources/Fonts/LICENSE)
# ---------------------------------------------------------------------------
set (DRAWEQ_FONT_FILES
    Resources/Fonts/SpaceGrotesk-Medium.ttf
    Resources/Fonts/JetBrainsMono-Regular.ttf)

set (DRAWEQ_IMAGE_FILES Resources/Images/mascot.png)

set (DRAWEQ_RESOURCES_PRESENT TRUE)
foreach (f ${DRAWEQ_FONT_FILES})
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        set (DRAWEQ_RESOURCES_PRESENT FALSE)
    endif ()
endforeach ()

if (DRAWEQ_RESOURCES_PRESENT)
    juce_add_binary_data (DrawEQResources
        SOURCES ${DRAWEQ_FONT_FILES} ${DRAWEQ_IMAGE_FILES})
else ()
    message (WARNING "Bundled resources missing - falling back to system faces "
                     "and no artwork. Run Tools/fetch_fonts.sh to restore the fonts.")
    add_library (DrawEQResources INTERFACE)
endif ()

# ---------------------------------------------------------------------------
# The plugin
# ---------------------------------------------------------------------------
juce_add_plugin (DrawEQ
    PRODUCT_NAME                DrawEQ
    COMPANY_NAME                "${DRAWEQ_COMPANY}"
    BUNDLE_ID                   ${DRAWEQ_BUNDLE_ID}
    PLUGIN_MANUFACTURER_CODE    ${DRAWEQ_COMPANY_CODE}
    PLUGIN_CODE                 ${DRAWEQ_PLUGIN_CODE}
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Fx EQ
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources (DrawEQ PRIVATE ${DRAWEQ_DSP_SOURCES} ${DRAWEQ_PLUGIN_SOURCES})

target_compile_definitions (DrawEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_REPORT_APP_USAGE=0)

target_include_directories (DrawEQ PRIVATE Source)

target_link_libraries (DrawEQ
    PRIVATE
        DrawEQResources
        draweq_eigen
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (DRAWEQ_RESOURCES_PRESENT)
    target_compile_definitions (DrawEQ PRIVATE DRAWEQ_HAS_RESOURCES=1)
else ()
    target_compile_definitions (DrawEQ PRIVATE DRAWEQ_HAS_RESOURCES=0)
endif ()

# ---------------------------------------------------------------------------
# Tools - offline utilities. Not shipped, not linked into the plugin.
# ---------------------------------------------------------------------------
juce_add_console_app (DrawEQSnapshot)

target_sources (DrawEQSnapshot PRIVATE
    Tools/RenderUI.cpp
    ${DRAWEQ_DSP_SOURCES}
    ${DRAWEQ_PLUGIN_SOURCES})

target_compile_definitions (DrawEQSnapshot PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_REPORT_APP_USAGE=0
    DRAWEQ_HAS_RESOURCES=$<IF:$<BOOL:${DRAWEQ_RESOURCES_PRESENT}>,1,0>)

target_include_directories (DrawEQSnapshot PRIVATE Source)

target_link_libraries (DrawEQSnapshot PRIVATE
    DrawEQResources
    draweq_eigen
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Loads the built VST3 through JUCE's own VST3 host and drives it the way a
# DAW would. Needs the plugin built first.
juce_add_console_app (DrawEQHostSim)

target_sources (DrawEQHostSim PRIVATE Tools/HostSim.cpp)

target_compile_definitions (DrawEQHostSim PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries (DrawEQHostSim PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_console_app (DrawEQFitBench)

target_sources (DrawEQFitBench PRIVATE Tools/FitBench.cpp ${DRAWEQ_DSP_SOURCES})

target_compile_definitions (DrawEQFitBench PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_REPORT_APP_USAGE=0)

target_include_directories (DrawEQFitBench PRIVATE Source)

target_link_libraries (DrawEQFitBench PRIVATE
    draweq_eigen
    juce::juce_dsp
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests - headless, no GUI modules, so they run in CI without an X server.
# ---------------------------------------------------------------------------
if (DRAWEQ_BUILD_TESTS)
    enable_testing ()

    juce_add_console_app (DrawEQTests)

    target_sources (DrawEQTests PRIVATE
        ${DRAWEQ_DSP_SOURCES}
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

    target_compile_definitions (DrawEQTests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_REPORT_APP_USAGE=0
        DRAWEQ_UNIT_TEST=1)

    target_include_directories (DrawEQTests PRIVATE Source)

    target_link_libraries (DrawEQTests PRIVATE
        draweq_eigen
        Catch2::Catch2WithMain
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    include (CTest)
    add_test (NAME draweq_unit COMMAND DrawEQTests)
endif ()
