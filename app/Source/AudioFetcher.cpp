#include "AudioFetcher.h"

AudioFetcher::AudioFetcher() : juce::Thread ("AudioFetcher") {}

AudioFetcher::~AudioFetcher() { stopThread (3000); }

void AudioFetcher::fetch (const juce::String& youTubeUrl,
                          const juce::String& serverBase,
                          Callback cb)
{
    if (isThreadRunning())
        stopThread (3000);

    url      = youTubeUrl;
    server   = serverBase;
    callback = std::move (cb);
    startThread();
}

void AudioFetcher::run()
{
    auto trimmed = server.trimCharactersAtEnd ("/");
    auto endpoint = juce::URL (trimmed + "/fetch");

    auto* body = new juce::DynamicObject();
    body->setProperty ("url", url);
    body->setProperty ("sample_rate", 44100);
    juce::var bodyVar (body);
    auto json = juce::JSON::toString (bodyVar);

    endpoint = endpoint.withPOSTData (json);

    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders ("Content-Type: application/json")
                    .withConnectionTimeoutMs (20000);

    std::unique_ptr<juce::InputStream> stream (endpoint.createInputStream (opts));

    if (stream == nullptr)
    {
        juce::MessageManager::callAsync ([cb = callback]
        { cb (false, {}, "Could not connect to server"); });
        return;
    }

    auto out = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("yt_sampler_"
                                  + juce::String (juce::Time::currentTimeMillis())
                                  + ".wav");

    juce::FileOutputStream fos (out);
    if (! fos.openedOk())
    {
        juce::MessageManager::callAsync ([cb = callback]
        { cb (false, {}, "Cannot write temp file"); });
        return;
    }

    fos.writeFromInputStream (*stream, -1);
    fos.flush();

    juce::MessageManager::callAsync ([cb = callback, out]
    { cb (true, out, {}); });
}
