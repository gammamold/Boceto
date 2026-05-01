#pragma once
#include "JuceHeader.h"
#include <atomic>

/*  AudioIODeviceCallback that captures input samples to a WAV file.
    Add this alongside the AudioSourcePlayer in AudioDeviceManager: it
    only reads input and never touches the output buffer, so playback
    keeps running. */
class Recorder : public juce::AudioIODeviceCallback
{
public:
    Recorder();
    ~Recorder() override;

    /** Open outputFile and start writing input samples. Returns false if
        the device isn't ready (e.g. mic permission not granted yet) or
        the file can't be created. */
    bool startRecording (const juce::File& outputFile);

    /** Finalise the WAV (drains the writer thread). Returns the file path
        that was written, or an empty File if not recording. */
    juce::File stopRecording();

    bool   isRecording()       const noexcept { return activeWriter.load() != nullptr; }
    double getElapsedSeconds() const;

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                            int numInputChannels,
                                            float* const* outputChannelData,
                                            int numOutputChannels,
                                            int numSamples,
                                            const juce::AudioIODeviceCallbackContext& context) override;

private:
    juce::TimeSliceThread writerThread { "Boceto Recorder" };
    juce::CriticalSection writerLock;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*>    activeWriter { nullptr };

    double  deviceSampleRate     = 0.0;
    int     deviceInputChannels  = 0;
    juce::File   currentFile;
    juce::int64  startTimeMs     = 0;

    JUCE_DECLARE_NON_COPYABLE (Recorder)
};
