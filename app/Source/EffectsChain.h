#pragma once
#include "JuceHeader.h"
#include "SamplerEngine.h"
#include <atomic>

/*  Post-mix effects chain. AudioSource wrapper: pulls from SamplerEngine,
    runs the buffer through DSP processors, hands it to the device player.

    v1 effect: DJ-style filter (StateVariableTPTFilter)
      * pos in -1..+1
      * |pos| < 0.04 → bypass (no processing)
      * pos < 0 → low-pass; cutoff sweeps from 20 kHz down to 80 Hz
      * pos > 0 → high-pass; cutoff sweeps from 80 Hz up to 20 kHz
    Resonance fixed at 0.7 (Q ≈ Butterworth).

    Future processors plug in alongside the filter via additional members. */
class EffectsChain : public juce::AudioSource
{
public:
    explicit EffectsChain (SamplerEngine& source);

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

    /** -1..+1; sets filter type and cutoff. 0 (± 0.04) = bypass. */
    void   setFilterPosition (double pos);
    double getFilterPosition() const { return filterPos.load(); }

    /** Human-readable description of the current filter state, for UI labels. */
    juce::String getFilterDescription() const;

private:
    SamplerEngine& sampler;

    juce::dsp::StateVariableTPTFilter<float> filter;
    std::atomic<double> filterPos { 0.0 };

    bool   prepared = false;
    double currentSampleRate = 44100.0;

    static constexpr double bypassThreshold = 0.04;

    void applyFilterPositionToFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsChain)
};
