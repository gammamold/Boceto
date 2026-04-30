#pragma once
#include "JuceHeader.h"

/*  Renders a loop region from a captured AudioBuffer through varispeed and
    writes a 16-bit WAV. "Render-as-heard": output length = (loopLen / speed)
    samples at sourceSampleRate. Runs on a worker thread; callback fires on
    the message thread. */
class LoopExporter : private juce::Thread
{
public:
    struct Request
    {
        juce::AudioBuffer<float> source;
        double  sourceSampleRate = 44100.0;
        int64_t startSample = 0;
        int64_t endSample   = 0;
        double  speed       = 1.0;
        int     padIndex    = 1;
    };

    using Callback = std::function<void (bool ok, juce::File output, juce::String error)>;

    LoopExporter();
    ~LoopExporter() override;

    void exportLoop (Request req, Callback cb);

private:
    void run() override;
    static juce::File getOutputDir();
    static juce::File makeOutputFile (int padIndex);

    Request  request;
    Callback callback;
};
