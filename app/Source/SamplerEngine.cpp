#include "SamplerEngine.h"

SamplerEngine::SamplerEngine()
{
    formatManager.registerBasicFormats();
}

SamplerEngine::~SamplerEngine() = default;

void SamplerEngine::prepareToPlay (int, double sampleRate)
{
    deviceSampleRate = sampleRate;
    for (auto& v : voices)
        v.prepareForDeviceRate (sampleRate);
}

void SamplerEngine::releaseResources() {}

void SamplerEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();
    const juce::ScopedLock sl (engineLock);
    for (auto& v : voices)
        v.renderInto (*info.buffer, info.startSample, info.numSamples);
}

void SamplerEngine::stopAllVoices()
{
    for (auto& v : voices) v.setPlaying (false);
}

void SamplerEngine::setMasterSpeed (double m)
{
    masterSpeedCache = juce::jlimit (0.25, 4.0, m);
    for (auto& v : voices) v.setMasterSpeed (masterSpeedCache);
}

void SamplerEngine::clearAllVoices()
{
    const juce::ScopedLock sl (engineLock);
    for (auto& v : voices)
    {
        v.setPlaying (false);
        v.loadFromBuffer (juce::AudioBuffer<float> (0, 0), 44100.0, {});
        v.prepareForDeviceRate (deviceSampleRate);
    }
    activeVoice = 0;
}

juce::File SamplerEngine::getCacheDir() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              .getChildFile ("Boceto");
}

juce::File SamplerEngine::getCachedClipFile (int voiceIdx) const
{
    return getCacheDir().getChildFile ("voice" + juce::String (voiceIdx) + ".wav");
}

bool SamplerEngine::loadFile (const juce::File& sourceFile)
{
    if (! sourceFile.existsAsFile()) return false;

    auto target = getCachedClipFile (activeVoice);
    if (sourceFile.getFullPathName() != target.getFullPathName())
    {
        target.getParentDirectory().createDirectory();
        target.deleteFile();
        if (! sourceFile.copyFileTo (target)) return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (target));
    if (reader == nullptr) return false;

    juce::AudioBuffer<float> tmp ((int) reader->numChannels,
                                  (int) reader->lengthInSamples);
    reader->read (&tmp, 0, (int) reader->lengthInSamples, 0, true, true);
    const double sr = reader->sampleRate;

    {
        const juce::ScopedLock sl (engineLock);
        voices[activeVoice].loadFromBuffer (std::move (tmp), sr, target.getFullPathName());
        voices[activeVoice].prepareForDeviceRate (deviceSampleRate);
    }
    return true;
}
