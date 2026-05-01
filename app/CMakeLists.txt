cmake_minimum_required(VERSION 3.22)
project(YouTubeSampler VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE -------------------------------------------------------------------
# Either drop JUCE in ./JUCE  OR let CMake fetch it:
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- App --------------------------------------------------------------------
juce_add_gui_app(YouTubeSampler
    PRODUCT_NAME              "YouTube Sampler"
    BUNDLE_ID                 com.gammamold.youtubesampler
    VERSION                   ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    # iOS
    IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortrait
    IPAD_SCREEN_ORIENTATIONS   UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortrait
    BACKGROUND_AUDIO_ENABLED  FALSE
)

juce_generate_juce_header(YouTubeSampler)

target_sources(YouTubeSampler PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/SamplerEngine.cpp
    Source/PolyVoice.cpp
    Source/AudioFetcher.cpp
    Source/LoopExporter.cpp
    Source/Recorder.cpp
    Source/EffectsChain.cpp
)

target_compile_definitions(YouTubeSampler PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:YouTubeSampler,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:YouTubeSampler,JUCE_VERSION>"
)

target_link_libraries(YouTubeSampler PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
