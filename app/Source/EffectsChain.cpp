#include "EffectsChain.h"
#include <cmath>

EffectsChain::EffectsChain (SamplerEngine& source)
    : sampler (source)
{
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setResonance (0.7f);
    filter.setCutoffFrequency (20000.0f);
}

void EffectsChain::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    sampler.prepareToPlay (samplesPerBlockExpected, sampleRate);
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) samplesPerBlockExpected,
                                  2 };
    filter.prepare (spec);
    filter.reset();
    prepared = true;
    applyFilterPositionToFilter();
}

void EffectsChain::releaseResources()
{
    sampler.releaseResources();
    prepared = false;
}

juce::String EffectsChain::getFilterDescription() const
{
    const double pos = filterPos.load();
    if (std::abs (pos) < bypassThreshold) return "BYPASS";

    if (pos < 0.0)
    {
        const double t = (-pos - bypassThreshold) / (1.0 - bypassThreshold);
        const double hz = 20000.0 * std::pow (80.0 / 20000.0, t);
        return "LP " + juce::String ((int) hz) + " Hz";
    }

    const double t = (pos - bypassThreshold) / (1.0 - bypassThreshold);
    const double hz = 80.0 * std::pow (20000.0 / 80.0, t);
    return "HP " + juce::String ((int) hz) + " Hz";
}

void EffectsChain::setFilterPosition (double pos)
{
    pos = juce::jlimit (-1.0, 1.0, pos);
    filterPos.store (pos);
    if (prepared)
        applyFilterPositionToFilter();
}

void EffectsChain::applyFilterPositionToFilter()
{
    const double pos = filterPos.load();
    if (std::abs (pos) < bypassThreshold)
        return; // skip — getNextAudioBlock will bypass anyway

    if (pos < 0.0)
    {
        const double t = (-pos - bypassThreshold) / (1.0 - bypassThreshold);
        const double hz = 20000.0 * std::pow (80.0 / 20000.0, t);
        filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        filter.setCutoffFrequency ((float) hz);
    }
    else
    {
        const double t = (pos - bypassThreshold) / (1.0 - bypassThreshold);
        const double hz = 80.0 * std::pow (20000.0 / 80.0, t);
        filter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        filter.setCutoffFrequency ((float) hz);
    }
}

void EffectsChain::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    sampler.getNextAudioBlock (info);

    if (! prepared) return;
    if (std::abs (filterPos.load()) < bypassThreshold) return;

    juce::dsp::AudioBlock<float> block (*info.buffer);
    auto sub = block.getSubBlock ((size_t) info.startSample,
                                  (size_t) info.numSamples);
    juce::dsp::ProcessContextReplacing<float> ctx (sub);
    filter.process (ctx);
}
