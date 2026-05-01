#pragma once
#include "JuceHeader.h"
#include "SamplerEngine.h"
#include <atomic>

/*  Post-mix effects chain. AudioSource wrapper around SamplerEngine.

    Pipeline:  sampler → filter → delay → chorus → out

    All processors fully bypassable when their wet mix / position is
    near zero — no extra CPU cost when an effect isn't engaged. */
class EffectsChain : public juce::AudioSource
{
public:
    explicit EffectsChain (SamplerEngine& source);

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

    // ─── filter ──────────────────────────────────────────────────────────────
    /** -1..+1; 0 (± 0.04) = bypass, neg = LP, pos = HP. */
    void   setFilterPosition (double pos);
    double getFilterPosition() const            { return filterPos.load(); }
    juce::String getFilterDescription() const;

    // ─── delay ───────────────────────────────────────────────────────────────
    /** Delay time in milliseconds, 10..1500. */
    void   setDelayTimeMs (double ms);
    double getDelayTimeMs() const               { return delayMs.load(); }

    /** Feedback amount 0..0.92. */
    void   setDelayFeedback (double f);
    double getDelayFeedback() const             { return delayFb.load(); }

    /** Wet mix 0..1; 0 = bypass. */
    void   setDelayMix (double m);
    double getDelayMix() const                  { return delayMixAmt.load(); }

    // ─── chorus ──────────────────────────────────────────────────────────────
    /** LFO rate 0.1..5 Hz. */
    void   setChorusRate (double hz);
    double getChorusRate() const                { return chorusRate.load(); }

    /** Depth 0..1. */
    void   setChorusDepth (double d);
    double getChorusDepth() const               { return chorusDepth.load(); }

    /** Wet mix 0..1; 0 = bypass. */
    void   setChorusMix (double m);
    double getChorusMix() const                 { return chorusMixAmt.load(); }

private:
    SamplerEngine& sampler;

    // Filter
    juce::dsp::StateVariableTPTFilter<float> filter;
    std::atomic<double>                      filterPos { 0.0 };

    // Delay (per-channel, supports up to 2 seconds)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 96000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 96000 };
    std::atomic<double> delayMs     { 250.0 };
    std::atomic<double> delayFb     { 0.4 };
    std::atomic<double> delayMixAmt { 0.0 };

    // Chorus
    juce::dsp::Chorus<float>            chorus;
    std::atomic<double> chorusRate    { 0.8 };
    std::atomic<double> chorusDepth   { 0.3 };
    std::atomic<double> chorusMixAmt  { 0.0 };

    bool   prepared = false;
    double currentSampleRate = 44100.0;

    static constexpr double bypassThreshold = 0.04;

    void applyFilterPositionToFilter();
    void applyChorusParams();
    void processFilter (juce::dsp::AudioBlock<float>& block);
    void processDelay  (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsChain)
};
