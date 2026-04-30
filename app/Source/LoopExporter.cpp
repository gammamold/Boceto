#include "LoopExporter.h"

LoopExporter::LoopExporter() : juce::Thread ("LoopExporter") {}
LoopExporter::~LoopExporter() { stopThread (5000); }

void LoopExporter::exportLoop (Request req, Callback cb)
{
    if (isThreadRunning()) stopThread (5000);
    request  = std::move (req);
    callback = std::move (cb);
    startThread();
}

juce::File LoopExporter::getOutputDir()
{
   #if JUCE_ANDROID
    juce::File downloads ("/storage/emulated/0/Download");
    if (downloads.isDirectory())
        return downloads;
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory);
   #else
    auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    auto dl   = home.getChildFile ("Downloads");
    if (dl.isDirectory()) return dl;
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
   #endif
}

juce::File LoopExporter::makeOutputFile (int padIndex)
{
    auto ts   = juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S");
    auto name = "yt_sampler_pad" + juce::String (padIndex) + "_" + ts + ".wav";
    return getOutputDir().getChildFile (name);
}

void LoopExporter::run()
{
    auto fail = [this] (const juce::String& msg)
    {
        juce::MessageManager::callAsync ([cb = callback, msg]
        { cb (false, {}, msg); });
    };

    auto& src = request.source;
    if (src.getNumSamples() == 0) { fail ("no clip loaded"); return; }

    const int  srcCh   = src.getNumChannels();
    const int  outCh   = juce::jmin (srcCh, 2);
    const int64_t loopLen = request.endSample - request.startSample;
    if (loopLen <= 1) { fail ("empty loop"); return; }

    juce::File outFile = makeOutputFile (request.padIndex);
    outFile.getParentDirectory().createDirectory();

    if (outFile.exists()) outFile.deleteFile();
    auto fos = std::unique_ptr<juce::FileOutputStream> (outFile.createOutputStream());
    if (! fos || ! fos->openedOk())
    {
        fail ("cannot open " + outFile.getFullPathName());
        return;
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (fos.get(),
                             request.sourceSampleRate,
                             (unsigned) outCh,
                             16,
                             {},
                             0));
    if (! writer) { fail ("cannot create writer"); return; }
    fos.release();

    const int blockSize = 4096;
    juce::AudioBuffer<float> outBlock (outCh, blockSize);

    double pos = (double) request.startSample;
    const double endPos = (double) request.endSample;
    const double step   = juce::jlimit (0.001, 16.0, request.speed);
    const int64_t numSamples = src.getNumSamples();

    while (pos < endPos && ! threadShouldExit())
    {
        outBlock.clear();
        int wrote = 0;
        for (int i = 0; i < blockSize && pos < endPos; ++i)
        {
            const int   idx     = (int) pos;
            const float frac    = (float) (pos - idx);
            int         idxNext = idx + 1;
            if (idxNext >= numSamples) idxNext = (int) numSamples - 1;

            for (int ch = 0; ch < outCh; ++ch)
            {
                const int chIdx = juce::jmin (ch, srcCh - 1);
                auto* p = src.getReadPointer (chIdx);
                const float s = p[idx] + frac * (p[idxNext] - p[idx]);
                outBlock.setSample (ch, i, s);
            }

            pos += step;
            ++wrote;
        }

        if (wrote > 0)
            writer->writeFromAudioSampleBuffer (outBlock, 0, wrote);
        if (wrote == 0) break;
    }

    writer.reset();

    juce::MessageManager::callAsync ([cb = callback, outFile]
    { cb (true, outFile, {}); });
}
