#pragma once
#include "JuceHeader.h"
#include "SamplerEngine.h"
#include "AudioFetcher.h"
#include "WaveformDisplay.h"
#include "PitchFader.h"
#include "LoopExporter.h"
#include "Recorder.h"

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kNumPads = 6;

    struct LoopState
    {
        bool         occupied  = false;
        juce::String clipPath;            // path to this pad's source clip
        double       loopStart = 0.0;
        double       loopEnd   = 1.0;
        double       speed     = 1.0;
    };

    void timerCallback() override;

    void onFetchClicked();
    void onPlayClicked();
    void onStopClicked();
    void onSetInClicked();
    void onSetOutClicked();
    void onSaveClicked();
    void onResetClicked();
    void onRangeClicked();
    void onExportClicked();
    void onSpreadClicked();
    void spreadToAllPads (bool useTransients);
    void onRecClicked();
    void startRecordingFlow();
    void stopRecordingFlow();
    void onNewProjectClicked();
    void clearAllPadsAndState();
    bool saveProjectToDisk();
    void markDirty();
    void onPadClicked (int padIndex);

    void recallPad (int padIndex);
    void capturePad (int padIndex);
    void selectActivePad (int padIndex);
    void refreshPadButtons();
    void setStatus (const juce::String&);

    void persistState();
    void restoreState();
    juce::File getStateFile() const;

    // Inputs
    juce::TextEditor urlField;
    juce::TextEditor serverField;
    juce::TextButton pasteBtn    { "PASTE" };
    juce::TextButton loadLocalBtn{ "LOAD" };
    juce::TextButton clearUrlBtn { "X" };
    juce::TextButton newBtn      { "NEW" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    void onLoadLocalClicked();
    void loadFromAnyFile (const juce::File& f);

    // Waveform + pitch
    WaveformDisplay waveform;
    PitchFader      pitch;

    // Calculator grid
    juce::TextButton fetchBtn  { "FETCH" };
    juce::TextButton recBtn    { "REC" };
    juce::TextButton playBtn   { "PLAY" };
    juce::TextButton stopBtn   { "STOP" };
    juce::TextButton exportBtn { "EXPORT" };

    juce::TextButton setInBtn  { "SET-IN" };
    juce::TextButton setOutBtn { "SET-OUT" };
    juce::TextButton saveBtn   { "SAVE" };
    juce::TextButton resetBtn  { "RESET" };

    juce::TextButton padBtns[kNumPads];
    juce::TextButton rangeBtn  { "RANGE" };
    juce::TextButton spreadBtn { "SPREAD" };

    juce::TextButton zoomOutBtn  { "Z-" };
    juce::TextButton zoomInBtn   { "Z+" };
    juce::TextButton zoomResetBtn{ "1:1" };
    juce::TextButton transientBtn{ "T:ALL" };

    juce::Label statusLabel;

    // State
    LoopState pads[kNumPads];
    int       transientCycleIdx = 5; // -1 default = ALL = idx 5
    void      cycleTransientLimit();

    // Audio
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer  audioSourcePlayer;
    SamplerEngine            sampler;
    AudioFetcher             fetcher;
    LoopExporter             exporter;
    Recorder                 recorder;
    bool                     inputDeviceOpened = false;
    bool                     loopsAutoStoppedForRec = false;

    // Project state
    bool                     projectDirty = false;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
