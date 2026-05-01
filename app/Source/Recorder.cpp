#include "Recorder.h"

Recorder::Recorder()
{
    writerThread.startThread();
}

Recorder::~Recorder()
{
    stopRecording();
    writerThread.stopThread (4000);
}

void Recorder::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    deviceSampleRate    = device->getCurrentSampleRate();
    deviceInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
}

void Recorder::audioDeviceStopped()
{
    deviceSampleRate    = 0.0;
    deviceInputChannels = 0;
}

bool Recorder::startRecording (const juce::File& outputFile)
{
    if (deviceSampleRate <= 0.0 || deviceInputChannels <= 0)
        return false;

    outputFile.deleteFile();
    outputFile.getParentDirectory().createDirectory();

    std::unique_ptr<juce::FileOutputStream> fos (outputFile.createOutputStream());
    if (fos == nullptr)
        return false;

    juce::WavAudioFormat fmt;
    auto* writer = fmt.createWriterFor (fos.get(),
                                        deviceSampleRate,
                                        (unsigned int) deviceInputChannels,
                                        16, {}, 0);
    if (writer == nullptr)
        return false;

    fos.release(); // ownership moved to writer

    {
        const juce::ScopedLock sl (writerLock);
        threadedWriter.reset (new juce::AudioFormatWriter::ThreadedWriter (writer, writerThread, 32768));
        activeWriter.store (threadedWriter.get());
    }

    currentFile = outputFile;
    startTimeMs = juce::Time::currentTimeMillis();
    return true;
}

juce::File Recorder::stopRecording()
{
    activeWriter.store (nullptr);
    {
        const juce::ScopedLock sl (writerLock);
        threadedWriter.reset();
    }
    auto file = currentFile;
    currentFile = juce::File();
    return file;
}

double Recorder::getElapsedSeconds() const
{
    if (! isRecording()) return 0.0;
    return (double) (juce::Time::currentTimeMillis() - startTimeMs) / 1000.0;
}

void Recorder::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                  int numInputChannels,
                                                  float* const* outputChannelData,
                                                  int numOutputChannels,
                                                  int numSamples,
                                                  const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused (outputChannelData, numOutputChannels, context);

    if (auto* w = activeWriter.load())
        if (numInputChannels > 0 && inputChannelData != nullptr)
            w->write (inputChannelData, numSamples);
}
