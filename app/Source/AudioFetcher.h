#pragma once
#include "JuceHeader.h"
#include <functional>

/*  Background download of a WAV from the yt-dlp HTTP server.
    Calls back on the message thread. */
class AudioFetcher : private juce::Thread
{
public:
    using Callback = std::function<void (bool success, juce::File downloaded, juce::String error)>;

    AudioFetcher();
    ~AudioFetcher() override;

    void fetch (const juce::String& youTubeUrl,
                const juce::String& serverBase,
                Callback cb);

private:
    void run() override;

    juce::String url, server;
    Callback     callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFetcher)
};
