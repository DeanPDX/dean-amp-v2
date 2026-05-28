cmake_minimum_required(VERSION 3.22)
project(DeanAmp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
# Embedded image assets (photoreal amp faceplate render).
# Generates BinaryData accessible as BinaryData::amp_face_png / _pngSize.
# ----------------------------------------------------------------------------
juce_add_binary_data(DeanAmpData
    SOURCES
        resources/amp_face.png)

# Position-independent so it can link into the plugin's shared module.
set_target_properties(DeanAmpData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

juce_add_plugin(DeanAmp
    PLUGIN_MANUFACTURER_CODE Dean
    PLUGIN_CODE              Damp
    FORMATS                  AU VST3 Standalone
    PRODUCT_NAME             "Dean Amp"
    COMPANY_NAME             "Dean Audio"
    BUNDLE_ID                com.deanaudio.deanamp
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    VST3_CATEGORIES          "Fx|Distortion"
    AU_MAIN_TYPE             "kAudioUnitType_Effect"
)

juce_generate_juce_header(DeanAmp)

target_sources(DeanAmp PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/dsp/TubeStage.cpp
    source/dsp/ToneStack.cpp
    source/dsp/PowerAmp.cpp
    source/dsp/NoiseGate.cpp
    source/dsp/CabSim.cpp
    source/ui/DeanLookAndFeel.cpp
    source/ui/RotaryKnob.cpp
    source/ui/LevelMeter.cpp
    source/ui/ToggleSwitch.cpp
    source/ui/AmpChassis.cpp
)

target_include_directories(DeanAmp PRIVATE source)

target_compile_definitions(DeanAmp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(DeanAmp PRIVATE
    DeanAmpData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_plugin_client
    juce::juce_gui_basics
    juce::juce_graphics
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
# Snapshot tool — renders the editor to a PNG without launching the standalone.
# ----------------------------------------------------------------------------
juce_add_gui_app(DeanAmpSnapshot
    PRODUCT_NAME "Dean Amp Snapshot"
    BUNDLE_ID    com.deanaudio.deanamp.snapshot
)
juce_generate_juce_header(DeanAmpSnapshot)
target_sources(DeanAmpSnapshot PRIVATE
    tools/SnapshotMain.cpp
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/dsp/TubeStage.cpp
    source/dsp/ToneStack.cpp
    source/dsp/PowerAmp.cpp
    source/dsp/NoiseGate.cpp
    source/dsp/CabSim.cpp
    source/ui/DeanLookAndFeel.cpp
    source/ui/RotaryKnob.cpp
    source/ui/LevelMeter.cpp
    source/ui/ToggleSwitch.cpp
    source/ui/AmpChassis.cpp
)
target_include_directories(DeanAmpSnapshot PRIVATE source)
target_compile_definitions(DeanAmpSnapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1
    JucePlugin_Build_Standalone=0
)
target_link_libraries(DeanAmpSnapshot PRIVATE
    DeanAmpData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_gui_basics
    juce::juce_graphics
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
# Tone-test tool — pushes a test signal through the processor and writes WAV.
# ----------------------------------------------------------------------------
juce_add_console_app(DeanAmpToneTest
    PRODUCT_NAME "Dean Amp Tone Test"
    BUNDLE_ID    com.deanaudio.deanamp.tonetest
)
juce_generate_juce_header(DeanAmpToneTest)
target_sources(DeanAmpToneTest PRIVATE
    tools/ToneTestMain.cpp
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/dsp/TubeStage.cpp
    source/dsp/ToneStack.cpp
    source/dsp/PowerAmp.cpp
    source/dsp/NoiseGate.cpp
    source/dsp/CabSim.cpp
    source/ui/DeanLookAndFeel.cpp
    source/ui/RotaryKnob.cpp
    source/ui/LevelMeter.cpp
    source/ui/ToggleSwitch.cpp
    source/ui/AmpChassis.cpp
)
target_include_directories(DeanAmpToneTest PRIVATE source)
target_compile_definitions(DeanAmpToneTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(DeanAmpToneTest PRIVATE
    DeanAmpData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_graphics
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
