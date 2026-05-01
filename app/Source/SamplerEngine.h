#pragma once
#include "JuceHeader.h"
#include "PolyVoice.h"
#include <vector>

/*  6-voice polyphonic sampler. Each voice owns its own clip and plays
    independently. The "active voice" is the one the UI is currently editing
    (markers, pitch fader, waveform reflect it).

    Compatibility: setLoopStart/End/Speed/etc. and the transient API delegate
    to the active voice so the rest of the app keeps using SamplerEngine like
    a single sampler. */
class SamplerEngine : public juce::AudioSource
{
public:
    static constexpr int kNumVoices = 6;

    SamplerEngine();
    ~SamplerEngine() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

    /** Loads a clip into the active voice (decodes + caches in app data dir). */
    bool loadFile (const juce::File& sourceFile);

    /** True iff active voice has a clip. */
    bool   hasFile() const                      { return voices[activeVoice].hasFile(); }
    double getLengthSeconds() const             { return voices[activeVoice].getLengthSeconds(); }

    bool snapshotForExport (juce::AudioBuffer<float>& outBuffer, double& outSr) const
    {
        return voices[activeVoice].snapshotForExport (outBuffer, outSr);
    }

    // ─── voice access ────────────────────────────────────────────────────────
    PolyVoice& voice (int i)              { return voices[juce::jlimit (0, kNumVoices - 1, i)]; }
    int  getActiveVoice() const           { return activeVoice; }
    void setActiveVoice (int i)           { activeVoice = juce::jlimit (0, kNumVoices - 1, i); }

    void stopAllVoices();
    /** Wipes every voice's buffer/state under the engine lock. */
    void clearAllVoices();

    /** Master pitch multiplier — broadcast to all voices. Default 1.0. */
    void   setMasterSpeed (double m);
    double getMasterSpeed() const { return masterSpeedCache; }

    /** Where each voice's clip is cached (persisted across app restarts). */
    juce::File getCacheDir() const;
    juce::File getCachedClipFile (int voiceIdx) const;

    // ─── delegated to active voice ──────────────────────────────────────────
    void   setLoopStart (double n)        { voices[activeVoice].setLoopStart (n); }
    void   setLoopEnd   (double n)        { voices[activeVoice].setLoopEnd   (n); }
    void   setSpeed     (double s)        { voices[activeVoice].setSpeed     (s); }
    void   setPlaying   (bool p)          { voices[activeVoice].setPlaying   (p); }
    void   jumpToLoopStart()              { voices[activeVoice].jumpToLoopStart(); }
    void   setPlayheadNorm (double n)     { voices[activeVoice].setPlayheadNorm (n); }

    bool   isPlaying() const              { return voices[activeVoice].isPlaying(); }
    double getSpeed() const               { return voices[activeVoice].getSpeed();  }
    double getLoopStartNorm() const       { return voices[activeVoice].getLoopStartNorm(); }
    double getLoopEndNorm() const         { return voices[activeVoice].getLoopEndNorm();   }
    double getPlayheadNorm() const        { return voices[activeVoice].getPlayheadNorm();  }

    std::vector<double> getActiveTransients() const { return voices[activeVoice].getActiveTransients(); }
    void   setTransientLimit (int limit)            { for (auto& v : voices) v.setTransientLimit (limit); }
    int    getTransientLimit() const                { return voices[activeVoice].getTransientLimit(); }
    double snapToNearestTransient (double n, double tolerance = 0.01) const
    { return voices[activeVoice].snapToNearestTransient (n, tolerance); }

    juce::AudioFormatManager& getFormatManager() { return formatManager; }

private:
    juce::AudioFormatManager formatManager;
    double                   deviceSampleRate = 44100.0;

    PolyVoice voices[kNumVoices];
    int       activeVoice = 0;
    double    masterSpeedCache = 1.0;

    mutable juce::CriticalSection engineLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEngine)
};
