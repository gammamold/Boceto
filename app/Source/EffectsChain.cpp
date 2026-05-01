#include "EffectsChain.h"
#include <cmath>

EffectsChain::EffectsChain (SamplerEngine& source)
    : sampler (source)
{
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setResonance (0.7f);
    filter.setCutoffFrequency (20000.0f);

    chorus.setRate     ((float) chorusRate.load());
    chorus.setDepth    ((float) chorusDepth.load());
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (0.0f);
    chorus.setMix      (0.0f);
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

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    delayL.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayR.setMaximumDelayInSamples ((int) (sampleRate * 2.0));
    delayL.prepare (monoSpec);
    delayR.prepare (monoSpec);
    delayL.reset();
    delayR.reset();

    chorus.prepare (spec);
    chorus.reset();

    prepared = true;
    applyFilterPositionToFilter();
    applyChorusParams();
}

void EffectsChain::releaseResources()
{
    sampler.releaseResources();
    prepared = false;
}

// ──────────────────────────── filter ──────────────────────────────────────────

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
        return;

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

void EffectsChain::processFilter (juce::dsp::AudioBlock<float>& block)
{
    if (std::abs (filterPos.load()) < bypassThreshold) return;
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    filter.process (ctx);
}

// ──────────────────────────── delay ───────────────────────────────────────────

void EffectsChain::setDelayTimeMs (double ms)  { delayMs    .store (juce::jlimit (10.0,  1500.0, ms)); }
void EffectsChain::setDelayFeedback (double f) { delayFb    .store (juce::jlimit (0.0,   0.92,  f )); }
void EffectsChain::setDelayMix (double m)      { delayMixAmt.store (juce::jlimit (0.0,   1.0,   m )); }

void EffectsChain::processDelay (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const float mix = (float) delayMixAmt.load();
    if (mix < (float) bypassThreshold) return;

    const float fb       = (float) delayFb.load();
    const float dSamples = (float) (delayMs.load() * 0.001 * currentSampleRate);
    delayL.setDelay (dSamples);
    delayR.setDelay (dSamples);

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    for (int n = 0; n < numSamples; ++n)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& dl = (ch == 0) ? delayL : delayR;
            float* p = buffer.getWritePointer (ch);
            const float dry = p[startSample + n];
            const float wet = dl.popSample (0);
            dl.pushSample (0, dry + wet * fb);
            p[startSample + n] = dry * (1.0f - mix) + wet * mix;
        }
    }
}

// ──────────────────────────── chorus ──────────────────────────────────────────

void EffectsChain::setChorusRate (double hz)
{
    chorusRate.store (juce::jlimit (0.1, 5.0, hz));
    if (prepared) chorus.setRate ((float) chorusRate.load());
}

void EffectsChain::setChorusDepth (double d)
{
    chorusDepth.store (juce::jlimit (0.0, 1.0, d));
    if (prepared) chorus.setDepth ((float) chorusDepth.load());
}

void EffectsChain::setChorusMix (double m)
{
    chorusMixAmt.store (juce::jlimit (0.0, 1.0, m));
    if (prepared) chorus.setMix ((float) chorusMixAmt.load());
}

void EffectsChain::applyChorusParams()
{
    chorus.setRate  ((float) chorusRate.load());
    chorus.setDepth ((float) chorusDepth.load());
    chorus.setMix   ((float) chorusMixAmt.load());
}

// ──────────────────────────── audio block ─────────────────────────────────────

void EffectsChain::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    sampler.getNextAudioBlock (info);
    if (! prepared) return;

    juce::dsp::AudioBlock<float> block (*info.buffer);
    auto sub = block.getSubBlock ((size_t) info.startSample,
                                  (size_t) info.numSamples);

    processFilter (sub);

    processDelay (*info.buffer, info.startSample, info.numSamples);

    if (chorusMixAmt.load() >= bypassThreshold)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        chorus.process (ctx);
    }
}
