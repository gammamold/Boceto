#pragma once
#include "JuceHeader.h"
#include <atomic>
#include <vector>

/*  One polyphonic voice. Each voice owns its own audio clip (buffer + sample
    rate + file path + transient analysis), so pads can hold different sources.

    Synchronisation: buffer mutation (loadFromBuffer) and reads (renderInto)
    are protected externally by SamplerEngine's lock. Atomic state for play /
    speed / loop range / readPos. */
class PolyVoice
{
public:
    PolyVoice() = default;

    void prepareForDeviceRate (double deviceSr) { deviceSampleRate = deviceSr; }

    /** Move-in a fully-decoded clip + its source path. Re-runs transient
        analysis. Caller must hold SamplerEngine's lock. */
    void loadFromBuffer (juce::AudioBuffer<float>&& newBuffer,
                         double sourceSampleRate,
                         juce::String sourcePath);

    /** True if this voice has audio loaded. */
    bool   hasFile() const                { return bufferNumSamples > 0; }
    double getLengthSeconds() const;
    juce::String getFilePath() const      { return filePath; }

    /** Loop coords in 0..1 of the buffer length. */
    void   setLoopStart (double normalised);
    void   setLoopEnd   (double normalised);
    double getLoopStartNorm() const;
    double getLoopEndNorm()   const;

    void   setSpeed (double s)            { speed.store (juce::jlimit (0.25, 4.0, s)); }
    double getSpeed() const               { return speed.load(); }

    /** Master multiplier applied on top of per-voice speed. Engine broadcasts. */
    void   setMasterSpeed (double m)      { masterSpeed.store (juce::jlimit (0.25, 4.0, m)); }

    void   setPlaying (bool p)            { playing.store (p); }
    bool   isPlaying() const              { return playing.load(); }

    void   setPlayheadNorm (double n);
    double getPlayheadNorm() const;
    void   jumpToLoopStart();

    /** Add this voice's output to dest. Caller holds engine lock. */
    void renderInto (juce::AudioBuffer<float>& dest, int startSample, int numSamples);

    /** Copy the buffer + sr for offline render. */
    bool snapshotForExport (juce::AudioBuffer<float>& outBuffer, double& outSr) const;

    // ─── transient analysis (per-voice, per-clip) ────────────────────────────
    std::vector<double> getActiveTransients() const;
    void   setTransientLimit (int limit)  { transientLimit = limit; }
    int    getTransientLimit() const      { return transientLimit; }
    double snapToNearestTransient (double n, double tolerance = 0.01) const;

private:
    void analyseTransients();

    juce::AudioBuffer<float> buffer;
    int                      bufferNumSamples = 0;
    double                   bufferSampleRate = 44100.0;
    double                   deviceSampleRate = 44100.0;
    juce::String             filePath;

    std::atomic<bool>        playing { false };
    std::atomic<double>      speed   { 1.0 };
    std::atomic<double>      masterSpeed { 1.0 };
    std::atomic<int64_t>     loopStartSamples { 0 };
    std::atomic<int64_t>     loopEndSamples   { 0 };
    std::atomic<double>      readPos { 0.0 };

    struct Transient { double position; float strength; };
    std::vector<Transient>   allTransients;
    int                      transientLimit = -1;

    JUCE_DECLARE_NON_COPYABLE (PolyVoice)
};
